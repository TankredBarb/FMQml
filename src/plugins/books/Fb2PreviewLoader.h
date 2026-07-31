#pragma once

#include "../../preview/PreviewData.h"

class QIODevice;
class QImage;

namespace PreviewInternal {
BookPreviewData loadFb2PreviewData(const QString &path, bool includeContent);
BookPreviewData loadFb2PreviewData(QIODevice *device, const QString &sourcePath, bool includeContent);
bool isFb2ZipPath(const QString &path);
BookPreviewData loadFb2ArchiveEntryPreviewData(const QString &entryPath, bool includeContent);
BookPreviewData loadFb2ZipPreviewData(const QString &path, bool includeContent);
QImage extractFb2ZipCoverArt(const QString &path);
QImage extractFb2CoverArt(const QString &path);
QImage extractFb2CoverArt(QIODevice *device);
} // namespace PreviewInternal
