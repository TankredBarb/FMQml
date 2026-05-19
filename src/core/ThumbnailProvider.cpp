#include "ThumbnailProvider.h"
#include <QImageReader>
#include <QFileInfo>
#include <QMutexLocker>
#include <QtMath>
#include <QSvgRenderer>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QRawFont>

#ifdef Q_OS_WIN
#include "WinThumbnailExtractor.h"
#endif

#ifdef HAS_QT_PDF
#include <QPdfDocument>
#endif

namespace {
constexpr qsizetype kThumbnailCacheLimitKb = 64 * 1024;

QSize bucketSize(const QSize &size)
{
    auto bucketDim = [](int value) {
        if (value <= 0) {
            return 128;
        }
        const int bucket = 64;
        return qBound(bucket, ((value + bucket - 1) / bucket) * bucket, 2048);
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

    QImage thumb;
    QFileInfo fi(path);
    QString suffix = fi.suffix().toLower();
    
    // 1. SVG
    if (suffix == "svg" || suffix == "svgz") {
        QSvgRenderer renderer(path);
        if (renderer.isValid()) {
            thumb = QImage(cacheSize, QImage::Format_ARGB32_Premultiplied);
            thumb.fill(Qt::transparent);
            QPainter p(&thumb);
            renderer.render(&p);
        }
    }
    // 2. Font
    else if (suffix == "ttf" || suffix == "otf" || suffix == "woff" || suffix == "woff2") {
        QRawFont rawFont(path, cacheSize.height() * 0.4);
        if (rawFont.isValid()) {
            thumb = QImage(cacheSize, QImage::Format_ARGB32_Premultiplied);
            thumb.fill(Qt::transparent);
            QPainter p(&thumb);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QColor("#FFFFFF")); // White text
            
            QString sample = "Aa";
            QPainterPath pathObj;
            
            QList<quint32> glyphs;
            QList<QPointF> positions;
            
            qreal x = 0;
            for (int i = 0; i < sample.length(); ++i) {
                quint32 glyph = rawFont.glyphIndexesForString(sample.mid(i, 1)).first();
                glyphs.append(glyph);
                positions.append(QPointF(x, rawFont.ascent()));
                
                QPainterPath glyphPath = rawFont.pathForGlyph(glyph);
                glyphPath.translate(x, rawFont.ascent());
                pathObj.addPath(glyphPath);
                
                QList<QPointF> advances = rawFont.advancesForGlyphIndexes({glyph});
                if (!advances.isEmpty()) x += advances.first().x();
            }
            
            QRectF bounds = pathObj.boundingRect();
            // Center the text
            p.translate((cacheSize.width() - bounds.width()) / 2.0 - bounds.x(),
                        (cacheSize.height() - bounds.height()) / 2.0 - bounds.y());
            
            p.drawPath(pathObj);
        }
    }
#ifdef HAS_QT_PDF
    // 2B. PDF (via QPdfDocument)
    else if (suffix == "pdf") {
        QPdfDocument pdf;
        if (pdf.load(path) == QPdfDocument::Error::None) {
            if (pdf.pageCount() > 0) {
                QSizeF pageSize = pdf.pagePointSize(0);
                QSize renderSize = cacheSize;
                if (pageSize.isValid()) {
                    qreal ratio = pageSize.width() / pageSize.height();
                    if (ratio > 1.0) {
                        renderSize.setHeight(qRound(cacheSize.width() / ratio));
                    } else {
                        renderSize.setWidth(qRound(cacheSize.height() * ratio));
                    }
                }
                thumb = pdf.render(0, renderSize);
            }
        }
    }
#else
    else if (suffix == "pdf") {
        // Fallback to image reader/shell
    }
#endif
    // 3. Image (via QImageReader)
    else {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        
        if (reader.canRead()) {
            const QSize imageSize = reader.size();
            if (imageSize.isValid()) {
                QSize thumbSize = imageSize;
                thumbSize.scale(cacheSize, Qt::KeepAspectRatio);
                reader.setScaledSize(thumbSize);
            }
            thumb = reader.read();
        }
    }
    
    // 4. Fallback to Windows Shell (for video, PDF, Office, etc.)
#ifdef Q_OS_WIN
    if (thumb.isNull() && !fi.isDir()) {
        thumb = WinThumbnailExtractor::extract(path, cacheSize);
    }
#endif
    
    if (!thumb.isNull()) {
            const int costKb = qMax(1, int((thumb.sizeInBytes() + 1023) / 1024));
            QMutexLocker locker(&m_cacheMutex);
            m_cache.insert(cacheKey, new QImage(thumb), costKb);
            return thumb;
        }

    return QImage();
}
