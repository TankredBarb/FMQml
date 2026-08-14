#pragma once

#include "TextPreviewTypes.h"

namespace TextPreview {

Classification classify(const QString &fileName, const QString &mimeName,
                        const QByteArray &prefix = {});
Snapshot readFile(const QString &path, const ReadOptions &options = {});
Snapshot readFilePage(const QString &path, qint64 byteOffset, qint64 firstLine,
                      Encoding encoding, const ReadOptions &options = {});
Snapshot readBytes(const QByteArray &bytes, const QString &fileName,
                   const QString &mimeName = {}, const ReadOptions &options = {});
Snapshot readBytesPage(const QByteArray &bytes, qint64 totalBytes,
                       const QString &fileName, const QString &mimeName,
                       qint64 byteOffset, qint64 firstLine, Encoding encoding,
                       const ReadOptions &options = {});
int countFileLines(const QString &path);
int countBytesLines(const QByteArray &bytes);

} // namespace TextPreview
