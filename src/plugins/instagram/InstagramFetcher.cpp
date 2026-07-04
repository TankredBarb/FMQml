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

QHash<QString, QString> &userCache()
{
    static QHash<QString, QString> cache;
    return cache;
}

QString fetchInstagramUserId(const QString &username)
{
    const QString lowerUser = username.toLower();
    {
        QMutexLocker locker(&cacheMutex());
        const auto it = userCache().constFind(lowerUser);
        if (it != userCache().cend() && !it.value().isEmpty()) {
            return it.value();
        }
    }

    const QUrl searchUrl(QStringLiteral("https://www.instagram.com/web/search/topsearch/?query=%1").arg(lowerUser));
    QString contentType;
    QString error;
    const QByteArray searchBytes = httpGetBytes(searchUrl, &contentType, &error);
    if (!searchBytes.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(searchBytes);
        const QJsonArray users = doc.object().value(QStringLiteral("users")).toArray();
        for (const QJsonValue &val : users) {
            const QJsonObject user = val.toObject().value(QStringLiteral("user")).toObject();
            const QString foundUsername = user.value(QStringLiteral("username")).toString();
            if (foundUsername.compare(lowerUser, Qt::CaseInsensitive) == 0) {
                QString pk = user.value(QStringLiteral("pk")).toVariant().toString();
                if (pk.isEmpty()) {
                    pk = user.value(QStringLiteral("pk_id")).toVariant().toString();
                }
                if (pk.isEmpty()) {
                    pk = user.value(QStringLiteral("id")).toVariant().toString();
                }
                if (!pk.isEmpty()) {
                    traceInstagram(QStringLiteral("Resolved user_id via topsearch"));
                    QMutexLocker locker(&cacheMutex());
                    userCache().insert(lowerUser, pk);
                    return pk;
                }
            }
        }
    }

    const QUrl infoUrl(QStringLiteral("https://www.instagram.com/api/v1/users/web_profile_info/?username=%1").arg(lowerUser));
    const QByteArray infoBytes = httpGetBytes(infoUrl, &contentType, &error);
    if (!infoBytes.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(infoBytes);
        const QString pk = doc.object()
                               .value(QStringLiteral("data"))
                               .toObject()
                               .value(QStringLiteral("user"))
                               .toObject()
                               .value(QStringLiteral("id"))
                               .toString();
        if (!pk.isEmpty()) {
            traceInstagram(QStringLiteral("Resolved user_id via web_profile_info"));
            QMutexLocker locker(&cacheMutex());
            userCache().insert(lowerUser, pk);
            return pk;
        }
    }

    return {};
}

InstagramPost fetchProfileFeedPage(const ParsedPath &path,
                                    const QString &after,
                                    const InstagramPost *basePost = nullptr)
{
    traceInstagram(QStringLiteral("Feed profile after=%1 baseItems=%2")
                       .arg(after.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            QString::number(basePost ? basePost->items.size() : 0)));

    InstagramPost post;
    if (basePost) {
        post = *basePost;
    } else {
        post.rootPath = path.rootPath;
        post.kind = path.kind;
        post.shortcode = path.shortcode;
        post.sourceUrl = QStringLiteral("https://www.instagram.com/%1/").arg(path.shortcode);
        post.title = path.shortcode;
        post.fetchedAt = QDateTime::currentDateTimeUtc();
    }

    const QString userId = fetchInstagramUserId(path.shortcode);
    if (userId.isEmpty()) {
        post.error = QStringLiteral("Could not resolve Instagram user ID for profile '%1'.").arg(path.shortcode);
        return post;
    }

    QUrl url(QStringLiteral("https://www.instagram.com/api/v1/feed/user/%1/").arg(userId));
    if (!after.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("max_id"), after);
        url.setQuery(query);
    }

    QString contentType;
    QString error;
    const QByteArray bytes = httpGetBytes(url, &contentType, &error);
    if (bytes.isEmpty()) {
        post.error = error.isEmpty() ? QStringLiteral("Instagram feed returned an empty response") : error;
        return post;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        post.error = QStringLiteral("Instagram feed JSON parse error: %1").arg(parseError.errorString());
        return post;
    }

    const QJsonObject obj = doc.object();
    const QJsonArray items = obj.value(QStringLiteral("items")).toArray();
    if (items.isEmpty() && !post.items.isEmpty()) {
        post.hasNextPage = false;
        post.nextCursor.clear();
        return post;
    }

    QSet<QString> seenUrls;
    for (const InstagramMediaItem &item : post.items) {
        seenUrls.insert(item.url);
    }
    qsizetype index = post.items.size() + 1;

    for (const QJsonValue &itemVal : items) {
        appendJsonMediaNode(post, itemVal.toObject(), seenUrls, index);
    }

    post.fetchedAt = QDateTime::currentDateTimeUtc();
    const bool moreAvailable = obj.value(QStringLiteral("more_available")).toBool(false);
    const QString nextMaxId = obj.value(QStringLiteral("next_max_id")).toString();
    post.hasNextPage = moreAvailable && !nextMaxId.isEmpty();
    post.nextCursor = nextMaxId;

    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram feed did not expose media for this profile.");
    } else {
        post.error.clear();
    }
    return post;
}

void appendStoryItemsFromArray(InstagramPost &post,
                               const QJsonArray &items,
                               QSet<QString> &seenUrls,
                               qsizetype &index)
{
    for (const QJsonValue &value : items) {
        appendJsonMediaNode(post, value.toObject(), seenUrls, index);
    }
}

InstagramPost parseInstagramStories(const ParsedPath &path, const QByteArray &json)
{
    InstagramPost post;
    post.rootPath = path.storiesRootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = QStringLiteral("https://www.instagram.com/stories/%1/").arg(path.shortcode);
    post.title = QStringLiteral("Stories");
    post.fetchedAt = QDateTime::currentDateTimeUtc();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        post.error = QStringLiteral("Instagram stories JSON parse error: %1").arg(parseError.errorString());
        return post;
    }

    const QJsonObject root = document.object();
    QSet<QString> seenUrls;
    qsizetype index = 1;

    appendStoryItemsFromArray(post,
                              root.value(QStringLiteral("reel")).toObject().value(QStringLiteral("items")).toArray(),
                              seenUrls,
                              index);
    appendStoryItemsFromArray(post, root.value(QStringLiteral("items")).toArray(), seenUrls, index);

    const QJsonArray reelsMedia = root.value(QStringLiteral("reels_media")).toArray();
    for (const QJsonValue &value : reelsMedia) {
        appendStoryItemsFromArray(post, value.toObject().value(QStringLiteral("items")).toArray(), seenUrls, index);
    }

    const QJsonObject reels = root.value(QStringLiteral("reels")).toObject();
    for (auto it = reels.constBegin(); it != reels.constEnd(); ++it) {
        appendStoryItemsFromArray(post, it.value().toObject().value(QStringLiteral("items")).toArray(), seenUrls, index);
    }

    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram stories are unavailable or empty for this profile.");
    } else {
        post.error.clear();
    }
    return post;
}

InstagramPost fetchStoriesForProfile(const ParsedPath &path)
{
    {
        QMutexLocker locker(&cacheMutex());
        const auto it = storyCache().constFind(path.storiesRootPath);
        if (it != storyCache().cend()
            && it->fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) < 120) {
            return it.value();
        }
    }

    InstagramPost post;
    post.rootPath = path.storiesRootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = QStringLiteral("https://www.instagram.com/stories/%1/").arg(path.shortcode);
    post.title = QStringLiteral("Stories");
    post.fetchedAt = QDateTime::currentDateTimeUtc();

    if (InstagramAuth::sessionCookieHeader().isEmpty()) {
        post.error = QStringLiteral("Instagram stories require a logged-in session.");
        return post;
    }

    const QString userId = fetchInstagramUserId(path.shortcode);
    if (userId.isEmpty()) {
        post.error = QStringLiteral("Could not resolve Instagram user ID for stories '%1'.").arg(path.shortcode);
        return post;
    }

    QString contentType;
    QString error;
    const QUrl url(QStringLiteral("https://www.instagram.com/api/v1/feed/user/%1/story/").arg(userId));
    QByteArray bytes = httpGetBytes(url,
                                    {},
                                    &contentType,
                                    &error,
                                    true,
                                    QByteArray(InstagramMobileUserAgent));
    if (bytes.isEmpty()) {
        post.error = error.isEmpty() ? QStringLiteral("Instagram stories returned an empty response") : error;
    } else {
        post = parseInstagramStories(path, bytes);
    }
    if (!post.error.isEmpty()) {
        QUrl fallbackUrl(QStringLiteral("https://www.instagram.com/api/v1/feed/reels_media/"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("reel_ids"), userId);
        fallbackUrl.setQuery(query);
        error.clear();
        contentType.clear();
        bytes = httpGetBytes(fallbackUrl,
                             {},
                             &contentType,
                             &error,
                             true,
                             QByteArray(InstagramMobileUserAgent));
        if (!bytes.isEmpty()) {
            InstagramPost fallbackPost = parseInstagramStories(path, bytes);
            if (fallbackPost.error.isEmpty()) {
                post = fallbackPost;
            }
        }
    }

    traceInstagram(QStringLiteral("Stories profile items=%1 error=%2")
                       .arg(QString::number(post.items.size()),
                            post.error.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));

    QMutexLocker locker(&cacheMutex());
    storyCache().insert(path.storiesRootPath, post);
    return post;
}

void appendJsonTimeline(InstagramPost &post,
                        const QJsonObject &user,
                        const QString &timelineKey,
                        QSet<QString> &seenUrls,
                        qsizetype &index)
{
    const QJsonArray edges = user.value(timelineKey)
                                 .toObject()
                                 .value(QStringLiteral("edges"))
                                 .toArray();
    for (const QJsonValue &edgeValue : edges) {
        appendJsonMediaNode(post, edgeValue.toObject().value(QStringLiteral("node")).toObject(), seenUrls, index);
    }

    if (timelineKey == QLatin1String("edge_owner_to_timeline_media")) {
        const QJsonObject pageInfo = user.value(timelineKey)
                                         .toObject()
                                         .value(QStringLiteral("page_info"))
                                         .toObject();
        post.hasNextPage = pageInfo.value(QStringLiteral("has_next_page")).toBool(false);
        post.nextCursor = pageInfo.value(QStringLiteral("end_cursor")).toString();
    }
}

void appendJsonConnection(InstagramPost &post,
                          const QJsonObject &connection,
                          QSet<QString> &seenUrls,
                          qsizetype &index)
{
    const QJsonArray edges = connection.value(QStringLiteral("edges")).toArray();
    for (const QJsonValue &edgeValue : edges) {
        appendJsonMediaNode(post, edgeValue.toObject().value(QStringLiteral("node")).toObject(), seenUrls, index);
    }

    const QJsonObject pageInfo = connection.value(QStringLiteral("page_info")).toObject();
    post.hasNextPage = pageInfo.value(QStringLiteral("has_next_page")).toBool(false);
    post.nextCursor = pageInfo.value(QStringLiteral("end_cursor")).toString();
}

QByteArray profileGraphqlVariablesJson(const QString &username, const QString &after)
{
    QJsonObject variables;
    variables.insert(QStringLiteral("after"), after.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(after));
    variables.insert(QStringLiteral("before"), QJsonValue::Null);
    variables.insert(QStringLiteral("data"), QJsonValue::Null);
    variables.insert(QStringLiteral("first"), InstagramProfileBatchSize);
    variables.insert(QStringLiteral("last"), QJsonValue::Null);
    variables.insert(QStringLiteral("username"), username);
    variables.insert(QStringLiteral("__relay_internal__pv__PolarisImmersiveFeedChainingEnabledrelayprovider"), false);
    variables.insert(QStringLiteral("__relay_internal__pv__PolarisAIGMMediaWebLabelEnabledrelayprovider"), false);
    variables.insert(QStringLiteral("__relay_internal__pv__PolarisAIGMAccountLabelEnabledrelayprovider"), false);
    variables.insert(QStringLiteral("__relay_internal__pv__PolarisReelsRecoDebugOverlayEnabledrelayprovider"), false);
    return QJsonDocument(variables).toJson(QJsonDocument::Compact);
}

QUrl profileGraphqlUrl(const QString &username, const QString &after)
{
    QUrl url(QStringLiteral("https://www.instagram.com/graphql/query/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("doc_id"), QString::fromLatin1(InstagramProfilePostsConnectionDocId));
    query.addQueryItem(QStringLiteral("variables"), QString::fromUtf8(profileGraphqlVariablesJson(username, after)));
    url.setQuery(query);
    return url;
}

QString firstRegexCapture(const QString &text, const QString &pattern)
{
    const QRegularExpressionMatch match = QRegularExpression(pattern).match(text);
    return match.hasMatch() ? match.captured(1) : QString{};
}

InstagramBootTokens fetchInstagramBootTokens(const QString &username)
{
    InstagramBootTokens tokens;
    QString contentType;
    QString error;
    const QUrl url(QStringLiteral("https://www.instagram.com/%1/").arg(username));
    const QByteArray htmlBytes = httpGetBytes(url, &contentType, &error);
    if (htmlBytes.isEmpty()) {
        traceInstagram(QStringLiteral("GraphQL boot page unavailable error=%1").arg(error));
        return tokens;
    }

    const QString html = QString::fromUtf8(htmlBytes);
    tokens.lsd = firstRegexCapture(html, QStringLiteral(R"ig(\["LSD",\[\],\{"token":"([^"]+)")ig"));
    tokens.dtsg = firstRegexCapture(html, QStringLiteral(R"ig(\["DTSGInitialData",\[\],\{"token":"([^"]+)")ig"));
    tokens.actorId = firstRegexCapture(html, QStringLiteral(R"ig("actorID":"([^"]+)")ig"));
    const QByteArray dsUserId = InstagramAuth::cookieValue(InstagramAuth::sessionCookieHeader(), "ds_user_id");
    traceInstagram(QStringLiteral("GraphQL boot tokens lsd=%1 dtsg=%2 actor=%3 actorMatchesCookieUser=%4")
                       .arg(tokens.lsd.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            tokens.dtsg.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            tokens.actorId.isEmpty() ? QStringLiteral("no")
                                : tokens.actorId == QLatin1String("0") ? QStringLiteral("zero")
                                : QStringLiteral("yes"),
                            !dsUserId.isEmpty() && tokens.actorId.toUtf8() == dsUserId
                                ? QStringLiteral("yes") : QStringLiteral("no")));
    return tokens;
}

QByteArray profileGraphqlPostBody(const QString &username,
                                  const QString &after,
                                  const InstagramBootTokens &tokens)
{
    QUrlQuery form;
    if (!tokens.actorId.isEmpty()) {
        form.addQueryItem(QStringLiteral("av"), tokens.actorId);
        form.addQueryItem(QStringLiteral("__user"), tokens.actorId);
    }
    form.addQueryItem(QStringLiteral("__a"), QStringLiteral("1"));
    form.addQueryItem(QStringLiteral("__req"), QStringLiteral("1"));
    if (!tokens.dtsg.isEmpty()) {
        form.addQueryItem(QStringLiteral("fb_dtsg"), tokens.dtsg);
    }
    if (!tokens.lsd.isEmpty()) {
        form.addQueryItem(QStringLiteral("lsd"), tokens.lsd);
    }
    form.addQueryItem(QStringLiteral("fb_api_req_friendly_name"),
                      QStringLiteral("PolarisProfilePostsTabContentQuery_connection"));
    form.addQueryItem(QStringLiteral("fb_api_caller_class"), QStringLiteral("RelayModern"));
    form.addQueryItem(QStringLiteral("server_timestamps"), QStringLiteral("true"));
    form.addQueryItem(QStringLiteral("doc_id"), QString::fromLatin1(InstagramProfilePostsConnectionDocId));
    form.addQueryItem(QStringLiteral("variables"), QString::fromUtf8(profileGraphqlVariablesJson(username, after)));
    return form.toString(QUrl::FullyEncoded).toUtf8();
}

QStringList jsonObjectKeys(const QJsonObject &object)
{
    QStringList keys;
    keys.reserve(object.size());
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        keys.append(it.key());
    }
    return keys;
}

int graphqlErrorCount(const QJsonObject &root)
{
    return root.value(QStringLiteral("errors")).toArray().size();
}

void traceGraphqlEnvelopeError(const QJsonObject &root)
{
    if (!instagramTraceEnabled() || !root.contains(QStringLiteral("error"))) {
        return;
    }

    const QJsonValue payloadValue = root.value(QStringLiteral("payload"));
    const QString payloadKeys = payloadValue.isObject()
        ? jsonObjectKeys(payloadValue.toObject()).join(QLatin1Char(','))
        : QString{};
    traceInstagram(QStringLiteral("GraphQL envelope error=%1 payloadType=%2 payloadKeys=%3")
                       .arg(root.value(QStringLiteral("error")).isUndefined() ? QStringLiteral("no") : QStringLiteral("yes"),
                            payloadValue.isObject() ? QStringLiteral("object")
                                : payloadValue.isArray() ? QStringLiteral("array")
                                : payloadValue.isString() ? QStringLiteral("string")
                                : payloadValue.isNull() ? QStringLiteral("null")
                                : QStringLiteral("other"),
                            payloadKeys));
}

QByteArray normalizedGraphqlJson(QByteArray json)
{
    json = json.trimmed();
    static constexpr QByteArrayView prefix("for (;;);");
    if (json.startsWith(prefix)) {
        json.remove(0, prefix.size());
    }
    return json.trimmed();
}

InstagramPost parseInstagramProfileGraphql(const ParsedPath &path,
                                           const QByteArray &json,
                                           const InstagramPost *basePost = nullptr)
{
    InstagramPost post;
    if (basePost) {
        post = *basePost;
    } else {
        post.rootPath = path.rootPath;
        post.kind = path.kind;
        post.shortcode = path.shortcode;
        post.sourceUrl = QStringLiteral("https://www.instagram.com/%1/").arg(path.shortcode);
        post.title = path.shortcode;
        post.fetchedAt = QDateTime::currentDateTimeUtc();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(normalizedGraphqlJson(json), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        traceInstagram(QStringLiteral("GraphQL parse error offset=%1 error=%2")
                           .arg(parseError.offset)
                           .arg(parseError.errorString()));
    }
    const QJsonObject connection = document.object()
                                        .value(QStringLiteral("data"))
                                        .toObject()
                                        .value(QStringLiteral("xdt_api__v1__feed__user_timeline_graphql_connection"))
                                        .toObject();
    if (connection.isEmpty()) {
        const QJsonObject root = document.object();
        traceGraphqlEnvelopeError(root);
        traceInstagram(QStringLiteral("GraphQL missing connection rootKeys=%1 dataKeys=%2 errorCount=%3")
                           .arg(jsonObjectKeys(root).join(QLatin1Char(',')),
                                jsonObjectKeys(root.value(QStringLiteral("data")).toObject()).join(QLatin1Char(',')),
                                QString::number(graphqlErrorCount(root))));
        post.error = QStringLiteral("Instagram GraphQL profile data is unavailable.");
        return post;
    }

    QSet<QString> seenUrls;
    for (const InstagramMediaItem &item : post.items) {
        seenUrls.insert(item.url);
    }
    qsizetype index = post.items.size() + 1;
    appendJsonConnection(post, connection, seenUrls, index);
    post.fetchedAt = QDateTime::currentDateTimeUtc();
    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram GraphQL did not expose public media for this profile.");
    } else {
        post.error.clear();
    }
    return post;
}

InstagramPost fetchProfileGraphqlPage(const ParsedPath &path, const QString &after, const InstagramPost *basePost = nullptr)
{
    traceInstagram(QStringLiteral("GraphQL profile after=%1 baseItems=%2")
                       .arg(after.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            QString::number(basePost ? basePost->items.size() : 0)));
    QString contentType;
    QString error;
    const QByteArray bytes = httpGetBytes(profileGraphqlUrl(path.shortcode, after), &contentType, &error);
    if (bytes.isEmpty()) {
        InstagramPost post = basePost ? *basePost : InstagramPost{};
        post.rootPath = path.rootPath;
        post.kind = path.kind;
        post.shortcode = path.shortcode;
        post.sourceUrl = QStringLiteral("https://www.instagram.com/%1/").arg(path.shortcode);
        post.fetchedAt = QDateTime::currentDateTimeUtc();
        post.error = error.isEmpty() ? QStringLiteral("Instagram GraphQL returned an empty response") : error;
        return post;
    }
    InstagramPost post = parseInstagramProfileGraphql(path, bytes, basePost);
    if (!post.error.isEmpty() && !InstagramAuth::sessionCookieHeader().isEmpty()) {
        const InstagramBootTokens tokens = fetchInstagramBootTokens(path.shortcode);
        if (!tokens.lsd.isEmpty() || !tokens.dtsg.isEmpty() || !tokens.actorId.isEmpty()) {
            const QString referer = QStringLiteral("https://www.instagram.com/%1/").arg(path.shortcode);
            const QByteArray body = profileGraphqlPostBody(path.shortcode, after, tokens);
            error.clear();
            contentType.clear();
            const QByteArray postBytes = httpPostFormBytes(QUrl(QStringLiteral("https://www.instagram.com/api/graphql/")),
                                                           body,
                                                           referer,
                                                           tokens.lsd.toUtf8(),
                                                           &contentType,
                                                           &error);
            if (!postBytes.isEmpty()) {
                InstagramPost postResult = parseInstagramProfileGraphql(path, postBytes, basePost);
                if (postResult.error.isEmpty()) {
                    traceInstagram(QStringLiteral("GraphQL POST succeeded after=%1")
                                       .arg(after.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));
                    return postResult;
                }
                const QByteArray dsUserId = InstagramAuth::cookieValue(InstagramAuth::sessionCookieHeader(), "ds_user_id");
                if (!dsUserId.isEmpty() && tokens.actorId.toUtf8() != dsUserId) {
                    InstagramBootTokens cookieActorTokens = tokens;
                    cookieActorTokens.actorId = QString::fromLatin1(dsUserId);
                    traceInstagram(QStringLiteral("GraphQL POST retry with cookie user actor after=%1")
                                       .arg(after.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));
                    error.clear();
                    contentType.clear();
                    const QByteArray cookieActorBody = profileGraphqlPostBody(path.shortcode, after, cookieActorTokens);
                    const QByteArray cookieActorBytes = httpPostFormBytes(QUrl(QStringLiteral("https://www.instagram.com/api/graphql/")),
                                                                          cookieActorBody,
                                                                          referer,
                                                                          tokens.lsd.toUtf8(),
                                                                          &contentType,
                                                                          &error);
                    if (!cookieActorBytes.isEmpty()) {
                        postResult = parseInstagramProfileGraphql(path, cookieActorBytes, basePost);
                        if (postResult.error.isEmpty()) {
                            traceInstagram(QStringLiteral("GraphQL POST cookie actor succeeded after=%1")
                                               .arg(after.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));
                            return postResult;
                        }
                    }
                }
                post = postResult;
            } else if (!error.isEmpty()) {
                post.error = error;
            }
        }
    }
    if (!post.error.isEmpty()) {
        post.error = QStringLiteral("%1 Set FM_INSTAGRAM_COOKIE or FM_INSTAGRAM_COOKIE_FILE with a valid browser session cookie if this profile requires it.")
                         .arg(post.error);
    }
    return post;
}

InstagramPost parseInstagramProfile(const ParsedPath &path, const QByteArray &json)
{
    InstagramPost post;
    post.rootPath = path.rootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = sourceUrlFor(path);
    post.fetchedAt = QDateTime::currentDateTimeUtc();

    const QJsonDocument document = QJsonDocument::fromJson(json);
    const QJsonObject user = document.object()
                                 .value(QStringLiteral("data"))
                                 .toObject()
                                 .value(QStringLiteral("user"))
                                 .toObject();
    if (user.isEmpty()) {
        post.error = QStringLiteral("Instagram profile data is unavailable. It may require login or the response format changed.");
        return post;
    }

    post.title = user.value(QStringLiteral("full_name")).toString();
    if (post.title.isEmpty()) {
        post.title = path.shortcode;
    }

    QSet<QString> seenUrls;
    qsizetype index = 1;
    appendJsonTimeline(post, user, QStringLiteral("edge_owner_to_timeline_media"), seenUrls, index);
    appendJsonTimeline(post, user, QStringLiteral("edge_felix_video_timeline"), seenUrls, index);

    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram did not expose public media for this profile. It may require login or the profile is private.");
    }
    return post;
}

InstagramPost parseInstagramMediaInfo(const ParsedPath &path, const QByteArray &json)
{
    InstagramPost post;
    post.rootPath = path.rootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = sourceUrlFor(path);
    post.title = QStringLiteral("Instagram %1 %2").arg(path.kind, path.shortcode);
    post.fetchedAt = QDateTime::currentDateTimeUtc();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        post.error = QStringLiteral("Instagram media info JSON parse error: %1").arg(parseError.errorString());
        return post;
    }

    const QJsonObject root = document.object();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    QSet<QString> seenUrls;
    qsizetype index = 1;
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        if (!jsonNodeMatchesShortcode(item, path.shortcode)) {
            continue;
        }
        appendJsonMediaNode(post, item, seenUrls, index);
    }

    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram media info did not expose media for this link.");
    } else {
        post.error.clear();
    }
    return post;
}

InstagramPost fetchMediaInfoPost(const ParsedPath &path)
{
    InstagramPost post;
    post.rootPath = path.rootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = sourceUrlFor(path);
    post.fetchedAt = QDateTime::currentDateTimeUtc();

    QString contentType;
    QString error;
    const QUrl url(QStringLiteral("https://www.instagram.com/api/v1/media/shortcode/%1/info/")
                       .arg(path.shortcode));
    QByteArray bytes = httpGetBytes(url, &contentType, &error);
    if (bytes.isEmpty()) {
        const QString mediaId = mediaIdFromShortcode(path.shortcode);
        if (!mediaId.isEmpty()) {
            traceInstagram(QStringLiteral("Media shortcode info unavailable, retry media_id fallback"));
            const QUrl mediaIdUrl(QStringLiteral("https://www.instagram.com/api/v1/media/%1/info/")
                                      .arg(mediaId));
            bytes = httpGetBytes(mediaIdUrl,
                                 {},
                                 &contentType,
                                 &error,
                                 true,
                                 QByteArray(InstagramMobileUserAgent));
        }
    }
    if (bytes.isEmpty()) {
        post.error = error.isEmpty() ? QStringLiteral("Instagram media info returned an empty response") : error;
        return post;
    }
    return parseInstagramMediaInfo(path, bytes);
}

InstagramPost fetchPost(const ParsedPath &path, bool allowExpandedCache);

InstagramPost fetchNextProfileBatch(const ParsedPath &path)
{
    InstagramPost post = fetchPost(path);
    if (path.kind != QLatin1String("user")) {
        post.error = QStringLiteral("Instagram pagination is only supported for profiles");
        return post;
    }
    if (!post.hasNextPage || post.nextCursor.isEmpty()) {
        post.error = QStringLiteral("Instagram did not expose another public batch for this profile");
        return post;
    }

    const qsizetype previousSize = post.items.size();
    InstagramPost feedPost = fetchProfileFeedPage(path, post.nextCursor, &post);
    if (feedPost.error.isEmpty()) {
        feedPost.extendedByPagination = true;
        QMutexLocker locker(&cacheMutex());
        postCache().insert(path.rootPath, feedPost);
        return feedPost;
    }

    if (!instagramGraphqlFallbackEnabled()) {
        return feedPost;
    }

    post = fetchProfileGraphqlPage(path, post.nextCursor, &post);
    if (!post.error.isEmpty()) {
        post.error.replace(QStringLiteral("Instagram GraphQL profile data is unavailable."),
                           QStringLiteral("Instagram pagination is unavailable."));
        return post;
    }
    if (post.items.size() == previousSize) {
        post.error = QStringLiteral("Instagram pagination returned no new public media");
        return post;
    }
    post.error.clear();
    post.extendedByPagination = true;

    QMutexLocker locker(&cacheMutex());
    postCache().insert(path.rootPath, post);
    return post;
}

InstagramPost parseInstagramPost(const ParsedPath &path, const QString &html)
{
    InstagramPost post;
    post.rootPath = path.rootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = sourceUrlFor(path);
    post.fetchedAt = QDateTime::currentDateTimeUtc();
    const QString decodedHtml = htmlDecode(html);

    static const QRegularExpression titleRegex(QStringLiteral(R"(<meta\s+property=["']og:title["']\s+content=["']([^"']*)["'])"),
                                               QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch titleMatch = titleRegex.match(decodedHtml);
    post.title = titleMatch.hasMatch()
        ? htmlDecode(titleMatch.captured(1)).simplified()
        : QStringLiteral("Instagram %1 %2").arg(path.kind, path.shortcode);

    QStringList imageUrls;
    QStringList videoUrls;

    const QList<QJsonObject> candidates = directPostCandidateObjects(decodedHtml, path.shortcode);
    for (const QJsonObject &candidate : candidates) {
        if (appendDirectPostJsonMedia(post, candidate)) {
            break;
        }
    }

    if (post.items.isEmpty()) {
        addUniqueUrl(imageUrls, metaPropertyContent(decodedHtml, QStringLiteral("og:image")));
        addUniqueUrl(imageUrls, metaPropertyContent(decodedHtml, QStringLiteral("twitter:image")));
        addUniqueUrl(videoUrls, metaPropertyContent(decodedHtml, QStringLiteral("og:video")));
        addUniqueUrl(videoUrls, metaPropertyContent(decodedHtml, QStringLiteral("og:video:secure_url")));

        if (imageUrls.isEmpty()) {
            addUniqueUrl(imageUrls, metaPropertyContent(decodedHtml, QStringLiteral("og:image:secure_url")));
        }
        if (videoUrls.isEmpty()) {
            addUniqueUrl(videoUrls, metaPropertyContent(decodedHtml, QStringLiteral("twitter:player:stream")));
        }

        qsizetype index = 1;
        for (const QString &url : imageUrls) {
            const QString suffix = suffixForUrl(url, true);
            InstagramMediaItem item;
            item.name = uniqueMediaName(post, QStringLiteral("image-%1").arg(index, 2, 10, QLatin1Char('0')), suffix);
            item.path = post.rootPath + QLatin1Char('/') + item.name;
            item.url = url;
            item.thumbnailUrl = url;
            item.suffix = suffix;
            item.mimeType = mimeForSuffix(suffix);
            item.timestamp = fallbackTimestampForIndex(post, index);
            item.image = true;
            post.items.append(item);
            ++index;
        }
    }

    if (post.items.isEmpty()) {
        qsizetype videoIndex = 1;
        for (const QString &url : videoUrls) {
            const QString suffix = suffixForUrl(url, false);
            InstagramMediaItem item;
            item.name = uniqueMediaName(post, QStringLiteral("video-%1").arg(videoIndex, 2, 10, QLatin1Char('0')), suffix);
            item.path = post.rootPath + QLatin1Char('/') + item.name;
            item.url = url;
            item.thumbnailUrl = imageUrls.isEmpty() ? QString{} : imageUrls.first();
            item.suffix = suffix;
            item.mimeType = mimeForSuffix(suffix);
            item.timestamp = fallbackTimestampForIndex(post, videoIndex);
            item.image = false;
            post.items.append(item);
            ++videoIndex;
        }
    }

    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram did not expose public media for this link. It may require login or the page format changed.");
    }
    return post;
}

InstagramPost fetchPost(const ParsedPath &path, bool allowExpandedCache)
{
    {
        QMutexLocker locker(&cacheMutex());
        const auto it = postCache().constFind(path.rootPath);
        if (it != postCache().cend()
            && it->fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) < 300
            && (allowExpandedCache || !it->extendedByPagination)
            && (it->error.isEmpty() || InstagramAuth::sessionCookieHeader().isEmpty())) {
            return it.value();
        }
    }

    if (path.kind == QLatin1String("user")) {
        if (!InstagramAuth::sessionCookieHeader().isEmpty()) {
            InstagramPost post = fetchProfileFeedPage(path, {});
            if (post.error.isEmpty()) {
                QMutexLocker locker(&cacheMutex());
                postCache().insert(path.rootPath, post);
                return post;
            }
            if (instagramGraphqlFallbackEnabled()) {
                post = fetchProfileGraphqlPage(path, {});
                if (post.error.isEmpty()) {
                    QMutexLocker locker(&cacheMutex());
                    postCache().insert(path.rootPath, post);
                    return post;
                }
            }
        }
    } else {
        InstagramPost post = fetchMediaInfoPost(path);
        if (post.error.isEmpty()) {
            QMutexLocker locker(&cacheMutex());
            postCache().insert(path.rootPath, post);
            return post;
        }
    }

    QString contentType;
    QString error;
    const QByteArray bytes = path.kind == QLatin1String("user")
        ? httpGetBytesWithoutCookie(QUrl(sourceUrlFor(path)), &contentType, &error)
        : httpGetBytes(QUrl(sourceUrlFor(path)), &contentType, &error);
    InstagramPost post;
    if (bytes.isEmpty()) {
        if (path.kind == QLatin1String("user") && !InstagramAuth::sessionCookieHeader().isEmpty()) {
            post = fetchProfileFeedPage(path, {});
            if (post.error.isEmpty()) {
                QMutexLocker locker(&cacheMutex());
                postCache().insert(path.rootPath, post);
                return post;
            }
        }
        post.rootPath = path.rootPath;
        post.kind = path.kind;
        post.shortcode = path.shortcode;
        post.sourceUrl = sourceUrlFor(path);
        post.fetchedAt = QDateTime::currentDateTimeUtc();
        post.error = error.isEmpty() ? QStringLiteral("Instagram returned an empty response") : error;
    } else if (path.kind == QLatin1String("user")) {
        post = parseInstagramProfile(path, bytes);
        if (!post.error.isEmpty()) {
            InstagramPost feedPost = fetchProfileFeedPage(path, {});
            if (feedPost.error.isEmpty()) {
                post = feedPost;
            }
        }
    } else {
        post = parseInstagramPost(path, QString::fromUtf8(bytes));
    }

    QMutexLocker locker(&cacheMutex());
    postCache().insert(path.rootPath, post);
    return post;
}

std::optional<InstagramMediaItem> mediaItemForPath(const QString &path)
{
    const ParsedPath parsed = parseInstagramPath(path);
    if (!parsed.valid || parsed.itemName.isEmpty()) {
        return std::nullopt;
    }
    const InstagramPost post = parsed.stories ? fetchStoriesForProfile(parsed) : fetchPost(parsed);
    const QString itemName = parsed.stories ? parsed.storyItemName : parsed.itemName;
    for (const InstagramMediaItem &item : post.items) {
        if (item.name == itemName || item.path == parsed.normalized) {
            return item;
        }
    }
    return std::nullopt;
}

std::optional<InstagramMediaItem> cachedMediaItemForPath(const QString &path)
{
    const ParsedPath parsed = parseInstagramPath(path);
    if (!parsed.valid || parsed.itemName.isEmpty() || parsed.loadMore || (parsed.stories && parsed.storyItemName.isEmpty())) {
        return std::nullopt;
    }

    QMutexLocker locker(&cacheMutex());
    const auto postIt = parsed.stories
        ? storyCache().constFind(parsed.storiesRootPath)
        : postCache().constFind(parsed.rootPath);
    const auto postEnd = parsed.stories ? storyCache().constEnd() : postCache().constEnd();
    if (postIt == postEnd) {
        return std::nullopt;
    }
    const QString itemName = parsed.stories ? parsed.storyItemName : parsed.itemName;
    for (const InstagramMediaItem &item : postIt->items) {
        if (item.name == itemName || item.path == parsed.normalized) {
            return item;
        }
    }
    return std::nullopt;
}

void updateCachedMediaItemSize(const QString &path, qint64 size)
{
    if (size <= 0) {
        return;
    }

    const ParsedPath parsed = parseInstagramPath(path);
    if (!parsed.valid || parsed.itemName.isEmpty()) {
        return;
    }

    QMutexLocker locker(&cacheMutex());
    auto postIt = parsed.stories
        ? storyCache().find(parsed.storiesRootPath)
        : postCache().find(parsed.rootPath);
    const auto postEnd = parsed.stories ? storyCache().end() : postCache().end();
    if (postIt == postEnd) {
        return;
    }
    const QString itemName = parsed.stories ? parsed.storyItemName : parsed.itemName;
    for (InstagramMediaItem &item : postIt->items) {
        if (item.name == itemName || item.path == parsed.normalized) {
            item.size = size;
            return;
        }
    }
}

QString parentInstagramPath(const QString &path)
{
    const ParsedPath parsed = parseInstagramPath(path);
    if (!parsed.valid) {
        return {};
    }
    if (parsed.itemName.isEmpty()) {
        return {};
    }
    if (parsed.stories) {
        return parsed.storyItemName.isEmpty() ? parsed.rootPath : parsed.storiesRootPath;
    }
    return parsed.rootPath;
}

QString fileNameForInstagramPath(const QString &path)
{
    const ParsedPath parsed = parseInstagramPath(path);
    if (!parsed.valid) {
        return {};
    }
    if (parsed.itemName.isEmpty()) {
        return QStringLiteral("Instagram %1").arg(parsed.shortcode);
    }
    if (parsed.stories) {
        return parsed.storyItemName.isEmpty() ? QStringLiteral("Stories") : parsed.storyItemName;
    }
    return parsed.itemName;
}

QString byteSizeText(qint64 size)
{
    if (size <= 0) {
        return {};
    }
    if (size < 1024) {
        return QStringLiteral("%1 B").arg(size);
    }
    const double kib = double(size) / 1024.0;
    if (kib < 1024.0) {
        return QStringLiteral("%1 KB").arg(kib, 0, 'f', 1);
    }
    return QStringLiteral("%1 MB").arg(kib / 1024.0, 0, 'f', 1);
}

FileEntry entryFromMedia(const InstagramMediaItem &item)
{
    FileEntry entry;
    entry.name = item.name;
    entry.path = item.path;
    entry.suffix = item.suffix;
    entry.size = item.size;
    entry.sizeText = byteSizeText(item.size);
    entry.modified = item.timestamp.isValid() ? item.timestamp : QDateTime::currentDateTimeUtc();
    entry.created = entry.modified;
    entry.modifiedText = entry.modified.toLocalTime().toString(Qt::ISODate);
    entry.createdText = entry.created.toLocalTime().toString(Qt::ISODate);
    entry.attributesText = QStringLiteral("Instagram public media");
    entry.providerCapabilitiesText = QStringLiteral("Read-only experimental provider");
    entry.mimeType = item.mimeType;
    entry.isDirectory = false;
    entry.isReadOnly = true;
    entry.isImage = item.image;
    entry.hasThumbnail = !item.thumbnailUrl.isEmpty();
    return entry;
}

FileEntry entryFromPost(const InstagramPost &post)
{
    FileEntry entry;
    entry.name = post.title.isEmpty() ? QStringLiteral("Instagram %1").arg(post.shortcode) : post.title;
    entry.path = post.rootPath;
    entry.modified = post.fetchedAt;
    entry.modifiedText = entry.modified.toString(Qt::ISODate);
    entry.attributesText = QStringLiteral("Instagram public link");
    if (post.hasNextPage) {
        entry.attributesText = QStringLiteral("Instagram public link; more media is available");
    }
    entry.providerCapabilitiesText = QStringLiteral("Read-only experimental provider");
    entry.isDirectory = true;
    entry.isReadOnly = true;
    return entry;
}

FileEntry entryFromLoadMore(const InstagramPost &post)
{
    FileEntry entry;
    entry.name = QStringLiteral("Load more...");
    entry.path = loadMorePathForRoot(post.rootPath);
    entry.modified = post.fetchedAt;
    entry.modifiedText = entry.modified.toString(Qt::ISODate);
    entry.attributesText = InstagramAuth::sessionCookieHeader().isEmpty()
        ? QStringLiteral("Instagram next batch; may require FM_INSTAGRAM_COOKIE")
        : QStringLiteral("Instagram next batch");
    entry.providerCapabilitiesText = QStringLiteral("Read-only experimental provider");
    entry.iconName = QStringLiteral("instagram-load-more");
    entry.isDirectory = true;
    entry.isReadOnly = true;
    return entry;
}

FileEntry entryFromStories(const InstagramPost &profilePost, const InstagramPost &storiesPost)
{
    FileEntry entry;
    entry.name = QStringLiteral("Stories");
    entry.path = storiesPathForRoot(profilePost.rootPath);
    entry.modified = storiesPost.fetchedAt.isValid() ? storiesPost.fetchedAt : profilePost.fetchedAt;
    entry.modifiedText = entry.modified.toString(Qt::ISODate);
    entry.attributesText = QStringLiteral("Instagram stories");
    entry.providerCapabilitiesText = QStringLiteral("Read-only experimental provider");
    entry.iconName = QStringLiteral("instagram-stories");
    entry.isDirectory = true;
    entry.isReadOnly = true;
    return entry;
}

} // namespace InstagramProviderInternal
