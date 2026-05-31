#pragma once

#include <QString>
#include <QStringList>

namespace ArchiveSupport {

bool isArchiveExtension(const QString &suffix);
bool isArchiveFilePath(const QString &path);
bool isArchivePath(const QString &path);
bool archiveBackendAvailable();
QString archiveLibraryPath();
QString sevenZipExecutablePath();

QString physicalArchivePath(const QString &path);
QStringList archiveSegments(const QString &path);
QString archiveBrowsePath(const QString &path);
QString archiveRootPath(const QString &physicalArchivePath);
QString archiveRootPathForPath(const QString &path);
QString archiveChildPath(const QString &parentPath, const QString &childName);
QString archiveParentPath(const QString &path);
QString archiveFileName(const QString &path);
QString normalizeArchivePath(const QString &path);
QString stripArchiveScheme(const QString &path);
QStringList splitArchiveTokens(const QString &path);

} // namespace ArchiveSupport
