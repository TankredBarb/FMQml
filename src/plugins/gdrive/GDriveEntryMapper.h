#pragma once

#include <optional>

#include <QJsonObject>
#include <QString>
#include <QVariantList>

#include "FileProvider.h"
#include "GDriveTypes.h"

namespace GDriveEntryMapper {

struct ExportTarget
{
    QString sourcePath;
    QString displayName;
    QString mimeType;
};

GDriveItemCapabilities shortcutAliasCapabilities();
GDriveItemCapabilities shortcutsRootCapabilities();
GDriveItemCapabilities trashRootCapabilities();
GDriveItemCapabilities shortcutViewCapabilities(const GDriveItemCapabilities &capabilities);
FileEntry shortcutViewEntry(FileEntry entry, const GDriveItemCapabilities &capabilities);
GDriveItemCapabilities trashViewCapabilities(const GDriveItemCapabilities &capabilities);
FileEntry trashViewEntry(FileEntry entry, const GDriveItemCapabilities &capabilities);
std::optional<FileEntry> shortcutAliasEntryFor(const FileEntry &shortcutEntry);
void cacheSharedShortcutAlias(const FileEntry &shortcutEntry, const QString &parentPath);
void cacheSharedShortcutInRoot(const FileEntry &shortcutEntry, const GDriveItemCapabilities &capabilities);
bool isSharedTrashViewPath(const QString &path);
std::optional<ExportTarget> googleAppsExportTargetForPath(const QString &path);
QString driveContentIdForPath(const QString &path);
QString driveCapabilitiesText(const GDriveItemCapabilities &capabilities);
QVariantList driveCapabilitiesProperties(const GDriveItemCapabilities &capabilities);
FileEntry virtualDirectoryEntry(const QString &name,
                                const QString &path,
                                const GDriveItemCapabilities &capabilities = {});
GDriveItemCapabilities driveCapabilitiesFromDriveFileObject(const QJsonObject &object);
QString driveQueryForPath(const QString &path);
FileEntry entryFromDriveFileObject(const QJsonObject &object);
FileEntry shortcutEntryWithTargetMetadata(FileEntry shortcutEntry, const FileEntry &targetEntry);

} // namespace GDriveEntryMapper
