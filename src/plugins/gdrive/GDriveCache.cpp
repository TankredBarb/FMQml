#include "GDriveCache.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

namespace {

struct GDriveSharedMetadata {
    QHash<QString, FileEntry> entries;
    QHash<QString, QStringList> children;
    QHash<QString, QString> parents;
    QHash<QString, QString> mimeTypes;
    QHash<QString, GDriveItemCapabilities> capabilities;
    GDriveStorageQuota quota;
};

QMutex &sharedMetadataMutex()
{
    static QMutex mutex;
    return mutex;
}

GDriveSharedMetadata &sharedMetadata()
{
    static GDriveSharedMetadata metadata;
    return metadata;
}

} // namespace

namespace GDriveCache {

void clearSharedMetadata()
{
    QMutexLocker locker(&sharedMetadataMutex());
    sharedMetadata() = {};
}

void cacheSharedEntry(const FileEntry &entry,
                      const QString &parentPath,
                      const QString &mimeType,
                      const GDriveItemCapabilities &capabilities)
{
    QMutexLocker locker(&sharedMetadataMutex());
    GDriveSharedMetadata &metadata = sharedMetadata();
    metadata.entries.insert(entry.path, entry);
    metadata.parents.insert(entry.path, parentPath);
    metadata.mimeTypes.insert(entry.path, mimeType);
    metadata.capabilities.insert(entry.path, capabilities);
}

void cacheSharedChildren(const QString &parentPath, const QStringList &children)
{
    QMutexLocker locker(&sharedMetadataMutex());
    sharedMetadata().children.insert(parentPath, children);
}

void removeSharedPath(const QString &path, const QString &parentPath)
{
    QMutexLocker locker(&sharedMetadataMutex());
    GDriveSharedMetadata &metadata = sharedMetadata();
    metadata.entries.remove(path);
    metadata.parents.remove(path);
    metadata.mimeTypes.remove(path);
    metadata.capabilities.remove(path);
    if (!parentPath.isEmpty()) {
        QStringList children = metadata.children.value(parentPath);
        children.removeAll(path);
        metadata.children.insert(parentPath, children);
    }
    metadata.children.remove(path);
}

std::optional<FileEntry> sharedEntry(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    const auto it = sharedMetadata().entries.constFind(path);
    if (it == sharedMetadata().entries.constEnd()) {
        return std::nullopt;
    }
    return it.value();
}

QStringList sharedChildren(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    return sharedMetadata().children.value(path);
}

std::optional<QStringList> sharedChildrenIfCached(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    const auto it = sharedMetadata().children.constFind(path);
    if (it == sharedMetadata().children.constEnd()) {
        return std::nullopt;
    }
    return it.value();
}

QString sharedParent(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    return sharedMetadata().parents.value(path);
}

QString sharedMimeType(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    return sharedMetadata().mimeTypes.value(path);
}

std::optional<GDriveItemCapabilities> sharedCapabilities(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    const auto it = sharedMetadata().capabilities.constFind(path);
    if (it == sharedMetadata().capabilities.constEnd()) {
        return std::nullopt;
    }
    return it.value();
}

void cacheSharedQuota(const GDriveStorageQuota &quota)
{
    if (!quota.valid) {
        return;
    }

    QMutexLocker locker(&sharedMetadataMutex());
    sharedMetadata().quota = quota;
}

std::optional<GDriveStorageQuota> sharedQuota()
{
    QMutexLocker locker(&sharedMetadataMutex());
    const GDriveStorageQuota quota = sharedMetadata().quota;
    if (!quota.valid) {
        return std::nullopt;
    }
    return quota;
}

} // namespace GDriveCache
