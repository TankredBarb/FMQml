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

QList<FileEntry> entriesFromProfile(const InstagramPost &profilePost, const InstagramPost &storiesPost)
{
    QList<FileEntry> entries;
    entries.reserve(profilePost.items.size() + 2);
    for (const InstagramMediaItem &item : profilePost.items) {
        entries.append(entryFromMedia(item));
    }
    if (!storiesPost.items.isEmpty()) {
        entries.append(entryFromStories(profilePost, storiesPost));
    }
    if (profilePost.hasNextPage) {
        entries.append(entryFromLoadMore(profilePost));
    }
    return entries;
}


} // namespace InstagramProviderInternal
