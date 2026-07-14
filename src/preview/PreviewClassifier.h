#pragma once

#include "PreviewData.h"

#include <QSize>

struct FileEntry;

namespace PreviewInternal {
bool quickLookPreviewTraceEnabled();
bool isDjvuDocument(const QString &suffix, const QString &mimeName);
bool isPreviewableRasterImage(const QString &suffix, const QString &mimeName);
QVariant prop(const QString &label, const QString &value);
QString propertyValue(const QVariantList &properties, const QString &label);
QSize dimensionsFromText(const QString &text);
void setPropertyValue(QVariantList &properties, const QString &label, const QString &value);
void removePropertyValue(QVariantList &properties, const QString &label);
QString remotePreviewRoot(bool create);
void removeRemotePreviewDir(const QString &path);
QString redactedPreviewPathForLog(const QString &path);
QString safePreviewFileName(QString name);
QString remotePreviewTooLargeText(const FileEntry &entry);
QString googleDriveAccessSummary(const FileEntry &entry);
QString googleDriveAccountLabel();
bool isTextSuffix(const QString &suffix);
bool isOfficeDocumentSuffix(const QString &suffix);
QString officeDocumentMimeLabel(const QString &suffix);
bool isFb2Suffix(const QString &suffix);
bool isGoogleAppsMimeType(const QString &mimeType);
QString materializedPreviewSuffix(const FileEntry &entry);
bool isVideoPreviewEntry(const FileEntry &entry);
bool materializedVideoLooksUsable(const QString &path, const QString &suffix);
} // namespace PreviewInternal
