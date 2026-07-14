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
bool audioCoverCandidateSuffix(const QString &suffix)
{
    const QString normalized = suffix.toLower();
    return normalized == QLatin1String("mp3")
        || normalized == QLatin1String("flac")
        || normalized == QLatin1String("m4a")
        || normalized == QLatin1String("m4b")
        || normalized == QLatin1String("mp4")
        || normalized == QLatin1String("ogg")
        || normalized == QLatin1String("oga");
}

#ifdef HAS_TAGLIB
QImage imageFromTagLibBytes(const TagLib::ByteVector &data)
{
    if (data.isEmpty()) {
        return {};
    }
    return QImage::fromData(reinterpret_cast<const uchar *>(data.data()), static_cast<int>(data.size()));
}

QImage extractAudioCoverArt(const QString &path)
{
#ifdef Q_OS_WIN
    const wchar_t *wpath = reinterpret_cast<const wchar_t *>(path.utf16());
#else
    const QByteArray utf8Path = path.toUtf8();
    const char *wpath = utf8Path.constData();
#endif

    {
        TagLib::MPEG::File file(wpath);
        if (file.isValid() && file.ID3v2Tag()) {
            const auto &frameMap = file.ID3v2Tag()->frameListMap();
            if (frameMap.contains("APIC")) {
                const auto &frames = frameMap["APIC"];
                for (auto *frame : frames) {
                    auto *picture = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frame);
                    const QImage image = picture ? imageFromTagLibBytes(picture->picture()) : QImage();
                    if (!image.isNull()) {
                        return image;
                    }
                }
            }
        }
    }

    {
        TagLib::FLAC::File file(wpath);
        if (file.isValid()) {
            for (auto *picture : file.pictureList()) {
                const QImage image = picture ? imageFromTagLibBytes(picture->data()) : QImage();
                if (!image.isNull()) {
                    return image;
                }
            }
        }
    }

    {
        TagLib::MP4::File file(wpath);
        if (file.isValid() && file.tag()) {
            auto items = file.tag()->itemMap();
            if (items.contains("covr")) {
                const auto covers = items["covr"].toCoverArtList();
                for (const auto &cover : covers) {
                    const QImage image = imageFromTagLibBytes(cover.data());
                    if (!image.isNull()) {
                        return image;
                    }
                }
            }
        }
    }

    {
        TagLib::Vorbis::File file(wpath);
        if (file.isValid() && file.tag()) {
            const auto fields = file.tag()->fieldListMap();
            if (fields.contains("METADATA_BLOCK_PICTURE")) {
                const auto values = fields["METADATA_BLOCK_PICTURE"];
                for (const auto &value : values) {
                    const QByteArray decoded = QByteArray::fromBase64(QByteArray(value.toCString()));
                    const QImage image = QImage::fromData(decoded);
                    if (!image.isNull()) {
                        return image;
                    }
                }
            }
        }
    }

    return {};
}
#else
QImage extractAudioCoverArt(const QString &)
{
    return {};
}
#endif

QString materializeAudioCoverSource(const QString &audioPath, const QString &cleanupDir, const QString &suffix)
{
    if (!audioCoverCandidateSuffix(suffix) || cleanupDir.isEmpty()) {
        return {};
    }

    QImage cover = extractAudioCoverArt(audioPath);
    if (cover.isNull()) {
        return {};
    }

    const QSize maxCoverSize(1024, 1024);
    if (cover.width() > maxCoverSize.width() || cover.height() > maxCoverSize.height()) {
        cover = cover.scaled(maxCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const QString coverPath = QDir(cleanupDir).filePath(QStringLiteral("cover.png"));
    return cover.save(coverPath, "PNG")
        ? QUrl::fromLocalFile(coverPath).toString(QUrl::FullyEncoded)
        : QString{};
}


} // namespace PreviewInternal
