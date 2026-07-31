#pragma once

#include "PreviewData.h"

class QIODevice;
class QImage;

namespace PreviewInternal {
Fb2PreviewData loadFb2PreviewData(const QString &path, bool includeContent);
Fb2PreviewData loadFb2PreviewData(QIODevice *device, const QString &sourcePath, bool includeContent);
bool isFb2ZipPath(const QString &path);
Fb2PreviewData loadFb2ArchiveEntryPreviewData(const QString &entryPath, bool includeContent);
Fb2PreviewData loadFb2ZipPreviewData(const QString &path, bool includeContent);
QImage extractFb2ZipCoverArt(const QString &path);
QImage extractFb2CoverArt(const QString &path);
QImage extractFb2CoverArt(QIODevice *device);
} // namespace PreviewInternal
