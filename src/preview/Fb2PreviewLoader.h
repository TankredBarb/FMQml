#pragma once

#include "PreviewData.h"

class QIODevice;

namespace PreviewInternal {
int fb2PageCharLimitForPixelSize(int pixelSize);
QStringList buildFb2Pages(const QStringList &paragraphs, int pageCharLimit);
Fb2PreviewData loadFb2PreviewData(const QString &path, bool includeContent);
Fb2PreviewData loadFb2PreviewData(QIODevice *device, const QString &sourcePath, bool includeContent);
bool isFb2ZipPath(const QString &path);
Fb2PreviewData loadFb2ArchiveEntryPreviewData(const QString &entryPath, bool includeContent);
Fb2PreviewData loadFb2ZipPreviewData(const QString &path, bool includeContent);
} // namespace PreviewInternal
