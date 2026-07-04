#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "FileProvider.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace InstagramProviderInternal {

constexpr int RequestTimeoutMs = 20000;
constexpr int InstagramProfileBatchSize = 12;
constexpr const char *InstagramProfilePostsConnectionDocId = "27839684308962379";
constexpr const char *InstagramBrowserUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
constexpr const char *InstagramMobileUserAgent = "Instagram 219.0.0.12.117 Android";
constexpr const char *LoadMoreItemName = "__load_more__";
constexpr const char *StoriesItemName = "stories";
constexpr int StoriesItemNameLength = 7;

struct InstagramMediaItem {
    QString name;
    QString path;
    QString url;
    QString thumbnailUrl;
    QString suffix;
    QString mimeType;
    QDateTime timestamp;
    qint64 size = 0;
    bool image = true;
};

struct InstagramPost {
    QString rootPath;
    QString kind;
    QString shortcode;
    QString sourceUrl;
    QString title;
    QString nextCursor;
    QList<InstagramMediaItem> items;
    QString error;
    QDateTime fetchedAt;
    bool hasNextPage = false;
    bool extendedByPagination = false;
};

struct ParsedPath {
    bool valid = false;
    QString kind;
    QString shortcode;
    QString itemName;
    QString storyItemName;
    QString normalized;
    QString rootPath;
    QString storiesRootPath;
    bool loadMore = false;
    bool stories = false;
};

struct InstagramBootTokens {
    QString lsd;
    QString dtsg;
    QString actorId;
};

QMutex &cacheMutex();
QHash<QString, InstagramPost> &postCache();
QHash<QString, InstagramPost> &storyCache();

bool instagramTraceEnabled();
bool instagramGraphqlFallbackEnabled();
void traceInstagram(const QString &message);

bool isInstagramSchemePath(const QString &path);
ParsedPath parseInstagramPath(const QString &path);
QString instagramUrlToPath(const QString &input);
QString sourceUrlFor(const ParsedPath &path);
QString mediaIdFromShortcode(const QString &shortcode);
QString loadMorePathForRoot(const QString &rootPath);
QString storiesPathForRoot(const QString &rootPath);
QString parentInstagramPath(const QString &path);
QString fileNameForInstagramPath(const QString &path);

QString suffixForUrl(const QString &url, bool image);
QString mimeForSuffix(const QString &suffix);
QString htmlDecode(QString value);
QByteArray httpGetBytes(const QUrl &url,
                        const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                        QString *contentType,
                        QString *error,
                        bool includeCookie = true,
                        const QByteArray &userAgent = {});
QByteArray httpGetBytes(const QUrl &url, QString *contentType, QString *error);
QByteArray httpGetBytesWithoutCookie(const QUrl &url, QString *contentType, QString *error);
QByteArray httpPostFormBytes(const QUrl &url,
                             const QByteArray &body,
                             const QString &referer,
                             const QByteArray &lsd,
                             QString *contentType,
                             QString *error);

InstagramPost fetchPost(const ParsedPath &path, bool allowExpandedCache = true);
InstagramPost fetchNextProfileBatch(const ParsedPath &path);
InstagramPost fetchStoriesForProfile(const ParsedPath &path);
std::optional<InstagramMediaItem> mediaItemForPath(const QString &path);
std::optional<InstagramMediaItem> cachedMediaItemForPath(const QString &path);
void updateCachedMediaItemSize(const QString &path, qint64 size);

FileEntry entryFromMedia(const InstagramMediaItem &item);
FileEntry entryFromPost(const InstagramPost &post);
FileEntry entryFromLoadMore(const InstagramPost &post);
FileEntry entryFromStories(const InstagramPost &profilePost, const InstagramPost &storiesPost);

std::unique_ptr<FileProvider> createInstagramFileProvider();

} // namespace InstagramProviderInternal
