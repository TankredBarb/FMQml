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


} // namespace InstagramProviderInternal
