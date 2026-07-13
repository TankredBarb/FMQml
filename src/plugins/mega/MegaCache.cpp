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

void renameSubtree(const QString &oldPath, const QString &newPath, const QString &newName)
{
    if (oldPath.isEmpty() || newPath.isEmpty() || oldPath == newPath) {
        return;
    }
    QMutexLocker locker(&cacheMutex());
    auto &cache = sharedCache();

    QHash<QString, FileEntry> renamedEntries;
    QHash<QString, QString> renamedHandles;
    QHash<QString, QStringList> renamedChildren;

    auto rewritePath = [&](const QString &path) {
        if (!pathIsInSubtree(path, oldPath)) {
            return path;
        }
        return newPath + path.mid(oldPath.size());
    };

    for (auto it = cache.entries.constBegin(); it != cache.entries.constEnd(); ++it) {
        FileEntry entry = it.value();
        const QString rewritten = rewritePath(it.key());
        if (rewritten != it.key()) {
            entry.path = rewritten;
            if (it.key() == oldPath && !newName.isEmpty()) {
                entry.name = newName;
            }
        }
        renamedEntries.insert(rewritten, entry);
    }
    for (auto it = cache.megaHandles.constBegin(); it != cache.megaHandles.constEnd(); ++it) {
        renamedHandles.insert(rewritePath(it.key()), it.value());
    }
    for (auto it = cache.children.constBegin(); it != cache.children.constEnd(); ++it) {
        QStringList children;
        for (const QString &child : it.value()) {
            children.append(rewritePath(child));
        }
        renamedChildren.insert(rewritePath(it.key()), children);
    }

    cache.entries = renamedEntries;
    cache.megaHandles = renamedHandles;
    cache.children = renamedChildren;
}

void cacheChildren(const QString &parentPath, const QStringList &childPaths)
{
    if (parentPath.isEmpty()) {
        return;
    }
    QMutexLocker locker(&cacheMutex());
    sharedCache().children.insert(parentPath, childPaths);
}

void appendChild(const QString &parentPath, const QString &childPath)
{
    if (parentPath.isEmpty() || childPath.isEmpty()) {
        return;
    }
    QMutexLocker locker(&cacheMutex());
    QStringList &children = sharedCache().children[parentPath];
    if (!children.contains(childPath)) {
        children.append(childPath);
    }
}

void removeChild(const QString &parentPath, const QString &childPath)
{
    if (parentPath.isEmpty() || childPath.isEmpty()) {
        return;
    }
    QMutexLocker locker(&cacheMutex());
    auto it = sharedCache().children.find(parentPath);
    if (it != sharedCache().children.end()) {
        it->removeAll(childPath);
    }
}

std::optional<QStringList> getChildren(const QString &parentPath)
{
    QMutexLocker locker(&cacheMutex());
    const auto it = sharedCache().children.constFind(parentPath);
    return it == sharedCache().children.constEnd() ? std::nullopt : std::optional<QStringList>(*it);
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

qint64 accountStorageUsedBytes()
{
    QMutexLocker locker(&cacheMutex());
    qint64 used = 0;
    const auto &entries = sharedCache().entries;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const FileEntry &entry = it.value();
        if (!entry.path.startsWith(QStringLiteral("mega:///")) || entry.isDirectory) {
            continue;
        }
        used += entry.size;
    }
    return used;
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
