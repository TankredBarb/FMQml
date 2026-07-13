#pragma once

#include <optional>

#include <QString>
#include <QStringList>

#include "FileProvider.h"

namespace MegaCache {

void clear();

// Link secrets are kept in-memory only. FileEntry::path stores only linkId.
void storeKey(const QString &linkId, const QString &linkKey, bool isFolder);
QString retrieveKey(const QString &linkId, bool *isFolder = nullptr);

void markLinkLoading(const QString &linkId);
void markLinkLoaded(const QString &linkId, bool success, const QString &errorString = {});

void cacheEntry(const QString &path, const FileEntry &entry, const QString &megaHandle);
std::optional<FileEntry> getEntry(const QString &path);
std::optional<QString> getMegaHandle(const QString &path);
void renameSubtree(const QString &oldPath, const QString &newPath, const QString &newName = {});

void cacheChildren(const QString &parentPath, const QStringList &childPaths);
void appendChild(const QString &parentPath, const QString &childPath);
void removeChild(const QString &parentPath, const QString &childPath);
std::optional<QStringList> getChildren(const QString &parentPath);
QList<FileEntry> childEntries(const QString &parentPath);
qint64 accountStorageUsedBytes();

void removePath(const QString &path);
void removeSubtree(const QString &path);

} // namespace MegaCache
