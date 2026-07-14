#pragma once

#include <QString>

namespace GDriveExportPolicy {

struct ExportFormat
{
    QString mimeType;
    QString suffix;
};

bool isGoogleAppsMimeType(const QString &mimeType);
ExportFormat defaultExportFormatForGoogleAppsMimeType(QString mimeType);
ExportFormat exportFormatForGoogleAppsDownload(const QString &mimeType, const QString &destinationFilePath);
QString withExportSuffix(QString name, const QString &suffix);
QString safeLocalExportFileName(QString name);
QString uniqueLocalFilePath(const QString &path);
QString iconSuffixForMimeType(QString mimeType);

} // namespace GDriveExportPolicy
