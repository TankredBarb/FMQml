#pragma once

#include <optional>

#include <QString>
#include <QStringList>

#include "FileProvider.h"
#include "GDriveTypes.h"

namespace GDriveCache {

void clearSharedMetadata();
void cacheSharedEntry(const FileEntry &entry,
                      const QString &parentPath,
                      const QString &mimeType,
                      const GDriveItemCapabilities &capabilities = {});
void cacheSharedChildren(const QString &parentPath, const QStringList &children);
void removeSharedPath(const QString &path, const QString &parentPath);
void cacheSharedThumbnailLink(const QString &path, const QString &thumbnailLink);

std::optional<FileEntry> sharedEntry(const QString &path);
QStringList sharedChildren(const QString &path);
std::optional<QStringList> sharedChildrenIfCached(const QString &path);
QString sharedParent(const QString &path);
QString sharedMimeType(const QString &path);
QString sharedThumbnailLink(const QString &path);
std::optional<GDriveItemCapabilities> sharedCapabilities(const QString &path);

void cacheSharedQuota(const GDriveStorageQuota &quota);
std::optional<GDriveStorageQuota> sharedQuota();

} // namespace GDriveCache
