#include "GDriveEntryMapper.h"

#include "GDriveCache.h"
#include "GDriveExportPolicy.h"
#include "GDrivePath.h"

#include <QDateTime>
#include <QJsonObject>
#include <QLocale>
#include <QSet>

namespace GDriveEntryMapper {
namespace {
constexpr QLatin1StringView GoogleDriveFolderMime{"application/vnd.google-apps.folder"};
constexpr QLatin1StringView GoogleDriveShortcutMime{"application/vnd.google-apps.shortcut"};
using GDriveCache::cacheSharedChildren;
using GDriveCache::cacheSharedEntry;
using GDriveCache::cacheSharedThumbnailLink;
using GDriveCache::sharedChildren;
using GDriveCache::sharedEntry;
using GDriveCache::sharedMimeType;
using GDriveCache::sharedParent;
using GDriveExportPolicy::iconSuffixForMimeType;
using GDriveExportPolicy::isGoogleAppsMimeType;
} // namespace

QString suffixForName(const QString &name)
{
    if (name.endsWith(QStringLiteral(".fb2.zip"), Qt::CaseInsensitive)) {
        return QStringLiteral("fb2.zip");
    }
    const int dotIndex = name.lastIndexOf(QLatin1Char('.'));
    if (dotIndex <= 0 || dotIndex == name.size() - 1) {
        return {};
    }
    return name.mid(dotIndex + 1).toLower();
}

QString byteSizeText(qint64 size)
{
    return QLocale().formattedDataSize(size);
}

QString isoDateText(const QDateTime &dateTime)
{
    return dateTime.isValid() ? dateTime.toLocalTime().toString(Qt::ISODate) : QString{};
}

bool isImageMimeType(const QString &mimeType)
{
    return mimeType.startsWith(QStringLiteral("image/"), Qt::CaseInsensitive);
}

using GDriveExportPolicy::defaultExportFormatForGoogleAppsMimeType;
using GDriveExportPolicy::exportFormatForGoogleAppsDownload;
using GDriveExportPolicy::iconSuffixForMimeType;
using GDriveExportPolicy::isGoogleAppsMimeType;
using GDriveExportPolicy::safeLocalExportFileName;
using GDriveExportPolicy::uniqueLocalFilePath;
using GDriveExportPolicy::withExportSuffix;


QString driveCapabilitiesText(const GDriveItemCapabilities &capabilities);

GDriveItemCapabilities shortcutAliasCapabilities()
{
    GDriveItemCapabilities capabilities;
    capabilities.canListChildren = true;
    return capabilities;
}

GDriveItemCapabilities shortcutsRootCapabilities()
{
    GDriveItemCapabilities capabilities;
    capabilities.canListChildren = true;
    return capabilities;
}

GDriveItemCapabilities trashRootCapabilities()
{
    GDriveItemCapabilities capabilities;
    capabilities.canListChildren = true;
    return capabilities;
}

GDriveItemCapabilities shortcutViewCapabilities(const GDriveItemCapabilities &capabilities)
{
    GDriveItemCapabilities result;
    result.canDownload = capabilities.canDownload;
    result.canListChildren = capabilities.canListChildren;
    result.canCopy = capabilities.canCopy;
    return result;
}

FileEntry shortcutViewEntry(FileEntry entry, const GDriveItemCapabilities &capabilities)
{
    entry.isReadOnly = true;
    entry.providerCapabilitiesText = driveCapabilitiesText(shortcutViewCapabilities(capabilities));
    return entry;
}

GDriveItemCapabilities trashViewCapabilities(const GDriveItemCapabilities &capabilities)
{
    GDriveItemCapabilities result;
    result.canListChildren = capabilities.canListChildren;
    return result;
}

FileEntry trashViewEntry(FileEntry entry, const GDriveItemCapabilities &capabilities)
{
    entry.isReadOnly = true;
    entry.providerCapabilitiesText = driveCapabilitiesText(trashViewCapabilities(capabilities));
    return entry;
}

std::optional<FileEntry> shortcutAliasEntryFor(const FileEntry &shortcutEntry)
{
    if (!shortcutEntry.isShortcut
        || !shortcutEntry.shortcutTargetIsDirectory
        || shortcutEntry.shortcutOpenPath.isEmpty()
        || shortcutEntry.shortcutTargetPath.isEmpty()) {
        return std::nullopt;
    }

    FileEntry aliasEntry = shortcutEntry;
    aliasEntry.path = shortcutEntry.shortcutOpenPath;
    aliasEntry.mimeType = QString(GoogleDriveFolderMime);
    aliasEntry.suffix.clear();
    aliasEntry.isDirectory = true;
    aliasEntry.isImage = false;
    aliasEntry.hasThumbnail = false;
    aliasEntry.isReadOnly = true;
    aliasEntry.providerCapabilitiesText = driveCapabilitiesText(shortcutAliasCapabilities());
    return aliasEntry;
}

void cacheSharedShortcutAlias(const FileEntry &shortcutEntry, const QString &parentPath)
{
    const std::optional<FileEntry> aliasEntry = shortcutAliasEntryFor(shortcutEntry);
    if (!aliasEntry) {
        return;
    }

    cacheSharedEntry(*aliasEntry,
                     parentPath,
                     QString(GoogleDriveFolderMime),
                     shortcutAliasCapabilities());
}

void cacheSharedShortcutInRoot(const FileEntry &shortcutEntry, const GDriveItemCapabilities &capabilities)
{
    if (!shortcutEntry.isShortcut || shortcutEntry.path.isEmpty()) {
        return;
    }

    const GDriveItemCapabilities viewCapabilities = shortcutViewCapabilities(capabilities);
    const FileEntry viewEntry = shortcutViewEntry(shortcutEntry, capabilities);
    cacheSharedEntry(viewEntry, QString(GDrivePath::ShortcutsRoot), QString(GoogleDriveShortcutMime), viewCapabilities);
    cacheSharedShortcutAlias(viewEntry, QString(GDrivePath::ShortcutsRoot));

    QStringList children = sharedChildren(QString(GDrivePath::ShortcutsRoot));
    if (!children.contains(viewEntry.path)) {
        children.append(viewEntry.path);
        cacheSharedChildren(QString(GDrivePath::ShortcutsRoot), children);
    }
}

bool isSharedTrashViewPath(const QString &path)
{
    QString current = GDrivePath::normalizedPath(path);
    QSet<QString> seen;
    while (!current.isEmpty() && !seen.contains(current)) {
        if (current == GDrivePath::Trash) {
            return true;
        }
        seen.insert(current);
        current = sharedParent(current);
    }
    return false;
}

std::optional<ExportTarget> googleAppsExportTargetForPath(const QString &path)
{
    const QString normalized = GDrivePath::normalizedPath(path);
    if (normalized.isEmpty()) {
        return std::nullopt;
    }

    QString sourcePath = normalized;
    QString displayName = GDrivePath::fallbackFileNameForPath(normalized);
    QString mimeType = sharedMimeType(normalized);
    if (const std::optional<FileEntry> entry = sharedEntry(normalized)) {
        displayName = entry->name;
        if (entry->isDirectory) {
            return std::nullopt;
        }
        if (entry->isShortcut) {
            sourcePath = entry->shortcutTargetPath;
            mimeType = entry->shortcutTargetMimeType;
        } else {
            mimeType = entry->mimeType;
        }
    }

    if (sourcePath.isEmpty() || !isGoogleAppsMimeType(mimeType)) {
        return std::nullopt;
    }
    return ExportTarget{sourcePath, displayName, mimeType};
}

QString driveContentIdForPath(const QString &path)
{
    const QString shortcutId = GDrivePath::idForShortcutPath(path);
    if (!shortcutId.isEmpty()) {
        const std::optional<FileEntry> aliasEntry = sharedEntry(path);
        if (!aliasEntry || !aliasEntry->shortcutTargetIsDirectory) {
            return {};
        }
        return GDrivePath::idForItemPath(aliasEntry->shortcutTargetPath);
    }
    return GDrivePath::driveParentIdForPath(path);
}

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString driveCapabilitiesText(const GDriveItemCapabilities &capabilities)
{
    return QStringLiteral(
               "canDownload: %1\n"
               "canEdit: %2\n"
               "canAddChildren: %3\n"
               "canListChildren: %4\n"
               "canRename: %5\n"
               "canTrash: %6\n"
               "canDelete: %7\n"
               "canCopy: %8")
        .arg(boolText(capabilities.canDownload),
             boolText(capabilities.canEdit),
             boolText(capabilities.canAddChildren),
             boolText(capabilities.canListChildren),
             boolText(capabilities.canRename),
             boolText(capabilities.canTrash),
             boolText(capabilities.canDelete),
             boolText(capabilities.canCopy));
}

QVariantMap capabilityProperty(const QString &label, bool value)
{
    return {
        {QStringLiteral("label"), label},
        {QStringLiteral("value"), value ? QStringLiteral("Allowed") : QStringLiteral("Unavailable")},
        {QStringLiteral("active"), value},
    };
}

QVariantList driveCapabilitiesProperties(const GDriveItemCapabilities &capabilities)
{
    return {
        capabilityProperty(QStringLiteral("Download"), capabilities.canDownload),
        capabilityProperty(QStringLiteral("Edit"), capabilities.canEdit),
        capabilityProperty(QStringLiteral("Create inside"), capabilities.canAddChildren),
        capabilityProperty(QStringLiteral("Browse / traverse"), capabilities.canListChildren),
        capabilityProperty(QStringLiteral("Rename"), capabilities.canRename),
        capabilityProperty(QStringLiteral("Trash"), capabilities.canTrash),
        capabilityProperty(QStringLiteral("Delete permanently"), capabilities.canDelete),
        capabilityProperty(QStringLiteral("Copy"), capabilities.canCopy),
    };
}

FileEntry virtualDirectoryEntry(const QString &name, const QString &path, const GDriveItemCapabilities &capabilities)
{
    FileEntry entry;
    entry.name = name;
    entry.path = path;
    entry.providerCapabilitiesText = driveCapabilitiesText(capabilities);
    entry.iconName = GDrivePath::virtualIconNameForPath(path);
    entry.modified = QDateTime::currentDateTime();
    entry.created = entry.modified;
    entry.modifiedText = isoDateText(entry.modified);
    entry.createdText = entry.modifiedText;
    entry.isDirectory = true;
    entry.isReadOnly = true;
    return entry;
}

GDriveItemCapabilities driveCapabilitiesFromDriveFileObject(const QJsonObject &object)
{
    GDriveItemCapabilities result;
    const QJsonObject capabilities = object.value(QStringLiteral("capabilities")).toObject();
    result.canDownload = capabilities.value(QStringLiteral("canDownload")).toBool(false);
    result.canEdit = capabilities.value(QStringLiteral("canEdit")).toBool(false);
    result.canAddChildren = capabilities.value(QStringLiteral("canAddChildren")).toBool(false);
    result.canListChildren = capabilities.value(QStringLiteral("canListChildren")).toBool(false);
    result.canRename = capabilities.value(QStringLiteral("canRename")).toBool(false);
    result.canTrash = capabilities.value(QStringLiteral("canTrash")).toBool(false);
    result.canDelete = capabilities.value(QStringLiteral("canDelete")).toBool(false);
    result.canCopy = capabilities.value(QStringLiteral("canCopy")).toBool(false);
    return result;
}

QString driveQueryForPath(const QString &path)
{
    if (path == GDrivePath::MyDrive) {
        return QStringLiteral("'root' in parents and trashed = false");
    }
    if (path == GDrivePath::SharedWithMe) {
        return QStringLiteral("sharedWithMe = true and trashed = false");
    }
    if (path == GDrivePath::ShortcutsRoot) {
        return QStringLiteral("mimeType = '%1' and trashed = false").arg(QString(GoogleDriveShortcutMime));
    }
    if (path == GDrivePath::Trash) {
        return QStringLiteral("trashed = true");
    }

    const QString folderId = driveContentIdForPath(path);
    if (!folderId.isEmpty()) {
        const QString trashed = isSharedTrashViewPath(path) ? QStringLiteral("true") : QStringLiteral("false");
        return QStringLiteral("'%1' in parents and trashed = %2").arg(folderId, trashed);
    }

    return {};
}

FileEntry entryFromDriveFileObject(const QJsonObject &object)
{
    const QString id = object.value(QStringLiteral("id")).toString().trimmed();
    const QString name = object.value(QStringLiteral("name")).toString().trimmed();
    const QString mimeType = object.value(QStringLiteral("mimeType")).toString();
    if (id.isEmpty() || name.isEmpty()) {
        return {};
    }

    const bool directory = mimeType == GoogleDriveFolderMime;
    const bool shortcut = mimeType == GoogleDriveShortcutMime;
    const QJsonObject shortcutDetails = object.value(QStringLiteral("shortcutDetails")).toObject();
    const QString shortcutTargetId = shortcutDetails.value(QStringLiteral("targetId")).toString().trimmed();
    const QString shortcutTargetMimeType = shortcutDetails.value(QStringLiteral("targetMimeType")).toString();
    const QString shortcutTargetResourceKey = shortcutDetails.value(QStringLiteral("targetResourceKey")).toString().trimmed();
    const GDriveItemCapabilities capabilities = driveCapabilitiesFromDriveFileObject(object);
    const QString thumbnailLink = object.value(QStringLiteral("thumbnailLink")).toString().trimmed();
    FileEntry entry;
    entry.name = name;
    entry.path = GDrivePath::itemPathForId(id);
    entry.mimeType = mimeType;
    entry.suffix = shortcut ? QStringLiteral("shortcut") : (directory ? QString{} : suffixForName(name));
    if (entry.suffix.isEmpty()) {
        entry.suffix = iconSuffixForMimeType(mimeType);
    }
    entry.isDirectory = directory;
    entry.isShortcut = shortcut;
    entry.shortcutTargetPath = shortcutTargetId.isEmpty() ? QString{} : GDrivePath::itemPathForId(shortcutTargetId);
    entry.shortcutTargetMimeType = shortcutTargetMimeType;
    entry.shortcutTargetResourceKey = shortcutTargetResourceKey;
    entry.shortcutTargetIsDirectory = shortcutTargetMimeType == GoogleDriveFolderMime;
    if (entry.isShortcut && entry.shortcutTargetIsDirectory && !entry.shortcutTargetPath.isEmpty()) {
        entry.shortcutOpenPath = GDrivePath::shortcutPathForId(id);
    }
    if (entry.isShortcut) {
        if (entry.shortcutTargetIsDirectory) {
            entry.iconName = QStringLiteral("gdrive-shortcut");
        } else {
            entry.iconName = QStringLiteral("gdrive-file-shortcut");
        }
    }
    entry.isReadOnly = true;
    entry.isImage = isImageMimeType(mimeType);
    entry.hasThumbnail = !directory && !thumbnailLink.isEmpty();
    entry.providerCapabilitiesText = driveCapabilitiesText(capabilities);
    cacheSharedThumbnailLink(entry.path, thumbnailLink);

    bool ok = false;
    const qint64 size = object.value(QStringLiteral("size")).toString().toLongLong(&ok);
    if (ok) {
        entry.size = size;
        entry.sizeText = byteSizeText(size);
    }

    entry.modified = QDateTime::fromString(object.value(QStringLiteral("modifiedTime")).toString(), Qt::ISODateWithMs);
    if (!entry.modified.isValid()) {
        entry.modified = QDateTime::fromString(object.value(QStringLiteral("modifiedTime")).toString(), Qt::ISODate);
    }
    entry.created = QDateTime::fromString(object.value(QStringLiteral("createdTime")).toString(), Qt::ISODateWithMs);
    if (!entry.created.isValid()) {
        entry.created = QDateTime::fromString(object.value(QStringLiteral("createdTime")).toString(), Qt::ISODate);
    }
    entry.modifiedText = isoDateText(entry.modified);
    entry.createdText = isoDateText(entry.created);
    return entry;
}

FileEntry shortcutEntryWithTargetMetadata(FileEntry shortcutEntry, const FileEntry &targetEntry)
{
    shortcutEntry.size = targetEntry.size;
    shortcutEntry.modified = targetEntry.modified;
    shortcutEntry.created = targetEntry.created;
    shortcutEntry.modifiedText = targetEntry.modifiedText;
    shortcutEntry.createdText = targetEntry.createdText;
    shortcutEntry.mimeType = targetEntry.mimeType;
    shortcutEntry.isImage = targetEntry.isImage;
    shortcutEntry.hasThumbnail = targetEntry.hasThumbnail;
    shortcutEntry.providerCapabilitiesText = targetEntry.providerCapabilitiesText;
    if (!targetEntry.suffix.isEmpty()) {
        shortcutEntry.suffix = targetEntry.suffix;
    }
    return shortcutEntry;
}


} // namespace GDriveEntryMapper
