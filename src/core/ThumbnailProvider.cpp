#include "ThumbnailProvider.h"
#include "../controllers/ThumbnailController.h"
#include "ArchiveSupport.h"
#include "FileProviderFactory.h"
#include "FileProviderPluginRegistry.h"
#include "CleanupSubsystem.h"
#include "LinuxAdminBroker.h"
#include "LocalFileProvider.h"
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
#include <QDateTime>
#include <QRawFont>
#include <QSet>
#include <QXmlStreamReader>

#ifdef Q_OS_WIN
#include "WinThumbnailExtractor.h"
#endif

#ifdef HAS_FFMPEG_THUMBNAILS
#include "VideoThumbnailExtractor.h"
#endif

#ifdef HAS_QT_PDF
#include <QPdfDocument>
#endif

#include <QUrl>
#include <QDir>
#include <QTemporaryFile>
#include <QThread>
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
constexpr qint64 kAdminThumbnailMaterializationLimit = 512LL * 1024 * 1024;

bool thumbnailTimingEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_THUMBNAIL_TIMING");
    return enabled;
}

bool thumbnailTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_THUMBNAIL_TRACE");
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

bool isAudioCoverSuffix(const QString &suffix)
{
    static const QSet<QString> suffixes = {
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"), QStringLiteral("oga"),
        QStringLiteral("m4a"), QStringLiteral("m4b")
    };
    return suffixes.contains(suffix.toLower());
}

bool isVideoSuffix(const QString &suffix)
{
    static const QSet<QString> suffixes = {
        QStringLiteral("3g2"), QStringLiteral("3gp"), QStringLiteral("asf"), QStringLiteral("avi"),
        QStringLiteral("divx"), QStringLiteral("flv"), QStringLiteral("m2ts"), QStringLiteral("m4v"),
        QStringLiteral("mkv"), QStringLiteral("mov"), QStringLiteral("mp4"), QStringLiteral("mpeg"),
        QStringLiteral("mpg"), QStringLiteral("mts"), QStringLiteral("ogv"), QStringLiteral("ts"),
        QStringLiteral("webm"), QStringLiteral("wmv")
    };
    return suffixes.contains(suffix.toLower());
}

bool shouldAttemptVideoThumbnail(const QString &suffix)
{
    return isVideoSuffix(suffix);
}

QString localFileIdentitySuffix(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return {};
    }
    return QStringLiteral("::mtime=%1::bytes=%2")
        .arg(info.lastModified().toMSecsSinceEpoch())
        .arg(info.size());
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
QImage extractCoverArt(const QString &path, const QString &suffix)
{
#ifdef Q_OS_WIN
    const wchar_t *wpath = reinterpret_cast<const wchar_t *>(path.utf16());
#else
    QByteArray utf8Path = path.toUtf8();
    const char *wpath = utf8Path.constData();
#endif

    QImage img;

    // 1. Check for MP3 / MPEG (ID3v2 APIC frame)
    if (suffix == QLatin1String("mp3")) {
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
    if (suffix == QLatin1String("flac")) {
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
    if (suffix == QLatin1String("mp4") || suffix == QLatin1String("m4a") || suffix == QLatin1String("m4b")) {
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
    if (suffix == QLatin1String("ogg") || suffix == QLatin1String("oga")) {
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

QString thumbnailLookupPath(QString path)
{
    const QString marker = QStringLiteral("::thumbrev=");
    const int markerIndex = path.lastIndexOf(marker);
    if (markerIndex < 0) {
        return path;
    }

    bool ok = false;
    path.mid(markerIndex + marker.size()).toInt(&ok);
    if (ok) {
        path.truncate(markerIndex);
    }
    return path;
}

ThumbnailProvider::ThumbnailProvider(ThumbnailController *controller)
    : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_cache(kThumbnailCacheLimitKb)
    , m_controller(controller)
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
    const QString decodedLookupPath = thumbnailLookupPath(decodedPath);
    const bool decodedInstagramPath = decodedLookupPath.trimmed().startsWith(QStringLiteral("instagram://"), Qt::CaseInsensitive);
    const bool decodedProviderPath = decodedInstagramPath || FileProviderFactory::hasPluginProviderForPath(decodedLookupPath);
    QString originalPath = ArchiveSupport::isArchivePath(decodedLookupPath)
        ? QDir::fromNativeSeparators(decodedLookupPath)
        : (decodedProviderPath
            ? decodedLookupPath.trimmed()
            : QDir::toNativeSeparators(decodedLookupPath));
    const QString cachePath = ArchiveSupport::isArchivePath(decodedPath)
        ? QDir::fromNativeSeparators(decodedPath)
        : (decodedProviderPath ? decodedPath.trimmed() : QDir::toNativeSeparators(decodedPath));
    const bool coverOnly = originalPath.endsWith(QStringLiteral("::cover"));
    if (coverOnly) {
        originalPath.chop(7);
    }
    QString path = originalPath;
    const bool instagramPath = path.startsWith(QStringLiteral("instagram://"), Qt::CaseInsensitive);
    const bool providerPath = instagramPath || FileProviderFactory::hasPluginProviderForPath(path);
    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(128, 128);
    const QSize cacheSize = bucketSize(targetSize);
    QString cacheKey = cachePath + QStringLiteral("::")
                    + QString::number(cacheSize.width())
                    + QStringLiteral("x")
                    + QString::number(cacheSize.height())
                    + (coverOnly ? QStringLiteral("::cover") : QString())
                    + ((!ArchiveSupport::isArchivePath(path) && !providerPath) ? localFileIdentitySuffix(path) : QString());

    if (thumbnailTraceEnabled()) {
        qInfo().noquote() << "[ThumbnailTrace] request"
                          << "path=" << originalPath
                          << "provider=" << providerPath
                          << "bucket=" << QStringLiteral("%1x%2").arg(cacheSize.width()).arg(cacheSize.height());
    }

    if (!coverOnly && providerPath) {
        const QString thumbnailUrl = FileProviderPluginRegistry::instance().thumbnailUrlForPath(path);
        const QUrl url(thumbnailUrl);
        if (url.isLocalFile()) {
            QImageReader reader(url.toLocalFile());
            reader.setAutoTransform(true);
            if (reader.canRead()) {
                const QSize imageSize = reader.size();
                if (imageSize.isValid()) {
                    QSize thumbSize = imageSize;
                    thumbSize.scale(cacheSize, Qt::KeepAspectRatio);
                    reader.setScaledSize(thumbSize);
                }
                const QImage thumb = reader.read();
                if (!thumb.isNull()) {
                    const int costKb = qMax(1, int((thumb.sizeInBytes() + 1023) / 1024));
                    QMutexLocker locker(&m_cacheMutex);
                    m_cache.insert(cacheKey, new QImage(thumb), costKb);
                    if (size) {
                        *size = thumb.size();
                    }
                    return thumb;
                }
            }
        }
    }

    // Cover-only request for a provider path: return a transparent 1×1 image.
    if (coverOnly && providerPath) {
        QImage transparent = transparentImage(QSize(1, 1));
        if (size) {
            *size = transparent.size();
        }
        return transparent;
    }

    // Provider path: try the native provider thumbnail
    // contract (ProviderThumbnailResult). The previous local-URL fast path
    // already ran above. This branch never copies the original file: it only
    // consumes provider-native thumbnail bytes/file the provider exposes.
    if (!coverOnly && providerPath) {
        if (m_controller) {
            const ThumbnailController::Lookup lookup = m_controller->providerThumbnail(path, cacheSize, 100);
            if (lookup.state == ThumbnailController::State::Ready) {
                if (thumbnailTraceEnabled()) {
                    qInfo().noquote() << "[ThumbnailTrace] provider-ready" << "path=" << originalPath;
                }
                if (size) {
                    *size = lookup.image.size();
                }
                return lookup.image;
            }
            if (lookup.state != ThumbnailController::State::Ready) {
                if (thumbnailTraceEnabled()) {
                    qInfo().noquote() << "[ThumbnailTrace] provider-soft-miss"
                                      << "path=" << originalPath
                                      << "state=" << static_cast<int>(lookup.state);
                }
                const QImage softMiss = transparentImage(QSize(1, 1));
                if (size) {
                    *size = softMiss.size();
                }
                return softMiss;
            }
        }

        // Provider work belongs to ThumbnailController. A bootstrap without
        // that service must fall back to the file-type icon rather than run
        // network or SDK work on the image-provider thread.
        const QImage softMiss = transparentImage(QSize(1, 1));
        if (size) {
            *size = softMiss.size();
        }
        return softMiss;
    }

    // Provider path without a local thumbnail: return empty immediately.
    // thumbnailUrlForPath() above already tried the plugin's local thumbnail
    // URL. If we're still here, no native thumbnail is available. Production
    // thumbnail requests must never materialize the original provider file;
    // QML shows the fallback file-type icon instead.
    if (providerPath) {
        if (size) {
            *size = QSize(0, 0);
        }
        return {};
    }

    if (!ArchiveSupport::isArchivePath(path) && !providerPath && !QFileInfo::exists(path)) {
#ifdef Q_OS_LINUX
        const bool adminSessionActive = !LinuxAdminBroker::activeSessionNonce().isEmpty();
        if (adminSessionActive) {
            const QString suffix = QFileInfo(path).suffix();
            const QString root = StagingLocationPolicy::defaultCleanupRoot();
            const QString base = root.isEmpty()
                ? QDir::tempPath()
                : QDir(root).filePath(QStringLiteral("admin-thumbnails"));
            QDir().mkpath(base);
            const QString pattern = QDir(base).filePath(
                QStringLiteral("fm-admin-thumb-XXXXXX")
                + (suffix.isEmpty() ? QString() : QString(QLatin1Char('.')) + suffix));
            QTemporaryFile staged(pattern);
            staged.setAutoRemove(true);
            if (staged.open()) {
                const QString stagedPath = staged.fileName();
                staged.close();
                LocalFileProvider localProvider;
                QString error;
                bool tooLarge = false;
                const bool copied = localProvider.copyToLocalFileForPreview(
                    path,
                    stagedPath,
                    [&tooLarge](qint64 processed, qint64) {
                        if (processed > kAdminThumbnailMaterializationLimit) {
                            tooLarge = true;
                            return false;
                        }
                        return true;
                    },
                    &error);
                if (thumbnailTraceEnabled()) {
                    qInfo().noquote() << "[ThumbnailTrace] admin-stage"
                                      << "path=" << originalPath
                                      << "suffix=" << suffix
                                      << "copied=" << copied
                                      << "tooLarge=" << tooLarge
                                      << "error=" << error;
                }
                if (copied && !tooLarge) {
                    const QString stagedId = QString::fromUtf8(QUrl::toPercentEncoding(stagedPath));
                    const QImage adminThumb = requestImage(stagedId, size, requestedSize);
                    if (!adminThumb.isNull()) {
                        const int costKb = qMax(1, int((adminThumb.sizeInBytes() + 1023) / 1024));
                        QMutexLocker locker(&m_cacheMutex);
                        m_cache.insert(cacheKey, new QImage(adminThumb), costKb);
                        return adminThumb;
                    }
                }
            }

            // The staged path can legitimately have no visual preview (for
            // example, an MP3 without embedded cover art). Preserve that miss
            // so QML shows the ordinary file-type icon instead of a blank
            // transparent thumbnail.
            if (size) {
                *size = QSize(0, 0);
            }
            QMutexLocker locker(&m_cacheMutex);
            m_negativeCache.insert(cacheKey);
            return {};
        }
#endif
        if (size) {
            *size = cacheSize;
        }
        return transparentImage(cacheSize);
    }

    {
        QMutexLocker locker(&m_cacheMutex);
        if (m_negativeCache.contains(cacheKey)) {
            if (size) {
                *size = QSize(0, 0);
            }
            if (thumbnailTimingEnabled()) {
                qInfo().noquote()
                    << "[ThumbnailProvider] negative-hit"
                    << "ms=" << totalTimer.elapsed()
                    << "size=" << QStringLiteral("%1x%2").arg(targetSize.width()).arg(targetSize.height())
                    << "bucket=" << QStringLiteral("%1x%2").arg(cacheSize.width()).arg(cacheSize.height())
                    << "path=" << originalPath;
            }
            return {};
        }
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
    if (const QImage cached = m_diskCache.load(cacheKey); !cached.isNull()) {
        const int costKb = qMax(1, int((cached.sizeInBytes() + 1023) / 1024));
        QMutexLocker locker(&m_cacheMutex);
        m_cache.insert(cacheKey, new QImage(cached), costKb);
        if (size) {
            *size = cached.size();
        }
        if (thumbnailTimingEnabled()) {
            qInfo().noquote()
                << "[ThumbnailProvider] disk-hit"
                << "ms=" << totalTimer.elapsed()
                << "path=" << originalPath;
        }
        return cached;
    }

    QImage thumb;
    QString stage = QStringLiteral("none");
    qint64 stageMs = 0;
    QFileInfo fi(path);
    QString suffix = fi.suffix().toLower();
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

    if (ArchiveSupport::isArchiveFilePath(path)) {
        if (size) {
            *size = QSize();
        }
        if (thumbnailTimingEnabled()) {
            qInfo().noquote()
                << "[ThumbnailProvider] skip-local-archive-container"
                << "ms=" << totalTimer.elapsed()
                << "path=" << originalPath;
        }
        return {};
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
    else if (isAudioCoverSuffix(suffix)) {
#ifdef HAS_TAGLIB
        QElapsedTimer stageTimer;
        if (thumbnailTimingEnabled()) {
            stageTimer.start();
        }
        QImage cover = extractCoverArt(path, suffix);
        if (!cover.isNull()) {
            thumb = cover.scaled(cacheSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        stage = QStringLiteral("taglib-cover");
        stageMs = thumbnailTimingEnabled() ? stageTimer.elapsed() : 0;
#endif
    }
    if (!coverOnly && thumb.isNull() && shouldAttemptVideoThumbnail(suffix)) {
#ifdef HAS_FFMPEG_THUMBNAILS
        QElapsedTimer stageTimer;
        if (thumbnailTimingEnabled()) {
            stageTimer.start();
        }
        const VideoThumbnailResult videoResult = VideoThumbnailExtractor::extract({ path, cacheSize, -1 });
        if (!videoResult.image.isNull()) {
            thumb = videoResult.image;
        }
        stage = QStringLiteral("ffmpeg-video");
        stageMs = thumbnailTimingEnabled() ? stageTimer.elapsed() : 0;
        if (thumbnailTimingEnabled()) {
            if (thumb.isNull()) {
                qInfo().noquote()
                    << "[ThumbnailProvider] ffmpeg-video-null"
                    << "stageMs=" << stageMs
                    << "suffix=" << suffix
                    << "timestampMs=" << videoResult.timestampMs
                    << "stream=" << videoResult.streamIndex
                    << "source=" << QStringLiteral("%1x%2").arg(videoResult.sourceSize.width()).arg(videoResult.sourceSize.height())
                    << "error=" << videoResult.error
                    << "path=" << originalPath;
            } else {
                qInfo().noquote()
                    << "[ThumbnailProvider] ffmpeg-video"
                    << "stageMs=" << stageMs
                    << "suffix=" << suffix
                    << "timestampMs=" << videoResult.timestampMs
                    << "stream=" << videoResult.streamIndex
                    << "source=" << QStringLiteral("%1x%2").arg(videoResult.sourceSize.width()).arg(videoResult.sourceSize.height())
                    << "result=" << QStringLiteral("%1x%2").arg(thumb.width()).arg(thumb.height())
                    << "path=" << originalPath;
            }
        }
#endif
    }
    // 3. Image (via QImageReader)
    if (thumb.isNull()) {
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
        m_diskCache.store(cacheKey, thumb);
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
        thumb = transparentImage(QSize(1, 1));
        QMutexLocker locker(&m_cacheMutex);
        m_cache.insert(cacheKey, new QImage(thumb), 1);
        if (size) {
            *size = thumb.size();
        }
        return thumb;
    }

    if (size) {
        *size = QSize(0, 0);
    }
    {
        QMutexLocker locker(&m_cacheMutex);
        m_negativeCache.insert(cacheKey);
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
