#include "QuickLookController.h"
#include <QFileInfo>
#include <QFileDevice>
#include <QFile>
#include <QByteArray>
#include <QMimeDatabase>
#include <QMimeType>
#include <QDateTime>
#include <QLocale>
#include <QStringList>
#include <QMetaObject>
#include <QPointer>
#include <QImageReader>
#include <QImage>
#include <QPixelFormat>
#include <QRegularExpression>
#include <QUrl>
#include <QTimer>
#include <QXmlStreamReader>
#include <QtConcurrent/QtConcurrentRun>
#include <memory>
#include <utility>
#include "../core/ArchiveFileProvider.h"
#include "../core/ArchiveSupport.h"
#include "../core/FileAccessResolver.h"
#include "../core/FileProviderFactory.h"
#include "../core/FileProviderPluginRegistry.h"
#include "../core/MetadataExtractor.h"
#include "../core/DriveUtils.h"
#include "../core/IsoMountManager.h"
#include "../core/LinuxAdminBroker.h"
#include "../core/CleanupSubsystem.h"
#include <QCoreApplication>
#include <QStorageInfo>
#include <QDir>
#include <QUuid>

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

#include "PreviewInternal.h"

namespace PreviewInternal {
bool jpegTailLooksComplete(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 2) {
        return false;
    }

    const qint64 tailSize = (std::min<qint64>)(file.size(), 4096);
    if (!file.seek(file.size() - tailSize)) {
        return false;
    }
    const QByteArray tail = file.read(tailSize);
    for (qsizetype i = tail.size() - 2; i >= 0; --i) {
        if (static_cast<uchar>(tail.at(i)) == 0xff
            && static_cast<uchar>(tail.at(i + 1)) == 0xd9) {
            return true;
        }
    }
    return false;
}

bool materializedImageLooksUsable(const QString &path, const QString &suffix, const QString &mimeType)
{
    const bool jpeg = suffix.compare(QStringLiteral("jpg"), Qt::CaseInsensitive) == 0
        || suffix.compare(QStringLiteral("jpeg"), Qt::CaseInsensitive) == 0
        || mimeType.compare(QStringLiteral("image/jpeg"), Qt::CaseInsensitive) == 0;
    if (jpeg && !jpegTailLooksComplete(path)) {
        return false;
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    return !image.isNull() && reader.error() == QImageReader::UnknownError;
}

bool materializedRemotePreviewLooksUsable(const QString &path, const FileEntry &entry)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.size() <= 0) {
        return false;
    }

    const QString suffix = materializedPreviewSuffix(entry);
    const QString mimeType = entry.isShortcut && !entry.shortcutTargetMimeType.isEmpty()
        ? entry.shortcutTargetMimeType
        : entry.mimeType;
    if (isPreviewableRasterImage(suffix, mimeType)) {
        return materializedImageLooksUsable(path, suffix, mimeType);
    }
    if (isVideoPreviewEntry(entry)) {
        return materializedVideoLooksUsable(path, suffix);
    }

    return true;
}

QString imageFormatName(QImage::Format format)
{
    switch (format) {
    case QImage::Format_Invalid: return {};
    case QImage::Format_Indexed8: return QStringLiteral("Indexed8");
    case QImage::Format_RGB32: return QStringLiteral("RGB32");
    case QImage::Format_ARGB32: return QStringLiteral("ARGB32");
    case QImage::Format_ARGB32_Premultiplied: return QStringLiteral("ARGB32 Premultiplied");
    case QImage::Format_RGB16: return QStringLiteral("RGB16");
    case QImage::Format_RGB888: return QStringLiteral("RGB888");
    case QImage::Format_RGBX8888: return QStringLiteral("RGBX8888");
    case QImage::Format_RGBA8888: return QStringLiteral("RGBA8888");
    case QImage::Format_RGBA8888_Premultiplied: return QStringLiteral("RGBA8888 Premultiplied");
    case QImage::Format_Alpha8: return QStringLiteral("Alpha8");
    case QImage::Format_Grayscale8: return QStringLiteral("Grayscale8");
    case QImage::Format_RGBX64: return QStringLiteral("RGBX64");
    case QImage::Format_RGBA64: return QStringLiteral("RGBA64");
    case QImage::Format_RGBA64_Premultiplied: return QStringLiteral("RGBA64 Premultiplied");
    case QImage::Format_Grayscale16: return QStringLiteral("Grayscale16");
    case QImage::Format_BGR888: return QStringLiteral("BGR888");
    case QImage::Format_CMYK8888: return QStringLiteral("CMYK8888");
    default: return QStringLiteral("Format %1").arg(static_cast<int>(format));
    }
}

bool quickLookCanConvertToPixelFormat(QImage::Format format)
{
    switch (format) {
    case QImage::Format_Indexed8:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_RGB16:
    case QImage::Format_RGB888:
    case QImage::Format_RGBX8888:
    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied:
    case QImage::Format_Alpha8:
    case QImage::Format_Grayscale8:
    case QImage::Format_RGBX64:
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
    case QImage::Format_Grayscale16:
    case QImage::Format_BGR888:
    case QImage::Format_CMYK8888:
        return true;
    default:
        return false;
    }
}

QString cheapFileName(QString path)
{
    path = QDir::fromNativeSeparators(path);
    while (path.length() > 1 && path.endsWith(QLatin1Char('/'))) {
        const bool driveRoot = path.length() == 3 && path.at(1) == QLatin1Char(':');
        if (driveRoot) {
            break;
        }
        path.chop(1);
    }

    const int slash = path.lastIndexOf(QLatin1Char('/'));
    const QString name = slash >= 0 ? path.mid(slash + 1) : path;
    return name.isEmpty() ? path : name;
}

ImageMetadataData loadImageMetadataData(const QString &path)
{
    ImageMetadataData data;

    QImageReader reader(path);
    reader.setAutoTransform(false);

    const QByteArray format = reader.format();
    if (!format.isEmpty()) {
        data.formatText = QString::fromLatin1(format).toUpper();
    }

    const QSize size = reader.size();
    if (size.isValid()) {
        data.width = size.width();
        data.height = size.height();
    }

    QImage::Format imageFormat = reader.imageFormat();
    if (quickLookCanConvertToPixelFormat(imageFormat)) {
        data.pixelFormatText = imageFormatName(imageFormat);
        const QPixelFormat pixelFormat = QImage::toPixelFormat(imageFormat);
        const int depth = pixelFormat.bitsPerPixel();
        if (depth > 0) {
            data.colorDepthText = QStringLiteral("%1 bit").arg(depth);
            data.alphaChannelText = pixelFormat.alphaUsage() == QPixelFormat::UsesAlpha
                ? QStringLiteral("Yes")
                : QStringLiteral("No");
        }
    }

    data.extraProperties = MetadataExtractor::extract(path);
    return data;
}


} // namespace PreviewInternal
