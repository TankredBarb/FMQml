#include "ThumbnailProvider.h"
#include <QImageReader>
#include <QFileInfo>

ThumbnailProvider::ThumbnailProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_cache(200) // Cache 200 thumbnails
{
}

ThumbnailProvider::~ThumbnailProvider() = default;

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QString path = id;
    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(128, 128);
    
    if (size) {
        *size = targetSize;
    }

    QString cacheKey = path + QString::number(targetSize.width()) + "x" + QString::number(targetSize.height());
    if (m_cache.contains(cacheKey)) {
        return *m_cache.object(cacheKey);
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    
    if (reader.canRead()) {
        const QSize imageSize = reader.size();
        if (imageSize.isValid()) {
            QSize thumbSize = imageSize;
            thumbSize.scale(targetSize, Qt::KeepAspectRatio);
            reader.setScaledSize(thumbSize);
        }
        
        QImage thumb = reader.read();
        if (!thumb.isNull()) {
            m_cache.insert(cacheKey, new QImage(thumb));
            return thumb;
        }
    }

    return QImage();
}
