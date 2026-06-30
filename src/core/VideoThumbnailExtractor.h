#pragma once

#include <QImage>
#include <QSize>
#include <QString>

struct VideoThumbnailRequest {
    QString path;
    QSize targetSize;
    qint64 preferredTimestampMs = -1;
};

struct VideoThumbnailResult {
    QImage image;
    qint64 timestampMs = -1;
    QSize sourceSize;
    int streamIndex = -1;
    QString error;
};

class VideoThumbnailExtractor {
public:
    static bool isAvailable();
    static VideoThumbnailResult extract(const VideoThumbnailRequest &request);
};
