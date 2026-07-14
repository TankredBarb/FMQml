#pragma once

#include "PreviewData.h"

struct FileEntry;

namespace PreviewInternal {
bool materializedRemotePreviewLooksUsable(const QString &path, const FileEntry &entry);
QString cheapFileName(QString path);
ImageMetadataData loadImageMetadataData(const QString &path);
} // namespace PreviewInternal
