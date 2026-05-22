#include "ThumbnailProvider.h"
#include "ArchiveSupport.h"
#include "FileProviderFactory.h"
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

#include <QUrl>
#include <QDir>
#include <QTemporaryFile>
#include <memory>

#ifdef HAS_TAGLIB
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/vorbisfile.h>
#include <taglib/taglib.h>
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
} // namespace

#ifdef HAS_TAGLIB
QImage extractCoverArt(const QString &path)
{
#ifdef Q_OS_WIN
    const wchar_t *wpath = reinterpret_cast<const wchar_t *>(path.utf16());
#else
    QByteArray utf8Path = path.toUtf8();
    const char *wpath = utf8Path.constData();
#endif

    QImage img;

    // 1. Check for MP3 / MPEG (ID3v2 APIC frame)
    {
        TagLib::MPEG::File mpegFile(wpath);
        if (mpegFile.isValid() && mpegFile.ID3v2Tag()) {
            TagLib::ID3v2::Tag *id3v2 = mpegFile.ID3v2Tag();
            const auto &frameMap = id3v2->frameListMap();
            if (frameMap.contains("APIC")) {
                const auto &frameList = frameMap["APIC"];
                for (auto *frame : frameList) {
                    auto *picFrame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frame);
                    if (picFrame) {
                        const TagLib::ByteVector &data = picFrame->picture();
                        if (!data.isEmpty()) {
                            img = QImage::fromData(
                                reinterpret_cast<const uchar *>(data.data()),
                                static_cast<int>(data.size())
                            );
                            if (!img.isNull()) {
                                return img;
                            }
                        }
                    }
                }
            }
        }
    }

    // 2. Check for FLAC
    {
        TagLib::FLAC::File flacFile(wpath);
        if (flacFile.isValid()) {
            const auto &picList = flacFile.pictureList();
            for (auto *pic : picList) {
                if (pic) {
                    const TagLib::ByteVector &data = pic->data();
                    if (!data.isEmpty()) {
                        img = QImage::fromData(
                            reinterpret_cast<const uchar *>(data.data()),
                            static_cast<int>(data.size())
                        );
                        if (!img.isNull()) {
                            return img;
                        }
                    }
                }
            }
        }
    }

    // 3. Check for MP4 / M4A / M4B
    {
        TagLib::MP4::File mp4File(wpath);
        if (mp4File.isValid() && mp4File.tag()) {
            TagLib::MP4::Tag *tag = mp4File.tag();
            auto itemMap = tag->itemMap();
            if (itemMap.contains("covr")) {
                auto coverList = itemMap["covr"].toCoverArtList();
                for (const auto &cover : coverList) {
                    const TagLib::ByteVector &data = cover.data();
                    if (!data.isEmpty()) {
                        img = QImage::fromData(
                            reinterpret_cast<const uchar *>(data.data()),
                            static_cast<int>(data.size())
                        );
                        if (!img.isNull()) {
                            return img;
                        }
                    }
                }
            }
        }
    }

    // 4. Check for OGG / Vorbis (experimental)
    {
        TagLib::Vorbis::File oggFile(wpath);
        if (oggFile.isValid() && oggFile.tag()) {
            auto fieldMap = oggFile.tag()->fieldListMap();
            if (fieldMap.contains("METADATA_BLOCK_PICTURE")) {
                const auto &list = fieldMap["METADATA_BLOCK_PICTURE"];
                for (const auto &base64Data : list) {
                    QByteArray decoded = QByteArray::fromBase64(QByteArray(base64Data.toCString()));
                    // This is a FLAC picture block. Proper parsing would be better, 
                    // but sometimes QImage can guess if it's raw.
                    img = QImage::fromData(decoded);
                    if (!img.isNull()) {
                        return img;
                    }
                }
            }
        }
    }

    return img;
}
#endif

ThumbnailProvider::ThumbnailProvider()
    : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_cache(kThumbnailCacheLimitKb)
{
}

ThumbnailProvider::~ThumbnailProvider() = default;

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString originalPath = QDir::toNativeSeparators(QUrl::fromPercentEncoding(id.toUtf8()));
    QString path = originalPath;
    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(128, 128);
    const QSize cacheSize = bucketSize(targetSize);

    QString cacheKey = originalPath + QStringLiteral("::")
                    + QString::number(cacheSize.width())
                    + QStringLiteral("x")
                    + QString::number(cacheSize.height());
    {
        QMutexLocker locker(&m_cacheMutex);
        if (QImage *cached = m_cache.object(cacheKey)) {
            if (size) {
                *size = cached->size();
            }
            return *cached;
        }
    }

    QImage thumb;
    QFileInfo fi(path);
    QString suffix = fi.suffix().toLower();
    std::unique_ptr<FileProvider> provider;
    std::unique_ptr<QIODevice> archiveDevice;
    QTemporaryFile tempFile;

    if (ArchiveSupport::isArchivePath(path) || ArchiveSupport::isArchiveFilePath(path)) {
        provider = FileProviderFactory::createProvider(path);
        if (provider) {
            archiveDevice = provider->openRead(path);
        }
        if (archiveDevice) {
            if (tempFile.open()) {
                tempFile.write(archiveDevice->readAll());
                tempFile.flush();
                path = tempFile.fileName();
                fi = QFileInfo(path);
                suffix = fi.suffix().toLower();
            }
        }
    }
    
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
    // 2C. Audio files (via TagLib)
    else if (suffix == "mp3" || suffix == "flac" || suffix == "ogg" || suffix == "m4a" || suffix == "mp4" || suffix == "m4b" || suffix == "wav" || suffix == "wma") {
#ifdef HAS_TAGLIB
        QImage cover = extractCoverArt(path);
        if (!cover.isNull()) {
            thumb = cover.scaled(cacheSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
#endif
    }
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
        if (size) {
            *size = thumb.size();
        }
        return thumb;
    }

    if (size) {
        *size = QSize(0, 0);
    }
    return QImage();
}
