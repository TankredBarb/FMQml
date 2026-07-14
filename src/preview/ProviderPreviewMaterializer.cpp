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
LocalPreviewData loadProviderPreviewData(const QString &path)
{
    LocalPreviewData data;
    data.type = QStringLiteral("info");
    data.name = cheapFileName(path);
    data.absolutePath = path;
    data.readable = false;
    data.writable = false;
    data.executable = false;
    data.permissionsText = QStringLiteral("Remote provider");
    data.attributesText = QStringLiteral("Remote");

    std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
    if (!provider) {
        data.content = QStringLiteral("No provider is available for this path.");
        return data;
    }
    const bool localAdminProvider = provider->scheme() == QLatin1String("file");
    const bool googleDriveProvider = provider->scheme() == QLatin1String("gdrive");
    if (googleDriveProvider) {
        data.attributesText.clear();
    }

    const QString normalized = provider->normalizedPath(path);
    const std::optional<FileEntry> entry = provider->entryInfo(normalized);
    if (!entry) {
        data.content = QStringLiteral("Remote metadata is not available yet.");
        return data;
    }

    data.modifiedText = entry->modified.isValid() ? QLocale().toString(entry->modified, QLocale::ShortFormat) : QString();
    const bool fileShortcut = entry->isShortcut && !entry->shortcutTargetIsDirectory;
    data.sizeText = entry->isDirectory
        ? QStringLiteral("Folder")
        : (entry->size > 0 ? DriveUtils::formatSize(entry->size) : (fileShortcut ? QStringLiteral("Unknown size") : DriveUtils::formatSize(entry->size)));
    const QString entryMimeType = fileShortcut && !entry->shortcutTargetMimeType.isEmpty()
        ? entry->shortcutTargetMimeType
        : entry->mimeType;
    data.name = entry->name;
    data.extension = fileShortcut ? materializedPreviewSuffix(*entry) : entry->suffix;
    data.directory = entry->isDirectory;
    data.mimeName = entry->isDirectory
        ? QStringLiteral("inode/directory")
        : (entryMimeType.isEmpty() ? QStringLiteral("remote/file") : entryMimeType);
    data.absolutePath = normalized;
    data.parentPath = provider->parentPath(normalized);
    data.readable = !entry->isDirectory;
    data.writable = false;
    data.executable = false;
    data.attributesText = googleDriveProvider ? QString{} : entry->attributesText;
    data.permissionsText = localAdminProvider
        ? (entry->isDirectory ? QStringLiteral("Administrator browse (read-only)")
                              : QStringLiteral("Administrator read-only"))
        : googleDriveProvider
        ? googleDriveAccessSummary(*entry)
        : (entry->isDirectory ? QStringLiteral("Browse") : QStringLiteral("Read"));

    if (entry->isDirectory) {
        data.content = QStringLiteral("Folder: %1\nSize: %2\nModified: %3")
            .arg(data.name, data.sizeText, data.modifiedText);
        return data;
    }

    if (entry->size > kRemotePreviewMaterializeLimit) {
        data.content = remotePreviewTooLargeText(*entry);
        data.sizeText = QStringLiteral("Large file (%1)").arg(data.sizeText);
        return data;
    }

    const QString root = remotePreviewRoot(true);
    if (root.isEmpty()) {
        data.content = QStringLiteral("Cannot create remote preview staging folder.");
        return data;
    }

    const QString cleanupDir = QDir(root).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir().mkpath(cleanupDir)) {
        data.content = QStringLiteral("Cannot create remote preview staging folder.");
        return data;
    }

    QString previewLeaseId;
    CleanupSubsystem::instance().registerArtifact(
        CleanupArtifactKind::RemotePreview,
        cleanupDir,
        root,
        true,
        &previewLeaseId);
    if (previewLeaseId.isEmpty()) {
        removeRemotePreviewDir(cleanupDir);
        data.content = QStringLiteral("Cannot register remote preview for cleanup.");
        return data;
    }

    const QString localCopyName = provider->localCopyFileName(normalized).trimmed();
    QString materializedName = safePreviewFileName(localCopyName.isEmpty() ? entry->name : localCopyName);
    const QString materializedSuffix = materializedPreviewSuffix(*entry);
    if (QFileInfo(materializedName).suffix().isEmpty() && !materializedSuffix.isEmpty()) {
        materializedName += QLatin1Char('.') + materializedSuffix;
    }
    const QString materializedPath = QDir(cleanupDir).filePath(materializedName);
    const QString stagingMaterializedPath = QDir(cleanupDir).filePath(QStringLiteral(".staging-") + materializedName);
    bool exceededLimit = false;
    QString error;
    bool copied = false;
    for (int attempt = 0; attempt < 2 && !copied; ++attempt) {
        QFile::remove(stagingMaterializedPath);
        error.clear();
        const bool staged = provider->copyToLocalFileForPreview(
            normalized,
            stagingMaterializedPath,
            [&exceededLimit](qint64 processed, qint64) {
                if (processed > kRemotePreviewMaterializeLimit) {
                    exceededLimit = true;
                    return false;
                }
                return true;
            },
            &error);

        if (!staged) {
            qWarning() << "[QuickLook] remote preview copy failed"
                       << "source:" << redactedPreviewPathForLog(normalized)
                       << "destination:" << stagingMaterializedPath
                       << "error:" << error;
            QFile::remove(stagingMaterializedPath);
            break;
        }

        if (!materializedRemotePreviewLooksUsable(stagingMaterializedPath, *entry)) {
            qWarning() << "[QuickLook] remote preview validation failed"
                       << "source:" << redactedPreviewPathForLog(normalized)
                       << "destination:" << stagingMaterializedPath
                       << "bytes:" << QFileInfo(stagingMaterializedPath).size();
            QFile::remove(stagingMaterializedPath);
            error = QStringLiteral("Cannot decode materialized remote preview.");
            continue;
        }

        QFile::remove(materializedPath);
        if (!QFile::rename(stagingMaterializedPath, materializedPath)) {
            QFile::remove(stagingMaterializedPath);
            error = QStringLiteral("Cannot finalize remote preview file.");
            break;
        }
        if (quickLookPreviewTraceEnabled()) {
            qWarning() << "[QuickLook] remote preview materialized"
                       << "source:" << redactedPreviewPathForLog(normalized)
                       << "path:" << materializedPath
                       << "bytes:" << QFileInfo(materializedPath).size();
        }
        copied = true;
    }

    if (!copied) {
        if (!previewLeaseId.isEmpty()) {
            CleanupSubsystem::instance().scheduleDeleteOnFailure(previewLeaseId);
        } else {
            removeRemotePreviewDir(cleanupDir);
        }
        data.content = exceededLimit
            ? remotePreviewTooLargeText(*entry)
            : (error.trimmed().isEmpty()
                ? QStringLiteral("Cannot materialize remote preview.")
                : error.trimmed());
        return data;
    }

    data = loadLocalPreviewData(materializedPath);
    data.cleanupDir = cleanupDir;
    data.cleanupLeaseId = previewLeaseId;
    data.materializedPath = materializedPath;
    data.metadataPath = materializedPath;
    if (data.type == QLatin1String("audio")) {
        data.audioCoverSource = materializeAudioCoverSource(materializedPath, cleanupDir, data.extension);
    }
    data.name = entry->name;
    if (!fileShortcut && !entry->suffix.isEmpty()) {
        data.extension = entry->suffix;
    }
    data.sizeText = entry->size > 0 ? DriveUtils::formatSize(entry->size) : data.sizeText;
    data.modifiedText = entry->modified.isValid() ? QLocale().toString(entry->modified, QLocale::ShortFormat) : data.modifiedText;
    data.absolutePath = normalized;
    data.parentPath = provider->parentPath(normalized);
    data.readable = true;
    data.writable = false;
    data.executable = false;
    data.permissionsText = googleDriveProvider ? googleDriveAccessSummary(*entry) : QStringLiteral("Read");
    data.attributesText = googleDriveProvider ? QString{} : entry->attributesText;
    data.fullTextAvailable = false;
    data.textChunked = false;
    data.textChunkIndex = 0;
    data.textChunkCount = 0;

    return data;
}

} // namespace PreviewInternal
