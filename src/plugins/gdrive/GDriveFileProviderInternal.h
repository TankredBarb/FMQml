#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <QJsonObject>
#include <QString>
#include <QVariantList>

#include "GDriveTypes.h"

class FileProvider;
class QNetworkAccessManager;

namespace GDriveFileProviderInternal {

struct ExportTarget
{
    QString sourcePath;
    QString displayName;
    QString mimeType;
};

std::unique_ptr<FileProvider> createProvider();
bool isTrashViewPath(const QString &path);
std::optional<ExportTarget> exportTargetForPath(const QString &path);
QVariantList capabilitiesProperties(const GDriveItemCapabilities &capabilities);
bool downloadFileToLocalFile(QNetworkAccessManager &network,
                             const QString &sourcePath,
                             const QString &mimeType,
                             const QString &destinationFilePath,
                             const QString &accessToken,
                             const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                             QString *error,
                             const QString &resourceKey = {});
bool restoreFileBlocking(QNetworkAccessManager &network,
                         const QString &fileId,
                         const QString &accessToken,
                         QJsonObject *restoredObject,
                         QString *error);

} // namespace GDriveFileProviderInternal
