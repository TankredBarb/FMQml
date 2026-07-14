#include "InstagramInternal.h"
#include "InstagramAuth.h"
#include <algorithm>
#include <limits>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <QUrlQuery>
namespace InstagramProviderInternal {
void addUniqueUrl(QStringList &urls, const QString &url)
{
    QString decoded = htmlDecode(url).trimmed();
    if (decoded.isEmpty()) {
        return;
    }
    decoded.replace(QStringLiteral("\\u002F"), QStringLiteral("/"));
    decoded.replace(QStringLiteral("\\u0025"), QStringLiteral("%"));
    if (!urls.contains(decoded)) {
        urls.append(decoded);
    }
}

QString uniqueMediaName(const InstagramPost &post, const QString &baseName, const QString &suffix)
{
    QString name = QStringLiteral("%1.%2").arg(baseName, suffix);
    for (qsizetype counter = 2; std::any_of(post.items.cbegin(), post.items.cend(), [&name](const InstagramMediaItem &item) {
             return item.name == name;
         }); ++counter) {
        name = QStringLiteral("%1-%2.%3").arg(baseName).arg(counter).arg(suffix);
    }
    return name;
}

QString metaPropertyContent(const QString &html, const QString &propertyName)
{
    const QString escapedProperty = QRegularExpression::escape(propertyName);
    const QRegularExpression propertyThenContent(
        QStringLiteral(R"(<meta\b(?=[^>]*\bproperty=["']%1["'])(?=[^>]*\bcontent=["']([^"']+)["'])[^>]*>)").arg(escapedProperty),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = propertyThenContent.match(html);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    const QRegularExpression nameThenContent(
        QStringLiteral(R"(<meta\b(?=[^>]*\bname=["']%1["'])(?=[^>]*\bcontent=["']([^"']+)["'])[^>]*>)").arg(escapedProperty),
        QRegularExpression::CaseInsensitiveOption);
    match = nameThenContent.match(html);
    return match.hasMatch() ? match.captured(1) : QString{};
}

std::optional<qint64> jsonIntegerValue(const QJsonValue &value)
{
    if (value.isDouble()) {
        return static_cast<qint64>(value.toDouble());
    }
    if (value.isString()) {
        bool ok = false;
        const qint64 parsed = value.toString().toLongLong(&ok);
        if (ok) {
            return parsed;
        }
    }
    return std::nullopt;
}

QDateTime instagramTimestampFromRaw(qint64 raw)
{
    if (raw <= 0) {
        return {};
    }
    if (raw > 10000000000000LL) {
        raw /= 1000000LL;
    } else if (raw > 10000000000LL) {
        raw /= 1000LL;
    }
    return QDateTime::fromSecsSinceEpoch(raw, QTimeZone::UTC);
}

QDateTime mediaTimestampForNode(const QJsonObject &node)
{
    static const QStringList timestampKeys = {
        QStringLiteral("taken_at"),
        QStringLiteral("taken_at_timestamp"),
        QStringLiteral("created_at"),
        QStringLiteral("created_time"),
        QStringLiteral("date")
    };

    for (const QString &key : timestampKeys) {
        const std::optional<qint64> raw = jsonIntegerValue(node.value(key));
        if (!raw.has_value()) {
            continue;
        }
        const QDateTime timestamp = instagramTimestampFromRaw(*raw);
        if (timestamp.isValid()) {
            return timestamp;
        }
    }
    return {};
}

qint64 mediaSizeForObject(const QJsonObject &object)
{
    static const QStringList sizeKeys = {
        QStringLiteral("file_size"),
        QStringLiteral("content_length"),
        QStringLiteral("contentLength"),
        QStringLiteral("byte_size"),
        QStringLiteral("bytes")
    };

    for (const QString &key : sizeKeys) {
        const std::optional<qint64> size = jsonIntegerValue(object.value(key));
        if (size.has_value() && *size > 0) {
            return *size;
        }
    }
    return 0;
}

QString imageCandidateUrl(const QJsonObject &candidate)
{
    return candidate.value(QStringLiteral("url")).toString().trimmed();
}

QString thumbnailUrlFromCandidates(const QJsonArray &candidates)
{
    QString fallback;
    QString selected;
    qint64 selectedArea = std::numeric_limits<qint64>::max();
    qint64 fallbackArea = 0;

    for (const QJsonValue &value : candidates) {
        const QJsonObject candidate = value.toObject();
        const QString url = imageCandidateUrl(candidate);
        if (url.isEmpty()) {
            continue;
        }

        const qint64 width = jsonIntegerValue(candidate.value(QStringLiteral("width"))).value_or(0);
        const qint64 height = jsonIntegerValue(candidate.value(QStringLiteral("height"))).value_or(0);
        const qint64 area = width > 0 && height > 0 ? width * height : 0;
        if (fallback.isEmpty() || area > fallbackArea) {
            fallback = url;
            fallbackArea = area;
        }
        if (width >= 160 && height >= 160 && area > 0 && area < selectedArea) {
            selected = url;
            selectedArea = area;
        }
    }

    return selected.isEmpty() ? fallback : selected;
}

QDateTime fallbackTimestampForIndex(const InstagramPost &post, qsizetype index)
{
    const QDateTime base = post.fetchedAt.isValid() ? post.fetchedAt : QDateTime::currentDateTimeUtc();
    return base.addSecs(-static_cast<qint64>(index));
}

void appendJsonMediaNode(InstagramPost &post,
                         const QJsonObject &node,
                         QSet<QString> &seenUrls,
                         qsizetype &index,
                         const QDateTime &inheritedTimestamp = {})
{
    QDateTime nodeTimestamp = mediaTimestampForNode(node);
    if (!nodeTimestamp.isValid()) {
        nodeTimestamp = inheritedTimestamp;
    }

    QString imageUrl = node.value(QStringLiteral("display_url")).toString();
    QString thumbnailUrl = node.value(QStringLiteral("thumbnail_src")).toString();
    if (thumbnailUrl.isEmpty()) {
        thumbnailUrl = node.value(QStringLiteral("thumbnail_url")).toString();
    }
    if (imageUrl.isEmpty()) {
        imageUrl = node.value(QStringLiteral("thumbnail_src")).toString();
    }
    const QJsonArray imageCandidates = node.value(QStringLiteral("image_versions2"))
                                           .toObject()
                                           .value(QStringLiteral("candidates"))
                                           .toArray();
    QJsonObject selectedImageCandidate;
    if (!imageCandidates.isEmpty()) {
        selectedImageCandidate = imageCandidates.first().toObject();
        const QString candidateThumbnailUrl = thumbnailUrlFromCandidates(imageCandidates);
        if (!candidateThumbnailUrl.isEmpty()) {
            thumbnailUrl = candidateThumbnailUrl;
        }
        if (imageUrl.isEmpty()) {
            imageUrl = selectedImageCandidate.value(QStringLiteral("url")).toString();
        }
    }

    QString videoUrl = node.value(QStringLiteral("video_url")).toString();
    const QJsonArray videos = node.value(QStringLiteral("video_versions")).toArray();
    QJsonObject selectedVideo;
    if (!videos.isEmpty()) {
        selectedVideo = videos.first().toObject();
        if (videoUrl.isEmpty()) {
            videoUrl = selectedVideo.value(QStringLiteral("url")).toString();
        }
    }
    const bool isVideo = !videoUrl.isEmpty();
    const QString mediaUrl = isVideo ? videoUrl : imageUrl;
    if (!mediaUrl.isEmpty() && !seenUrls.contains(mediaUrl)) {
        seenUrls.insert(mediaUrl);
        const bool image = !isVideo;
        const QString suffix = suffixForUrl(mediaUrl, image);
        QString shortcode = node.value(QStringLiteral("shortcode")).toString();
        if (shortcode.isEmpty()) {
            shortcode = node.value(QStringLiteral("code")).toString();
        }
        const QString baseName = shortcode.isEmpty()
            ? QStringLiteral("%1-%2").arg(image ? QStringLiteral("image") : QStringLiteral("video"), QString::number(index, 10))
            : shortcode;

        InstagramMediaItem item;
        item.name = uniqueMediaName(post, baseName, suffix);
        item.path = post.rootPath + QLatin1Char('/') + item.name;
        item.url = mediaUrl;
        item.thumbnailUrl = thumbnailUrl.isEmpty() ? (image ? imageUrl : QString{}) : thumbnailUrl;
        item.suffix = suffix;
        item.mimeType = mimeForSuffix(suffix);
        item.timestamp = nodeTimestamp.isValid() ? nodeTimestamp : fallbackTimestampForIndex(post, index);
        item.size = isVideo ? mediaSizeForObject(selectedVideo) : mediaSizeForObject(selectedImageCandidate);
        if (item.size <= 0) {
            item.size = mediaSizeForObject(node);
        }
        item.image = image;
        post.items.append(item);
        ++index;
    }

    const QJsonArray children = node.value(QStringLiteral("edge_sidecar_to_children"))
                                    .toObject()
                                    .value(QStringLiteral("edges"))
                                    .toArray();
    for (const QJsonValue &childValue : children) {
        appendJsonMediaNode(post, childValue.toObject().value(QStringLiteral("node")).toObject(), seenUrls, index, nodeTimestamp);
    }

    const QJsonArray carousel = node.value(QStringLiteral("carousel_media")).toArray();
    for (const QJsonValue &childValue : carousel) {
        appendJsonMediaNode(post, childValue.toObject(), seenUrls, index, nodeTimestamp);
    }
}

qsizetype matchingJsonObjectEnd(const QString &text, qsizetype start)
{
    if (start < 0 || start >= text.size() || text.at(start) != QLatin1Char('{')) {
        return -1;
    }

    bool inString = false;
    bool escaped = false;
    qsizetype depth = 0;
    for (qsizetype i = start; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == QLatin1Char('\\')) {
                escaped = true;
            } else if (ch == QLatin1Char('"')) {
                inString = false;
            }
            continue;
        }

        if (ch == QLatin1Char('"')) {
            inString = true;
        } else if (ch == QLatin1Char('{')) {
            ++depth;
        } else if (ch == QLatin1Char('}')) {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }

    return -1;
}

std::optional<QJsonObject> parseJsonObjectSlice(const QString &text, qsizetype start)
{
    const qsizetype end = matchingJsonObjectEnd(text, start);
    if (end <= start) {
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QByteArray bytes = text.mid(start, end - start + 1).toUtf8();
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

QString jsonNodeShortcode(const QJsonObject &node)
{
    QString shortcode = node.value(QStringLiteral("shortcode")).toString();
    if (shortcode.isEmpty()) {
        shortcode = node.value(QStringLiteral("code")).toString();
    }
    return shortcode;
}

bool jsonNodeMatchesShortcode(const QJsonObject &node, const QString &shortcode)
{
    return jsonNodeShortcode(node).compare(shortcode, Qt::CaseSensitive) == 0;
}

bool jsonValueContainsShortcode(const QJsonValue &value, const QString &shortcode)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (jsonNodeMatchesShortcode(object, shortcode)) {
            return true;
        }
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (jsonValueContainsShortcode(it.value(), shortcode)) {
                return true;
            }
        }
    } else if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &child : array) {
            if (jsonValueContainsShortcode(child, shortcode)) {
                return true;
            }
        }
    }
    return false;
}

bool jsonObjectHasMediaFields(const QJsonObject &node)
{
    return node.contains(QStringLiteral("display_url"))
        || node.contains(QStringLiteral("thumbnail_src"))
        || node.contains(QStringLiteral("thumbnail_url"))
        || node.contains(QStringLiteral("image_versions2"))
        || node.contains(QStringLiteral("video_url"))
        || node.contains(QStringLiteral("video_versions"))
        || node.contains(QStringLiteral("edge_sidecar_to_children"))
        || node.contains(QStringLiteral("carousel_media"));
}

int mediaNodeScore(const QJsonObject &node, const QString &shortcode)
{
    int score = 0;
    if (jsonNodeMatchesShortcode(node, shortcode)) {
        score += 100;
    } else if (jsonValueContainsShortcode(node, shortcode)) {
        score += 80;
    }
    if (node.contains(QStringLiteral("edge_sidecar_to_children"))
        || node.contains(QStringLiteral("carousel_media"))) {
        score += 60;
    }
    if (node.contains(QStringLiteral("display_url"))
        || node.contains(QStringLiteral("thumbnail_src"))
        || node.contains(QStringLiteral("thumbnail_url"))
        || node.contains(QStringLiteral("image_versions2"))) {
        score += 30;
    }
    if (node.contains(QStringLiteral("video_url"))
        || node.contains(QStringLiteral("video_versions"))) {
        score += 30;
    }
    if (node.contains(QStringLiteral("items"))) {
        score += 20;
    }
    return score;
}

QString jsonStringUnescaped(QString text)
{
    text.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    text.replace(QStringLiteral("\\/"), QStringLiteral("/"));
    text.replace(QStringLiteral("\\u002F"), QStringLiteral("/"));
    text.replace(QStringLiteral("\\u0026"), QStringLiteral("&"));
    return text;
}

void appendDirectPostCandidatesFromText(QList<QJsonObject> &result,
                                        QSet<QString> &seenCompacts,
                                        const QString &text,
                                        const QString &shortcode)
{
    const QString escapedShortcode = QRegularExpression::escape(shortcode);
    const QRegularExpression shortcodeRegex(QStringLiteral("\"((shortcode)|(code))\"\\s*:\\s*\"%1\"|\"%1\"")
                                                .arg(escapedShortcode));
    QRegularExpressionMatchIterator it = shortcodeRegex.globalMatch(text);

    int matchedShortcodes = 0;
    while (it.hasNext() && result.size() < 12 && matchedShortcodes < 80) {
        ++matchedShortcodes;
        const QRegularExpressionMatch match = it.next();
        qsizetype searchFrom = match.capturedStart();
        QJsonObject bestObject;
        int bestScore = 0;

        for (int attempts = 0; attempts < 80 && searchFrom > 0; ++attempts) {
            const qsizetype start = text.lastIndexOf(QLatin1Char('{'), searchFrom);
            if (start < 0) {
                break;
            }
            const std::optional<QJsonObject> object = parseJsonObjectSlice(text, start);
            if (object) {
                const int score = mediaNodeScore(*object, shortcode);
                if (score > bestScore) {
                    bestObject = *object;
                    bestScore = score;
                }
            }
            searchFrom = start - 1;
        }

        if (bestScore < 80) {
            continue;
        }
        const QString compact = QString::fromUtf8(QJsonDocument(bestObject).toJson(QJsonDocument::Compact));
        if (seenCompacts.contains(compact)) {
            continue;
        }
        seenCompacts.insert(compact);
        result.append(bestObject);
    }
}

QList<QJsonObject> directPostCandidateObjects(const QString &html, const QString &shortcode)
{
    QList<QJsonObject> result;
    QSet<QString> seenCompacts;
    appendDirectPostCandidatesFromText(result, seenCompacts, html, shortcode);
    appendDirectPostCandidatesFromText(result, seenCompacts, jsonStringUnescaped(html), shortcode);

    std::sort(result.begin(), result.end(), [&shortcode](const QJsonObject &a, const QJsonObject &b) {
        return mediaNodeScore(a, shortcode) > mediaNodeScore(b, shortcode);
    });
    Q_UNUSED(shortcode)
    traceInstagram(QStringLiteral("Direct post HTML candidates count=%1").arg(result.size()));
    return result;
}

void appendMatchingMediaObjects(InstagramPost &post,
                                const QJsonValue &value,
                                QSet<QString> &seenUrls,
                                qsizetype &index,
                                int &visited)
{
    if (++visited > 512) {
        return;
    }

    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (jsonNodeMatchesShortcode(object, post.shortcode) && jsonObjectHasMediaFields(object)) {
            appendJsonMediaNode(post, object, seenUrls, index);
            return;
        }

        if (object.contains(QStringLiteral("shortcode_media"))) {
            appendMatchingMediaObjects(post, object.value(QStringLiteral("shortcode_media")), seenUrls, index, visited);
        }
        if (object.contains(QStringLiteral("media"))) {
            appendMatchingMediaObjects(post, object.value(QStringLiteral("media")), seenUrls, index, visited);
        }
        if (object.contains(QStringLiteral("items"))) {
            appendMatchingMediaObjects(post, object.value(QStringLiteral("items")), seenUrls, index, visited);
        }

        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            appendMatchingMediaObjects(post, it.value(), seenUrls, index, visited);
        }
    } else if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &child : array) {
            appendMatchingMediaObjects(post, child, seenUrls, index, visited);
        }
    }
}

bool appendDirectPostJsonMedia(InstagramPost &post, const QJsonObject &object)
{
    QSet<QString> seenUrls;
    qsizetype index = post.items.size() + 1;
    int visited = 0;
    appendMatchingMediaObjects(post, object, seenUrls, index, visited);
    return !post.items.isEmpty();
}


} // namespace InstagramProviderInternal
