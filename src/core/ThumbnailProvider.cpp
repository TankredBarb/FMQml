#include "ThumbnailProvider.h"
#include <QImageReader>
#include <QFileInfo>
#include <QMutexLocker>
#include <QtMath>

namespace {
constexpr qsizetype kThumbnailCacheLimitKb = 64 * 1024;

QSize bucketSize(const QSize &size)
{
    auto bucketDim = [](int value) {
        if (value <= 0) {
            return 128;
        }
        const int bucket = 64;
        return qBound(bucket, ((value + bucket - 1) / bucket) * bucket, 512);
    };

    return QSize(bucketDim(size.width()), bucketDim(size.height()));
}
}

ThumbnailProvider::ThumbnailProvider()
    : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_cache(kThumbnailCacheLimitKb)
{
}

ThumbnailProvider::~ThumbnailProvider() = default;

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QString path = id;
    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(128, 128);
    const QSize cacheSize = bucketSize(targetSize);
    
    if (size) {
        *size = cacheSize;
    }

    QString cacheKey = path + QStringLiteral("::")
                    + QString::number(cacheSize.width())
                    + QStringLiteral("x")
                    + QString::number(cacheSize.height());
    {
        QMutexLocker locker(&m_cacheMutex);
        if (QImage *cached = m_cache.object(cacheKey)) {
            return *cached;
        }
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    
    if (reader.canRead()) {
        const QSize imageSize = reader.size();
        if (imageSize.isValid()) {
            QSize thumbSize = imageSize;
            thumbSize.scale(cacheSize, Qt::KeepAspectRatio);
            reader.setScaledSize(thumbSize);
        }
        
        QImage thumb = reader.read();
        if (!thumb.isNull()) {
            const int costKb = qMax(1, int((thumb.sizeInBytes() + 1023) / 1024));
            QMutexLocker locker(&m_cacheMutex);
            m_cache.insert(cacheKey, new QImage(thumb), costKb);
            return thumb;
        }
    }

    return QImage();
}
