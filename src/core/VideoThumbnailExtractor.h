#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <functional>

struct VideoThumbnailRequest {
    QString path;
    QSize targetSize;
    qint64 preferredTimestampMs = -1;
    std::function<bool()> cancelled = {};
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
