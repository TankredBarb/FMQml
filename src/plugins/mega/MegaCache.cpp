#include "MegaCache.h"

#include <iterator>

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

namespace {

struct LinkState {
    QString key;
    bool isFolder = false;
    bool loading = false;
    bool loaded = false;
    QString error;
};

struct MegaSharedMetadata {
    QHash<QString, LinkState> links;
    QHash<QString, FileEntry> entries;
    QHash<QString, QString> megaHandles;
    QHash<QString, QStringList> children;
};

QMutex &cacheMutex()
{
    static QMutex mutex;
    return mutex;
}

MegaSharedMetadata &sharedCache()
{
    static MegaSharedMetadata cache;
    return cache;
}

bool pathIsInSubtree(const QString &candidate, const QString &root)
{
    return candidate == root || candidate.startsWith(root + QLatin1Char('/'));
}

} // namespace

namespace MegaCache {

void clear()
{
    QMutexLocker locker(&cacheMutex());
    sharedCache() = {};
}

void storeKey(const QString &linkId, const QString &linkKey, bool isFolder)
{
    if (linkId.isEmpty() || linkKey.isEmpty()) {
        return;
    }
    QMutexLocker locker(&cacheMutex());
    auto &state = sharedCache().links[linkId];
    if (state.key != linkKey || state.isFolder != isFolder) {
        state.loaded = false;
        state.loading = false;
        state.error.clear();
    }
    state.key = linkKey;
    state.isFolder = isFolder;
}

QString retrieveKey(const QString &linkId, bool *isFolder)
{
    QMutexLocker locker(&cacheMutex());
    const auto state = sharedCache().links.value(linkId);
    if (isFolder) {
        *isFolder = state.isFolder;
    }
    return state.key;
}

bool hasKey(const QString &linkId)
{
    QMutexLocker locker(&cacheMutex());
    return sharedCache().links.contains(linkId) && !sharedCache().links.value(linkId).key.isEmpty();
}

void markLinkLoading(const QString &linkId)
{
    QMutexLocker locker(&cacheMutex());
    auto &state = sharedCache().links[linkId];
    state.loading = true;
    state.error.clear();
}

void markLinkLoaded(const QString &linkId, bool success, const QString &errorString)
{
    QMutexLocker locker(&cacheMutex());
    auto &state = sharedCache().links[linkId];
    state.loading = false;
    state.loaded = success;
    state.error = success ? QString{} : errorString;
}

bool isLinkLoading(const QString &linkId)
{
    QMutexLocker locker(&cacheMutex());
    return sharedCache().links.value(linkId).loading;
}

bool isLinkLoaded(const QString &linkId)
{
    QMutexLocker locker(&cacheMutex());
    return sharedCache().links.value(linkId).loaded;
}

QString linkError(const QString &linkId)
{
    QMutexLocker locker(&cacheMutex());
    return sharedCache().links.value(linkId).error;
}

void cacheEntry(const QString &path, const FileEntry &entry, const QString &megaHandle)
{
    if (path.isEmpty()) {
        return;
    }
    QMutexLocker locker(&cacheMutex());
    sharedCache().entries.insert(path, entry);
    if (!megaHandle.isEmpty()) {
        sharedCache().megaHandles.insert(path, megaHandle);
    }
}

std::optional<FileEntry> getEntry(const QString &path)
{
    QMutexLocker locker(&cacheMutex());
    const auto it = sharedCache().entries.constFind(path);
    return it == sharedCache().entries.constEnd() ? std::nullopt : std::optional<FileEntry>(*it);
}

std::optional<QString> getMegaHandle(const QString &path)
{
    QMutexLocker locker(&cacheMutex());
    const auto it = sharedCache().megaHandles.constFind(path);
    return it == sharedCache().megaHandles.constEnd() ? std::nullopt : std::optional<QString>(*it);
}

void cacheChildren(const QString &parentPath, const QStringList &childPaths)
{
    if (parentPath.isEmpty()) {
        return;
    }
    QMutexLocker locker(&cacheMutex());
    sharedCache().children.insert(parentPath, childPaths);
}

std::optional<QStringList> getChildren(const QString &parentPath)
{
    QMutexLocker locker(&cacheMutex());
    const auto it = sharedCache().children.constFind(parentPath);
    return it == sharedCache().children.constEnd() ? std::nullopt : std::optional<QStringList>(*it);
}

std::optional<QStringList> getChildrenIfCached(const QString &parentPath)
{
    return getChildren(parentPath);
}

QList<FileEntry> childEntries(const QString &parentPath)
{
    QMutexLocker locker(&cacheMutex());
    QList<FileEntry> entries;
    const auto childrenIt = sharedCache().children.constFind(parentPath);
    if (childrenIt == sharedCache().children.constEnd()) {
        return entries;
    }
    for (const QString &childPath : *childrenIt) {
        const auto entryIt = sharedCache().entries.constFind(childPath);
        if (entryIt != sharedCache().entries.constEnd()) {
            entries.append(*entryIt);
        }
    }
    return entries;
}

void removePath(const QString &path)
{
    QMutexLocker locker(&cacheMutex());
    auto &cache = sharedCache();
    cache.entries.remove(path);
    cache.megaHandles.remove(path);
    cache.children.remove(path);
}

void removeSubtree(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    QMutexLocker locker(&cacheMutex());
    auto &cache = sharedCache();
    for (auto it = cache.entries.begin(); it != cache.entries.end();) {
        it = pathIsInSubtree(it.key(), path) ? cache.entries.erase(it) : std::next(it);
    }
    for (auto it = cache.megaHandles.begin(); it != cache.megaHandles.end();) {
        it = pathIsInSubtree(it.key(), path) ? cache.megaHandles.erase(it) : std::next(it);
    }
    for (auto it = cache.children.begin(); it != cache.children.end();) {
        it = pathIsInSubtree(it.key(), path) ? cache.children.erase(it) : std::next(it);
    }
}

} // namespace MegaCache
