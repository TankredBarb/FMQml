#pragma once

#include "PreviewData.h"

#include <QByteArray>

namespace PreviewInternal {
bool readFileRangeAsAdministrator(const QString &path, qint64 offset, qint64 length,
                                  QByteArray *data, qint64 *totalSize);
LocalPreviewData loadLocalPreviewData(const QString &path);
} // namespace PreviewInternal
