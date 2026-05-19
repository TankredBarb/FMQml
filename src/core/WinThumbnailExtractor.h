#pragma once

#include <QString>
#include <QImage>
#include <QSize>

#ifdef Q_OS_WIN
class WinThumbnailExtractor {
public:
    // Uses Windows Shell (IThumbnailProvider or IExtractImage) to get the system thumbnail.
    static QImage extract(const QString &path, const QSize &requestedSize);
};
#endif
