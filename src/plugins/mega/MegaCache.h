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
bool hasKey(const QString &linkId);

void markLinkLoading(const QString &linkId);
void markLinkLoaded(const QString &linkId, bool success, const QString &errorString = {});
bool isLinkLoading(const QString &linkId);
bool isLinkLoaded(const QString &linkId);
QString linkError(const QString &linkId);

void cacheEntry(const QString &path, const FileEntry &entry, const QString &megaHandle);
std::optional<FileEntry> getEntry(const QString &path);
std::optional<QString> getMegaHandle(const QString &path);

void cacheChildren(const QString &parentPath, const QStringList &childPaths);
std::optional<QStringList> getChildren(const QString &parentPath);
std::optional<QStringList> getChildrenIfCached(const QString &parentPath);
QList<FileEntry> childEntries(const QString &parentPath);

void removePath(const QString &path);
void removeSubtree(const QString &path);

} // namespace MegaCache
