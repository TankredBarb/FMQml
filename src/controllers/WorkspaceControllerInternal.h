#pragma once

#include <QVariantMap>

namespace WorkspaceControllerInternal {
QString normalizedLocalPath(const QString &path);
#ifdef Q_OS_WIN
bool deletePolicyPathEquals(const QString &lhs, const QString &rhs);
bool deletePolicyIsChildOfPath(const QString &path, const QString &ancestor);
#endif
QString nativeDisplayPath(const QString &path);
QString uriSchemeForPath(const QString &path);
bool isProviderUriPath(const QString &path);
bool isLocalFilesystemPath(const QString &path);
QString localPathFromUrlVariant(const QVariant &value);
QString externalDropStatusMessage(int acceptedCount, int conflictCount, int invalidCount);
bool pathsReferToSameDropDestination(const QString &lhs, const QString &rhs);
bool isPortablePlaceRoot(const QString &path);
bool pathBelongsToProviderPlaceRoot(const QString &path, const QString &rootPath);
QString normalizedArchiveFormat(QString format);
QString archiveExtractionBaseName(const QString &fileName);
bool archiveFormatRequiresSingleFile(const QString &format);
QString uniqueArchivePath(const QString &folderPath, const QStringList &sources, const QString &format);
QVariantMap makeDeleteDetails(bool blocked,
                              bool warning,
                              bool explicitConfirmation,
                              const QString &title,
                              const QString &subtitle,
                              const QString &details,
                              const QString &confirmPhrase,
                              const QString &buttonText);
} // namespace WorkspaceControllerInternal
