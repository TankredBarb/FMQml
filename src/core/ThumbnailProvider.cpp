#include "ThumbnailProvider.h"
#include "ArchiveSupport.h"
#include "FileProviderFactory.h"
#include <QElapsedTimer>
#include <QFile>
#include <QImageReader>
#include <QFileInfo>
#include <QMutexLocker>
#include <QtMath>
#include <QSvgRenderer>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QRawFont>
#include <QSet>
#include <QXmlStreamReader>

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

bool thumbnailTimingEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_THUMBNAIL_TIMING");
    return enabled;
}

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

QImage transparentImage(const QSize &size)
{
    QImage image(size.isValid() ? size : QSize(1, 1), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

bool isAudioSuffix(const QString &suffix)
{
    static const QSet<QString> suffixes = {
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"), QStringLiteral("oga"),
        QStringLiteral("m4a"), QStringLiteral("m4b"), QStringLiteral("mp4"), QStringLiteral("wav"),
        QStringLiteral("wma"), QStringLiteral("aac"), QStringLiteral("opus"), QStringLiteral("aiff"),
        QStringLiteral("aif"), QStringLiteral("alac"), QStringLiteral("ape"), QStringLiteral("mka")
    };
    return suffixes.contains(suffix.toLower());
}

QImage audioCoverPlaceholder(const QSize &size)
{
    const QSize imageSize = size.isValid() ? size : QSize(128, 128);
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal side = qMin(imageSize.width(), imageSize.height());
    const qreal stroke = qMax<qreal>(2.0, side * 0.055);
    const qreal stemX = imageSize.width() * 0.58;
    const qreal stemTop = imageSize.height() * 0.25;
    const qreal stemBottom = imageSize.height() * 0.66;
    const qreal headW = side * 0.26;
    const qreal headH = side * 0.17;
    const QRectF headRect(stemX - headW * 0.72, stemBottom - headH * 0.16, headW, headH);
    const QPointF flagEnd(imageSize.width() * 0.73, imageSize.height() * 0.34);

    QPainterPath note;
    note.addEllipse(headRect);
    note.moveTo(stemX, stemBottom);
    note.lineTo(stemX, stemTop);
    note.quadTo(imageSize.width() * 0.68, imageSize.height() * 0.26, flagEnd.x(), flagEnd.y());

    painter.setPen(QPen(QColor(111, 129, 255, 220), stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(111, 129, 255, 96));
    painter.drawPath(note);

    return image;
}

QString xmlAttributeValue(const QXmlStreamAttributes &attributes, QStringView name)
{
    for (const QXmlStreamAttribute &attribute : attributes) {
        if (attribute.name() == name) {
            return attribute.value().toString();
        }
    }
    return {};
}

QImage extractFb2CoverArt(QIODevice *device)
{
    if (!device || !device->isOpen()) {
        return {};
    }

    QString coverId;
    QXmlStreamReader xml(device);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) {
            continue;
        }

        const QString name = xml.name().toString();
        if (name == QLatin1String("image") && coverId.isEmpty()) {
            coverId = xmlAttributeValue(xml.attributes(), QStringLiteral("href"));
            if (coverId.startsWith(QLatin1Char('#'))) {
                coverId.remove(0, 1);
            }
        } else if (name == QLatin1String("binary")) {
            const QString id = xml.attributes().value(QStringLiteral("id")).toString();
            if (!coverId.isEmpty() && id == coverId) {
                const QString encoded = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                const QByteArray bytes = QByteArray::fromBase64(encoded.toLatin1());
                return QImage::fromData(bytes);
            }
        }
    }

    return {};
}

QImage extractFb2CoverArt(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    return extractFb2CoverArt(&file);
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
    QElapsedTimer totalTimer;
    if (thumbnailTimingEnabled()) {
        totalTimer.start();
    }

    const QString decodedPath = QUrl::fromPercentEncoding(id.toUtf8());
    QString originalPath = ArchiveSupport::isArchivePath(decodedPath)
        ? QDir::fromNativeSeparators(decodedPath)
        : QDir::toNativeSeparators(decodedPath);
    const bool coverOnly = originalPath.endsWith(QStringLiteral("::cover"));
    if (coverOnly) {
        originalPath.chop(7);
    }
    QString path = originalPath;
    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(128, 128);
    const QSize cacheSize = bucketSize(targetSize);

    if (!ArchiveSupport::isArchivePath(path) && !QFileInfo::exists(path)) {
        if (size) {
            *size = cacheSize;
        }
        return transparentImage(cacheSize);
    }

    QString cacheKey = originalPath + QStringLiteral("::")
                    + QString::number(cacheSize.width())
                    + QStringLiteral("x")
                    + QString::number(cacheSize.height())
                    + (coverOnly ? QStringLiteral("::cover") : QString());
    {
        QMutexLocker locker(&m_cacheMutex);
        if (QImage *cached = m_cache.object(cacheKey)) {
            if (size) {
                *size = cached->size();
            }
            if (thumbnailTimingEnabled()) {
                qInfo().noquote()
                    << "[ThumbnailProvider] hit"
                    << "ms=" << totalTimer.elapsed()
                    << "size=" << QStringLiteral("%1x%2").arg(targetSize.width()).arg(targetSize.height())
                    << "bucket=" << QStringLiteral("%1x%2").arg(cacheSize.width()).arg(cacheSize.height())
                    << "path=" << originalPath;
            }
            return *cached;
        }
    }

    QImage thumb;
    QString stage = QStringLiteral("none");
    qint64 stageMs = 0;
    QFileInfo fi(path);
    QString suffix = fi.suffix().toLower();
    std::unique_ptr<FileProvider> provider;
    std::unique_ptr<QIODevice> archiveDevice;
    QTemporaryFile tempFile;

    if (ArchiveSupport::isArchivePath(path)) {
        const QString archiveName = ArchiveSupport::archiveFileName(path);
        const QString archiveSuffix = QFileInfo(archiveName).suffix().toLower();
        if (coverOnly && archiveSuffix == QStringLiteral("fb2")) {
            std::unique_ptr<FileProvider> archiveProvider = FileProviderFactory::createProvider(path);
            std::unique_ptr<QIODevice> device = archiveProvider ? archiveProvider->openRead(path) : nullptr;
            if (device) {
                QImage cover = extractFb2CoverArt(device.get());
                if (!cover.isNull()) {
                    thumb = cover.scaled(cacheSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    stage = QStringLiteral("archive-fb2-cover");
                }
            }
        }

        if (thumb.isNull()) {
            if (size) {
                *size = QSize();
            }
            if (thumbnailTimingEnabled()) {
                qInfo().noquote()
                    << "[ThumbnailProvider] skip-archive-container"
                    << "ms=" << totalTimer.elapsed()
                    << "path=" << originalPath;
            }
            return {};
        }
    }

    if (thumb.isNull() && ArchiveSupport::isArchiveFilePath(path)) {
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
    if (thumb.isNull() && suffix == "fb2") {
        QElapsedTimer stageTimer;
        if (thumbnailTimingEnabled()) {
            stageTimer.start();
        }
        QImage cover = extractFb2CoverArt(path);
        if (!cover.isNull()) {
            thumb = cover.scaled(cacheSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        stage = QStringLiteral("fb2-cover");
        stageMs = thumbnailTimingEnabled() ? stageTimer.elapsed() : 0;
    }
    // 1. SVG
    else if (suffix == "svg" || suffix == "svgz") {
        QElapsedTimer stageTimer;
        if (thumbnailTimingEnabled()) {
            stageTimer.start();
        }
        QSvgRenderer renderer(path);
        if (renderer.isValid()) {
            thumb = QImage(cacheSize, QImage::Format_ARGB32_Premultiplied);
            thumb.fill(Qt::transparent);
            QPainter p(&thumb);
            renderer.render(&p);
        }
        stage = QStringLiteral("svg");
        stageMs = thumbnailTimingEnabled() ? stageTimer.elapsed() : 0;
    }
    // 2. Font
    else if (suffix == "ttf" || suffix == "otf" || suffix == "woff" || suffix == "woff2") {
        QElapsedTimer stageTimer;
        if (thumbnailTimingEnabled()) {
            stageTimer.start();
        }
        QRawFont rawFont(path, cacheSize.height() * 0.4);
        if (rawFont.isValid()) {
            thumb = QImage(cacheSize, QImage::Format_ARGB32_Premultiplied);
            thumb.fill(Qt::transparent);
            QPainter p(&thumb);
            p.setRenderHint(QPainter::Antialiasing);

            const qreal inset = qMax<qreal>(4.0, qMin(cacheSize.width(), cacheSize.height()) * 0.08);
            const QRectF paperRect = QRectF(QPointF(inset, inset),
                                            QSizeF(cacheSize.width() - inset * 2,
                                                   cacheSize.height() - inset * 2));
            p.setPen(QPen(QColor(210, 214, 220, 190), qMax<qreal>(1.0, inset * 0.12)));
            p.setBrush(QColor("#F8FAFC"));
            p.drawRoundedRect(paperRect, inset * 0.7, inset * 0.7);
            
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
            p.translate(paperRect.center().x() - bounds.width() / 2.0 - bounds.x(),
                        paperRect.center().y() - bounds.height() / 2.0 - bounds.y());
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#111827"));
            p.drawPath(pathObj);
        }
        stage = QStringLiteral("font");
        stageMs = thumbnailTimingEnabled() ? stageTimer.elapsed() : 0;
    }
#ifdef HAS_QT_PDF
    // 2B. PDF (via QPdfDocument)
    else if (suffix == "pdf") {
        QElapsedTimer stageTimer;
        if (thumbnailTimingEnabled()) {
            stageTimer.start();
        }
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
        stage = QStringLiteral("pdf");
        stageMs = thumbnailTimingEnabled() ? stageTimer.elapsed() : 0;
    }
#else
    else if (suffix == "pdf") {
        // Fallback to image reader/shell
    }
#endif
    // 2C. Audio files (via TagLib)
    else if (isAudioSuffix(suffix)) {
#ifdef HAS_TAGLIB
        QElapsedTimer stageTimer;
        if (thumbnailTimingEnabled()) {
            stageTimer.start();
        }
        QImage cover = extractCoverArt(path);
        if (!cover.isNull()) {
            thumb = cover.scaled(cacheSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        stage = QStringLiteral("taglib-cover");
        stageMs = thumbnailTimingEnabled() ? stageTimer.elapsed() : 0;
#endif
    }
    // 3. Image (via QImageReader)
    else {
        QElapsedTimer stageTimer;
        if (thumbnailTimingEnabled()) {
            stageTimer.start();
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
            thumb = reader.read();
        }
        stage = QStringLiteral("image-reader");
        stageMs = thumbnailTimingEnabled() ? stageTimer.elapsed() : 0;
    }
    
    // 4. Fallback to Windows Shell (for video, PDF, Office, etc.)
#ifdef Q_OS_WIN
    if (!coverOnly && thumb.isNull() && !fi.isDir()) {
        QElapsedTimer stageTimer;
        if (thumbnailTimingEnabled()) {
            stageTimer.start();
        }
        thumb = WinThumbnailExtractor::extract(path, cacheSize);
        stage = QStringLiteral("win-shell");
        stageMs = thumbnailTimingEnabled() ? stageTimer.elapsed() : 0;
    }
#endif
    
    if (!thumb.isNull()) {
        const int costKb = qMax(1, int((thumb.sizeInBytes() + 1023) / 1024));
        QMutexLocker locker(&m_cacheMutex);
        m_cache.insert(cacheKey, new QImage(thumb), costKb);
        if (size) {
            *size = thumb.size();
        }
        if (thumbnailTimingEnabled()) {
            qInfo().noquote()
                << "[ThumbnailProvider] miss"
                << "totalMs=" << totalTimer.elapsed()
                << "stageMs=" << stageMs
                << "stage=" << stage
                << "suffix=" << suffix
                << "target=" << QStringLiteral("%1x%2").arg(targetSize.width()).arg(targetSize.height())
                << "bucket=" << QStringLiteral("%1x%2").arg(cacheSize.width()).arg(cacheSize.height())
                << "result=" << QStringLiteral("%1x%2").arg(thumb.width()).arg(thumb.height())
                << "path=" << originalPath;
        }
        return thumb;
    }

    if (coverOnly && isAudioSuffix(suffix)) {
        thumb = audioCoverPlaceholder(cacheSize);
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
    if (thumbnailTimingEnabled()) {
        qInfo().noquote()
            << "[ThumbnailProvider] miss-null"
            << "totalMs=" << totalTimer.elapsed()
            << "stageMs=" << stageMs
            << "stage=" << stage
            << "suffix=" << suffix
            << "target=" << QStringLiteral("%1x%2").arg(targetSize.width()).arg(targetSize.height())
            << "bucket=" << QStringLiteral("%1x%2").arg(cacheSize.width()).arg(cacheSize.height())
            << "path=" << originalPath;
    }
    return QImage();
}
