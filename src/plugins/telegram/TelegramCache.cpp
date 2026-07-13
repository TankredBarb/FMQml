#include "TelegramCache.h"

#include <QMutexLocker>

namespace TelegramProviderInternal {

namespace {

struct TelegramSharedCache {
    QHash<QString, TelegramEntry> entries;
    QHash<QString, QStringList> children;
    QHash<QString, TelegramFilesPage> pages;
};

TelegramSharedCache &sharedCache()
{
    static TelegramSharedCache cache;
    return cache;
}

} // namespace

QMutex &cacheMutex()
{
    static QMutex mutex;
    return mutex;
}

void clearCache()
{
    QMutexLocker locker(&cacheMutex());
    sharedCache() = {};
}

void storeEntry(const TelegramEntry &entry)
{
    QMutexLocker locker(&cacheMutex());
    sharedCache().entries.insert(entry.path, entry);
}

std::optional<TelegramEntry> cachedEntry(const QString &path)
{
    QMutexLocker locker(&cacheMutex());
    const auto it = sharedCache().entries.constFind(path);
    if (it == sharedCache().entries.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

void storeChildren(const QString &parentPath, const QList<TelegramEntry> &entries)
{
    QMutexLocker locker(&cacheMutex());
    QStringList paths;
    paths.reserve(entries.size());
    for (const TelegramEntry &entry : entries) {
        sharedCache().entries.insert(entry.path, entry);
        paths.append(entry.path);
    }
    sharedCache().children.insert(parentPath, paths);
}

void appendChildren(const QString &parentPath, const QList<TelegramEntry> &entries)
{
    QMutexLocker locker(&cacheMutex());
    QStringList paths = sharedCache().children.value(parentPath);
    for (const TelegramEntry &entry : entries) {
        sharedCache().entries.insert(entry.path, entry);
        if (!paths.contains(entry.path)) {
            paths.append(entry.path);
        }
    }
    sharedCache().children.insert(parentPath, paths);
}

QStringList cachedChildren(const QString &parentPath)
{
    QMutexLocker locker(&cacheMutex());
    return sharedCache().children.value(parentPath);
}

void storePagination(const QString &parentPath, qint64 nextFromMessageId, bool hasMore)
{
    QMutexLocker locker(&cacheMutex());
    TelegramFilesPage page;
    page.nextFromMessageId = nextFromMessageId;
    page.hasMore = hasMore;
    sharedCache().pages.insert(parentPath, page);
}

std::optional<TelegramFilesPage> pagination(const QString &parentPath)
{
    QMutexLocker locker(&cacheMutex());
    const auto it = sharedCache().pages.constFind(parentPath);
    if (it == sharedCache().pages.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

} // namespace TelegramProviderInternal
