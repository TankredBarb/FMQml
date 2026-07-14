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


} // namespace InstagramProviderInternal
