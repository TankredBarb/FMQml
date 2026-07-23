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
bool readFileRangeAsAdministrator(const QString &path, qint64 offset, qint64 length,
                                  QByteArray *data, qint64 *totalSize)
{
#ifdef Q_OS_LINUX
    LinuxAdminBroker::Request request;
    request.operation = LinuxAdminBroker::Operation::ReadFile;
    request.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.sessionNonce = LinuxAdminBroker::activeSessionNonce();
    request.sourcePath = path;
    request.offset = offset;
    request.length = length;
    if (request.sessionNonce.isEmpty()) {
        return false;
    }
    LinuxAdminBroker broker;
    const LinuxAdminBroker::Result result = broker.submitBlocking(request);
    if (qEnvironmentVariableIntValue("FM_ADMIN_TRACE") != 0) {
        qInfo().noquote() << "[AdminTrace] quicklook-read"
                          << "path=" << QDir::toNativeSeparators(path)
                          << "offset=" << offset
                          << "length=" << length
                          << "success=" << result.success
                          << "code=" << result.errorCode
                          << "message=" << result.errorMessage
                          << "dataBytes=" << result.data.size()
                          << "totalSize=" << result.totalSize;
    }
    if (!result.success) {
        return false;
    }
    *data = result.data;
    *totalSize = result.totalSize;
    return true;
#else
    Q_UNUSED(path)
    Q_UNUSED(offset)
    Q_UNUSED(length)
    Q_UNUSED(data)
    Q_UNUSED(totalSize)
    return false;
#endif
}

LocalPreviewData loadLocalPreviewData(const QString &path)
{
    LocalPreviewData data;
    QLocale loc;
    const QFileInfo info(path);
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(path);

    data.name = info.fileName();
    if (data.name.isEmpty()) {
        data.name = cheapFileName(path);
    }
    data.extension = info.suffix().toLower();
    data.directory = info.isDir();
    data.hidden = info.isHidden();
    data.symlink = info.isSymLink();
    data.absolutePath = info.absoluteFilePath();
    data.parentPath = info.absolutePath();
    data.readable = info.isReadable();
    data.writable = info.isWritable();
    data.executable = info.isExecutable();
    data.mimeName = mime.name();
    data.sizeText = data.directory
        ? QStringLiteral("Folder")
        : DriveUtils::formatSize(info.size());
    data.modifiedText = loc.toString(info.lastModified(), QLocale::ShortFormat);

    const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
    data.hidden = capabilities.attributes.hidden;
    if (capabilities.isDirectory) {
        data.readable = capabilities.access.canBrowse;
        data.writable = capabilities.access.canCreateChildren;
        data.executable = capabilities.access.canTraverse;
    } else {
        data.readable = capabilities.access.canRead;
        data.writable = capabilities.access.canModify;
        data.executable = capabilities.access.canExecute;
    }
    data.permissionsText = capabilities.accessSummary;
    data.attributesText = capabilities.attributesSummary;

    const bool isDriveRoot = info.isRoot();
    if (data.directory) {
        data.mimeName = QStringLiteral("inode/directory");
        data.type = QStringLiteral("info");
        if (isDriveRoot) {
            QStorageInfo storage(path);
            if (storage.isValid()) {
                const qint64 total = storage.bytesTotal();
                const qint64 free  = storage.bytesFree();
                const qint64 used  = total - free;

                data.mimeName = QStringLiteral("drive");
                data.extension = DriveUtils::detectDriveType(storage);
                QString driveName = path;
                while (driveName.endsWith(QChar('/')) || driveName.endsWith(QChar('\\'))) {
                    driveName.chop(1);
                }
                if (!driveName.isEmpty()) {
                    data.name = driveName;
                }
                data.sizeText = DriveUtils::formatSize(total);
                data.modifiedText = total > 0
                    ? QStringLiteral("%1% free").arg(static_cast<int>(free * 100 / total))
                    : QStringLiteral("no media");
                data.extraProperties.append(prop(QStringLiteral("File System"), QString::fromLatin1(storage.fileSystemType())));
                data.extraProperties.append(prop(QStringLiteral("Total Space"), DriveUtils::formatSize(total)));
                data.extraProperties.append(prop(QStringLiteral("Free Space"),  DriveUtils::formatSize(free)));
                data.extraProperties.append(prop(QStringLiteral("Used Space"),  DriveUtils::formatSize(used)));
                if (total > 0) {
                    data.extraProperties.append(prop(QStringLiteral("Usage"), QStringLiteral("%1%").arg(static_cast<int>(used * 100 / total))));
                }
                data.extraProperties.append(prop(QStringLiteral("Drive Type"), data.extension));
            }
        }
        data.content = QStringLiteral("Folder: %1\nSize: %2\nModified: %3")
            .arg(data.name, data.sizeText, data.modifiedText);
        return data;
    }

    const bool isSvg = mime.name() == QStringLiteral("image/svg+xml")
        || data.extension == QStringLiteral("svg")
        || data.extension == QStringLiteral("svgz");
    const bool isImage = isPreviewableRasterImage(data.extension, mime.name());
    const bool isImageMetadataFile = isImage && !isSvg;
    data.requestMetadata = !isImageMetadataFile;

    if (isSvg) {
        data.type = QStringLiteral("svg");
        data.content = path;
    } else if (isImage) {
        data.type = QStringLiteral("image");
        data.content = path;
        data.requestImageMetadata = true;
    } else if (mime.name() == QStringLiteral("application/pdf") || data.extension == QStringLiteral("pdf")) {
        data.type = QStringLiteral("pdf");
        data.content = path;
    } else if (data.extension == QStringLiteral("ttf") || data.extension == QStringLiteral("otf")
               || data.extension == QStringLiteral("woff") || data.extension == QStringLiteral("woff2")
               || (data.extension != QStringLiteral("fon")
                   && (mime.name() == QStringLiteral("font/ttf") || mime.name() == QStringLiteral("font/otf")
                       || mime.name() == QStringLiteral("application/font-woff") || mime.name() == QStringLiteral("font/woff2")))) {
        data.type = QStringLiteral("font");
        data.content = path;
    } else if (data.extension == QStringLiteral("exe") || data.extension == QStringLiteral("dll") || data.extension == QStringLiteral("msi")) {
        data.type = QStringLiteral("executable");
        data.content = path;
    } else if (data.extension == QStringLiteral("lnk")) {
        data.type = QStringLiteral("shortcut");
        data.content = path;
    } else if (isFb2Suffix(data.extension) || isFb2ZipPath(path)) {
        const bool fb2Zip = isFb2ZipPath(path);
#ifdef HAS_UNOFFICIAL_BIT7Z
        const Fb2PreviewData fb2 = fb2Zip
            ? loadFb2ZipPreviewData(path, false)
            : loadFb2PreviewData(path, false);
#else
        const Fb2PreviewData fb2 = loadFb2PreviewData(path, false);
#endif
        data.type = QStringLiteral("book");
        data.mimeName = fb2Zip
            ? QStringLiteral("application/x-fictionbook+zip")
            : QStringLiteral("application/x-fictionbook+xml");
        if (fb2Zip) {
            data.extension = QStringLiteral("fb2.zip");
        }
        data.content = fb2.content;
        data.extraProperties = fb2.extraProperties;
        data.bookPages = fb2.pages;
        data.bookParagraphs = fb2.paragraphs;
        data.bookCoverSource = fb2.coverSource;
        data.bookTitle = fb2.title;
        data.bookAuthor = fb2.author;
        data.lines = fb2.lines;
        data.bookPageIndex = fb2.pageIndex;
        data.requestMetadata = false;
    } else if (isEpubSuffix(data.extension)) {
        const EpubPreviewData epub = loadEpubPreviewData(path, false);
        data.type = QStringLiteral("book");
        data.mimeName = QStringLiteral("application/epub+zip");
        data.content = epub.content;
        data.extraProperties = epub.extraProperties;
        data.bookPages = epub.pages;
        data.bookParagraphs = epub.paragraphs;
        data.bookCoverSource = epub.coverSource;
        data.bookTitle = epub.title;
        data.bookAuthor = epub.author;
        data.lines = epub.lines;
        data.bookPageIndex = epub.pageIndex;
        data.requestMetadata = false;
    } else if (isOfficeDocumentSuffix(data.extension)) {
        data.type = QStringLiteral("info");
        data.mimeName = officeDocumentMimeLabel(data.extension);
        data.content = QStringLiteral("Name: %1\nSize: %2 bytes\nModified: %3")
            .arg(data.name)
            .arg(info.size())
            .arg(info.lastModified().toString());
    } else if (mime.name().startsWith(QStringLiteral("text/")) || mime.inherits(QStringLiteral("text/plain"))
               || mime.inherits(QStringLiteral("application/json")) || mime.inherits(QStringLiteral("application/javascript"))
               || mime.inherits(QStringLiteral("application/xml")) || isTextSuffix(data.extension)) {
        data.type = QStringLiteral("text");
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray raw = file.read(kTextPreviewLimit);
            data.content = QString::fromUtf8(raw);
            data.lines = data.content.count(QLatin1Char('\n')) + 1;
            if (file.size() > kTextPreviewLimit) {
                data.textTruncated = true;
                data.fullTextAvailable = true;
                if (!data.content.isEmpty() && !data.content.endsWith(QLatin1Char('\n'))) {
                    data.content.append(QLatin1Char('\n'));
                }
                data.content.append(QStringLiteral("..."));
            }
        } else {
            QByteArray raw;
            qint64 fileSize = 0;
            if (readFileRangeAsAdministrator(path, 0, kTextPreviewLimit, &raw, &fileSize)) {
                data.content = QString::fromUtf8(raw);
                data.lines = data.content.isEmpty()
                    ? 0
                    : data.content.count(QLatin1Char('\n')) + 1;
                data.readable = true;
                data.sizeText = DriveUtils::formatSize(fileSize);
                if (fileSize > kTextPreviewLimit) {
                    data.textTruncated = true;
                    data.fullTextAvailable = true;
                    if (!data.content.isEmpty() && !data.content.endsWith(QLatin1Char('\n'))) {
                        data.content.append(QLatin1Char('\n'));
                    }
                    data.content.append(QStringLiteral("..."));
                }
            } else {
                data.content = QStringLiteral("Cannot read file.");
            }
        }
    } else if (mime.name().startsWith(QStringLiteral("audio/"))) {
        data.type = QStringLiteral("audio");
        data.content = path;
    } else if (mime.name().startsWith(QStringLiteral("video/"))) {
        data.type = QStringLiteral("video");
        data.content = path;
    } else if (mime.inherits(QStringLiteral("application/zip"))
               || mime.inherits(QStringLiteral("application/x-tar"))
               || mime.inherits(QStringLiteral("application/x-7z-compressed"))
               || mime.inherits(QStringLiteral("application/x-rar-compressed"))) {
        data.type = QStringLiteral("archive");
        data.content = path;
    } else {
        data.type = QStringLiteral("info");
        data.content = QStringLiteral("Name: %1\nSize: %2 bytes\nModified: %3")
            .arg(data.name)
            .arg(info.size())
            .arg(info.lastModified().toString());
    }

    return data;
}


} // namespace PreviewInternal
