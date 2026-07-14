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

#include "../preview/PreviewInternal.h"
#include "../core/FileEntryPresentationResolver.h"

using namespace PreviewInternal;

QuickLookController::QuickLookController(QObject *parent)
    : QObject(parent)
{
}

QuickLookController::~QuickLookController()
{
    clearMaterializedPreview();
}

void QuickLookController::setIsoMountManager(IsoMountManager *manager)
{
    m_isoMountManager = manager;
}

QString QuickLookController::path() const { return m_path; }
QString QuickLookController::content() const { return m_content; }
QString QuickLookController::type() const { return m_type; }
QString QuickLookController::extension() const { return m_extension; }
QString QuickLookController::name() const { return m_name; }
QString QuickLookController::presentationIconSourceForPath(const QString &path,
                                                           bool directory,
                                                           const QString &suffix,
                                                           const QString &mimeName,
                                                           bool useNativeIcons,
                                                           bool useHighQualitySystemIcons) const
{
    return FileEntryPresentationResolver::previewIconSource(
        path, directory, suffix, mimeName, useNativeIcons, useHighQualitySystemIcons);
}
bool QuickLookController::isRemotePreviewContentPath(const QString &path) const
{
    return FileEntryPresentationResolver::isRemotePreviewContentPath(path);
}
bool QuickLookController::canRequestThumbnailForPath(const QString &path) const
{
    return FileEntryPresentationResolver::canRequestThumbnail(path);
}
QString QuickLookController::sizeText() const { return m_sizeText; }
QString QuickLookController::modifiedText() const { return m_modifiedText; }
QString QuickLookController::mimeName() const { return m_mimeName; }
bool QuickLookController::directory() const { return m_directory; }
bool QuickLookController::hidden() const { return m_hidden; }
bool QuickLookController::symlink() const { return m_symlink; }
bool QuickLookController::readable() const { return m_readable; }
bool QuickLookController::writable() const { return m_writable; }
bool QuickLookController::executable() const { return m_executable; }
QString QuickLookController::absolutePath() const { return m_absolutePath; }
QString QuickLookController::parentPath() const { return m_parentPath; }
QString QuickLookController::permissionsText() const { return m_permissionsText; }
QString QuickLookController::attributesText() const { return m_attributesText; }
int QuickLookController::lines() const { return m_lines; }
bool QuickLookController::textTruncated() const { return m_textTruncated; }
bool QuickLookController::fullTextAvailable() const { return m_fullTextAvailable; }
bool QuickLookController::textChunked() const { return m_textChunked; }
int QuickLookController::textChunkIndex() const { return m_textChunkIndex; }
int QuickLookController::textChunkCount() const { return m_textChunkCount; }
bool QuickLookController::loading() const { return m_loading; }
bool QuickLookController::visible() const { return m_visible; }
QVariantList QuickLookController::extraProperties() const { return m_extraProperties; }
QString QuickLookController::audioTitle() const { return m_audioTitle; }
QString QuickLookController::audioArtist() const { return m_audioArtist; }
QString QuickLookController::audioAlbum() const { return m_audioAlbum; }
QString QuickLookController::audioYear() const { return m_audioYear; }
QString QuickLookController::audioTrack() const { return m_audioTrack; }
QString QuickLookController::audioGenre() const { return m_audioGenre; }
QString QuickLookController::audioComment() const { return m_audioComment; }
QString QuickLookController::audioDuration() const { return m_audioDuration; }
QString QuickLookController::audioBitrate() const { return m_audioBitrate; }
QString QuickLookController::audioSampleRate() const { return m_audioSampleRate; }
QString QuickLookController::audioChannels() const { return m_audioChannels; }
QString QuickLookController::audioCoverSource() const { return m_audioCoverSource; }
QString QuickLookController::mediaSourceUrl() const
{
    if (!m_materializedPreviewFile.isEmpty()
        && (m_type == QStringLiteral("audio")
            || m_type == QStringLiteral("video")
            || m_type == QStringLiteral("image")
            || m_type == QStringLiteral("svg")
            || m_type == QStringLiteral("pdf")
            || m_type == QStringLiteral("font"))) {
        return QUrl::fromLocalFile(m_materializedPreviewFile).toString(QUrl::FullyEncoded);
    }
    if (m_path.isEmpty() || ArchiveSupport::isArchivePath(m_path)) {
        return {};
    }
    if (FileProviderFactory::hasPluginProviderForPath(m_path)) {
        return {};
    }
    return QUrl::fromLocalFile(m_path).toString(QUrl::FullyEncoded);
}
bool QuickLookController::hasPdfSupport() const
{
#ifdef HAS_QT_PDF
    return true;
#else
    return false;
#endif
}

bool QuickLookController::hasMultimediaSupport() const
{
#ifdef HAS_QT_MULTIMEDIA
    return true;
#else
    return false;
#endif
}

int QuickLookController::imageWidth() const { return m_imageWidth; }
int QuickLookController::imageHeight() const { return m_imageHeight; }
QString QuickLookController::imageFormatText() const { return m_imageFormatText; }
QString QuickLookController::imageColorDepthText() const { return m_imageColorDepthText; }
QString QuickLookController::imageAlphaChannelText() const { return m_imageAlphaChannelText; }
QString QuickLookController::imageDpiText() const { return m_imageDpiText; }
QString QuickLookController::imageColorSpaceText() const { return m_imageColorSpaceText; }
QString QuickLookController::imagePixelFormatText() const { return m_imagePixelFormatText; }
int QuickLookController::bookPageIndex() const { return m_bookPageIndex; }
int QuickLookController::bookPageCount() const { return m_bookPages.size(); }
QString QuickLookController::bookCoverSource() const { return m_bookCoverSource; }
QString QuickLookController::bookTitle() const { return m_bookTitle; }
QString QuickLookController::bookAuthor() const { return m_bookAuthor; }

void QuickLookController::resetAudioProperties()
{
    m_audioTitle.clear();
    m_audioArtist.clear();
    m_audioAlbum.clear();
    m_audioYear.clear();
    m_audioTrack.clear();
    m_audioGenre.clear();
    m_audioComment.clear();
    m_audioDuration.clear();
    m_audioBitrate.clear();
    m_audioSampleRate.clear();
    m_audioChannels.clear();
    m_audioCoverSource.clear();
}

void QuickLookController::syncAudioProperties(const QVariantList &properties)
{
    m_audioTitle = propertyValue(properties, QStringLiteral("Title"));
    m_audioArtist = propertyValue(properties, QStringLiteral("Artist"));
    m_audioAlbum = propertyValue(properties, QStringLiteral("Album"));
    m_audioYear = propertyValue(properties, QStringLiteral("Year"));
    m_audioTrack = propertyValue(properties, QStringLiteral("Track"));
    m_audioGenre = propertyValue(properties, QStringLiteral("Genre"));
    m_audioComment = propertyValue(properties, QStringLiteral("Comment"));
    m_audioDuration = propertyValue(properties, QStringLiteral("Duration"));
    m_audioBitrate = propertyValue(properties, QStringLiteral("Bitrate"));
    m_audioSampleRate = propertyValue(properties, QStringLiteral("Sample Rate"));
    m_audioChannels = propertyValue(properties, QStringLiteral("Channels"));
}

void QuickLookController::resetImageInfo()
{
    m_imageWidth = 0;
    m_imageHeight = 0;
    m_imageFormatText.clear();
    m_imageColorDepthText.clear();
    m_imageAlphaChannelText.clear();
    m_imageDpiText.clear();
    m_imageColorSpaceText.clear();
    m_imagePixelFormatText.clear();
}

void QuickLookController::resetBookInfo()
{
    m_bookPages.clear();
    m_bookParagraphs.clear();
    m_bookPageIndex = 0;
    m_bookReaderPixelSize = kFb2DefaultReaderPixelSize;
    m_bookCoverSource.clear();
    m_bookTitle.clear();
    m_bookAuthor.clear();
    m_bookContentLoading = false;
    ++m_bookContentGeneration;
}

void QuickLookController::syncImageProperties(const QVariantList &properties)
{
    const QString format = propertyValue(properties, QStringLiteral("Format"));
    const QString colorDepth = propertyValue(properties, QStringLiteral("Color Depth"));
    const QString alpha = propertyValue(properties, QStringLiteral("Alpha Channel"));
    const QString dpi = propertyValue(properties, QStringLiteral("DPI"));
    const QString colorSpace = propertyValue(properties, QStringLiteral("Color Space"));
    const QString pixelFormat = propertyValue(properties, QStringLiteral("Pixel Format"));
    const QSize dimensions = dimensionsFromText(propertyValue(properties, QStringLiteral("Dimensions")));

    if (!format.isEmpty()) {
        m_imageFormatText = format;
    }
    if (dimensions.isValid()) {
        m_imageWidth = dimensions.width();
        m_imageHeight = dimensions.height();
    }
    if (!colorDepth.isEmpty()) {
        m_imageColorDepthText = colorDepth;
    }
    if (!alpha.isEmpty()) {
        m_imageAlphaChannelText = alpha;
    }
    m_imageDpiText = dpi;
    m_imageColorSpaceText = colorSpace;
    if (!pixelFormat.isEmpty()) {
        m_imagePixelFormatText = pixelFormat;
    }
}

bool QuickLookController::imageMetadataRequested() const
{
    return m_previewPaneImageMetadataRequested || m_quickLookImageMetadataRequested;
}

void QuickLookController::setImageMetadataRequested(const QString &scope, bool requested)
{
    bool changed = false;
    if (scope == QStringLiteral("quicklook")) {
        changed = m_quickLookImageMetadataRequested != requested;
        m_quickLookImageMetadataRequested = requested;
    } else {
        changed = m_previewPaneImageMetadataRequested != requested;
        m_previewPaneImageMetadataRequested = requested;
    }

    if (changed && requested && imageMetadataRequested()) {
        requestImageMetadata();
    }
}

void QuickLookController::requestImageMetadata()
{
    const QString imagePath = m_type == QStringLiteral("image") && !m_materializedPreviewFile.isEmpty()
        ? m_materializedPreviewFile
        : (m_type == QStringLiteral("image") && !m_content.isEmpty()
        ? m_content
        : m_path);
    if (!imageMetadataRequested()
        || m_imageMetadataLoading
        || m_type != QStringLiteral("image")
        || imagePath.isEmpty()
        || ArchiveSupport::isArchivePath(imagePath)
        || m_imageMetadataLoadedPath == imagePath) {
        return;
    }

    const QString path = imagePath;
    const int myGen = m_previewGeneration.load();
    m_imageMetadataLoading = true;

    QPointer<QuickLookController> self(this);
    (void)QtConcurrent::run([self, path, myGen]() {
        ImageMetadataData data = loadImageMetadataData(path);
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, path, myGen, data = std::move(data)]() mutable {
            if (!self || myGen != self->m_previewGeneration.load()) {
                return;
            }
            self->m_imageMetadataLoading = false;
            self->m_imageMetadataLoadedPath = path;
            self->m_imageWidth = data.width;
            self->m_imageHeight = data.height;
            self->m_imageFormatText = std::move(data.formatText);
            self->m_imageColorDepthText = std::move(data.colorDepthText);
            self->m_imageAlphaChannelText = std::move(data.alphaChannelText);
            self->m_imageDpiText = std::move(data.dpiText);
            self->m_imageColorSpaceText = std::move(data.colorSpaceText);
            self->m_imagePixelFormatText = std::move(data.pixelFormatText);
            self->m_extraProperties = std::move(data.extraProperties);
            self->syncImageProperties(self->m_extraProperties);
            emit self->extraPropertiesChanged();
            emit self->imageSizeChanged();
            emit self->imageInfoChanged();
        });
    });
}

void QuickLookController::requestMetadata(const QString &path, int previewGeneration, int retryAttempt, const QString &expectedPath)
{
    const QString activePath = expectedPath.isEmpty() ? path : expectedPath;
    QPointer<QuickLookController> self(this);
    (void)QtConcurrent::run([self, path, activePath, previewGeneration, retryAttempt]() {
        QVariantList props = MetadataExtractor::extract(path);
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(), [self, path, activePath, previewGeneration, retryAttempt, props = std::move(props)]() mutable {
            if (!self
                || previewGeneration != self->m_previewGeneration.load()
                || self->m_path != activePath) {
                return;
            }

            const bool keepExistingAudioProps = retryAttempt > 0
                && props.isEmpty()
                && self->m_type == QStringLiteral("audio")
                && !self->m_extraProperties.isEmpty();
            if (!keepExistingAudioProps) {
                self->m_extraProperties = std::move(props);
            }

            if (self->m_type == QStringLiteral("audio")) {
                if (!keepExistingAudioProps) {
                    self->syncAudioProperties(self->m_extraProperties);
                }
                emit self->audioPropertiesChanged();

                const bool missingCoreAudioMetadata = self->m_audioDuration.isEmpty()
                    || self->m_audioSampleRate.isEmpty();
                if (missingCoreAudioMetadata && retryAttempt < kAudioMetadataRetryCount) {
                    const int nextAttempt = retryAttempt + 1;
                    QTimer::singleShot(kAudioMetadataRetryBaseDelayMs * nextAttempt,
                                       self.data(),
                                       [self, path, activePath, previewGeneration, nextAttempt]() {
                        if (!self
                            || previewGeneration != self->m_previewGeneration.load()
                            || self->m_path != activePath
                            || self->m_type != QStringLiteral("audio")
                            || (!self->m_audioDuration.isEmpty() && !self->m_audioSampleRate.isEmpty())) {
                            return;
                        }
                        self->requestMetadata(path, previewGeneration, nextAttempt, activePath);
                    });
                }
            } else if (self->m_type == QStringLiteral("image")
                       || self->m_type == QStringLiteral("svg")) {
                self->syncImageProperties(self->m_extraProperties);
                emit self->imageInfoChanged();
                emit self->imageSizeChanged();
            }

            emit self->extraPropertiesChanged();
        }, Qt::QueuedConnection);
    });
}

void QuickLookController::clearMaterializedPreview()
{
    m_materializedPreviewFile.clear();
    const QString leaseId = std::move(m_materializedPreviewLeaseId);
    m_materializedPreviewLeaseId.clear();
    const QString cleanupDir = std::move(m_materializedPreviewDir);
    m_materializedPreviewDir.clear();

    if (!leaseId.isEmpty()) {
        CleanupSubsystem::instance().scheduleDelete(leaseId);
    } else if (!cleanupDir.isEmpty()) {
        removeRemotePreviewDir(cleanupDir);
    }
}

void QuickLookController::preview(const QString &path)
{
    previewPath(path, false);
}

void QuickLookController::previewDrive(const QVariantMap &drive)
{
    const QString rootPath = drive.value(QStringLiteral("rootPath")).toString();
    if (rootPath.isEmpty()) {
        preview(QStringLiteral("devices://"));
        return;
    }

    ++m_previewGeneration;
    clearMaterializedPreview();
    resetAudioProperties();
    resetImageInfo();
    resetBookInfo();
    m_imageMetadataLoading = false;
    m_imageMetadataLoadedPath.clear();

    const qint64 totalBytes = drive.value(QStringLiteral("totalBytes")).toLongLong();
    const qint64 freeBytes = drive.value(QStringLiteral("freeBytes")).toLongLong();
    const QString fileSystem = drive.value(QStringLiteral("fileSystem")).toString();
    const QString driveType = drive.value(QStringLiteral("driveType")).toString();
    const QString deviceDescription = drive.value(QStringLiteral("deviceDescription")).toString();
    const QString blockDevice = drive.value(QStringLiteral("blockDevice")).toString();
    const QString name = drive.value(QStringLiteral("name")).toString();

    m_path = rootPath;
    m_content.clear();
    m_type = QStringLiteral("drive");
    m_extension = fileSystem;
    m_name = name.isEmpty() ? DriveUtils::rootDisplayName(rootPath) : name;
    m_sizeText = totalBytes > 0
        ? QStringLiteral("%1 free of %2").arg(DriveUtils::formatSize(freeBytes), DriveUtils::formatSize(totalBytes))
        : QString();
    m_modifiedText = deviceDescription;
    m_mimeName = QStringLiteral("drive");
    m_directory = false;
    m_hidden = false;
    m_symlink = false;
    m_readable = true;
    m_writable = false;
    m_executable = false;
    m_absolutePath = rootPath;
    m_parentPath.clear();
    m_permissionsText.clear();
    m_attributesText.clear();
    m_lines = 0;
    m_textTruncated = false;
    m_fullTextAvailable = false;
    m_textChunked = false;
    m_textChunkIndex = 0;
    m_textChunkCount = 0;
    m_extraProperties.clear();
    m_extraProperties.append(prop(QStringLiteral("Mount point"), rootPath));
    if (!fileSystem.isEmpty()) m_extraProperties.append(prop(QStringLiteral("Filesystem"), fileSystem));
    if (!driveType.isEmpty()) m_extraProperties.append(prop(QStringLiteral("Drive type"), driveType));
    if (totalBytes > 0) {
        m_extraProperties.append(prop(QStringLiteral("Capacity"), DriveUtils::formatSize(totalBytes)));
        m_extraProperties.append(prop(QStringLiteral("Free space"), DriveUtils::formatSize(freeBytes)));
        m_extraProperties.append(prop(QStringLiteral("Capacity bytes"), QString::number(totalBytes)));
        m_extraProperties.append(prop(QStringLiteral("Free bytes"), QString::number(freeBytes)));
    }
    m_extraProperties.append(prop(QStringLiteral("Critical"), drive.value(QStringLiteral("critical")).toBool() ? QStringLiteral("true") : QStringLiteral("false")));
    if (!deviceDescription.isEmpty()) m_extraProperties.append(prop(QStringLiteral("Device"), deviceDescription));
    if (!blockDevice.isEmpty()) m_extraProperties.append(prop(QStringLiteral("Block device"), blockDevice));
    const bool loadingChangedValue = m_loading;
    m_loading = false;

    emit pathChanged();
    emit contentChanged();
    emit typeChanged();
    emit extensionChanged();
    emit nameChanged();
    emit sizeTextChanged();
    emit modifiedTextChanged();
    emit mimeNameChanged();
    emit directoryChanged();
    emit hiddenChanged();
    emit symlinkChanged();
    emit readableChanged();
    emit writableChanged();
    emit executableChanged();
    emit absolutePathChanged();
    emit parentPathChanged();
    emit permissionsTextChanged();
    emit attributesTextChanged();
    emit linesChanged();
    emit textStateChanged();
    emit extraPropertiesChanged();
    emit audioPropertiesChanged();
    emit mediaSourceUrlChanged();
    emit imageSizeChanged();
    emit imageInfoChanged();
    emit bookPageStateChanged();
    if (loadingChangedValue) emit loadingChanged();
}

void QuickLookController::loadFullText()
{
    if (m_path.isEmpty() || m_type != QStringLiteral("text") || !m_textTruncated || !m_fullTextAvailable) {
        return;
    }

    QFileInfo info(m_path);
    if (info.exists() && info.size() > kTextFullLoadLimit) {
        loadTextChunk(0);
        return;
    }

    const QString path = m_path;
    const int myGen = ++m_previewGeneration;
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged();
    }

    QPointer<QuickLookController> self(this);
    (void)QtConcurrent::run([self, path, myGen]() {
        PreviewData data;
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            const qint64 fileSize = file.size();
            const qint64 bytesToRead = fileSize >= 0 ? qMin(fileSize, kTextFullLoadLimit) : kTextFullLoadLimit;
            QByteArray raw = file.read(bytesToRead);
            data.content = QString::fromUtf8(raw);
            data.lines = data.content.isEmpty() ? 0 : data.content.count('\n') + 1;
            data.truncated = fileSize > kTextFullLoadLimit;
            data.fullTextAvailable = false;
            data.chunked = false;
            if (data.truncated) {
                if (!data.content.isEmpty() && !data.content.endsWith('\n')) {
                    data.content.append('\n');
                }
                data.content.append(QStringLiteral("...\nFile is too large to load fully in QuickLook."));
                data.lines = data.content.count('\n') + 1;
            }
        } else {
            QByteArray raw;
            qint64 fileSize = 0;
            if (readFileRangeAsAdministrator(path, 0, kTextFullLoadLimit, &raw, &fileSize)) {
                data.content = QString::fromUtf8(raw);
                data.lines = data.content.isEmpty() ? 0 : data.content.count('\n') + 1;
                data.truncated = fileSize > kTextFullLoadLimit;
                data.fullTextAvailable = false;
                data.chunked = false;
                if (data.truncated) {
                    if (!data.content.isEmpty() && !data.content.endsWith('\n')) {
                        data.content.append('\n');
                    }
                    data.content.append(QStringLiteral("...\nFile is too large to load fully in QuickLook."));
                    data.lines = data.content.count('\n') + 1;
                }
            } else {
                data.content = QStringLiteral("Cannot read file.");
                data.lines = 0;
                data.truncated = false;
                data.fullTextAvailable = false;
            }
        }

        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(), [self, myGen, previewData = std::move(data)]() mutable {
            if (!self || myGen != self->m_previewGeneration.load()) {
                return;
            }
            self->m_content = std::move(previewData.content);
            self->m_lines = previewData.lines;
            self->m_textTruncated = previewData.truncated;
            self->m_fullTextAvailable = previewData.fullTextAvailable;
            self->m_textChunked = previewData.chunked;
            self->m_textChunkIndex = previewData.chunkIndex;
            self->m_textChunkCount = previewData.chunkCount;
            if (self->m_loading) {
                self->m_loading = false;
                emit self->loadingChanged();
            }
            emit self->linesChanged();
            emit self->textStateChanged();
            emit self->contentChanged();
        }, Qt::QueuedConnection);
    });
}

void QuickLookController::loadTextChunk(int chunkIndex)
{
    if (m_path.isEmpty() || m_type != QStringLiteral("text") || !m_fullTextAvailable) {
        return;
    }

    const QString path = m_path;
    const int myGen = ++m_previewGeneration;
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged();
    }

    QPointer<QuickLookController> self(this);
    (void)QtConcurrent::run([self, path, chunkIndex, myGen]() {
        PreviewData data;
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            const qint64 fileSize = file.size();
            const int chunkCount = fileSize > 0
                ? static_cast<int>((fileSize + kTextChunkSize - 1) / kTextChunkSize)
                : 1;
            const int clampedIndex = qBound(0, chunkIndex, qMax(0, chunkCount - 1));
            file.seek(static_cast<qint64>(clampedIndex) * kTextChunkSize);
            QByteArray raw = file.read(kTextChunkSize);
            data.content = QString::fromUtf8(raw);
            data.lines = data.content.isEmpty() ? 0 : data.content.count('\n') + 1;
            data.truncated = chunkCount > 1;
            data.fullTextAvailable = true;
            data.chunked = chunkCount > 1;
            data.chunkIndex = clampedIndex;
            data.chunkCount = chunkCount;
        } else {
            QByteArray raw;
            qint64 fileSize = 0;
            const qint64 requestedOffset = static_cast<qint64>(qMax(0, chunkIndex)) * kTextChunkSize;
            if (readFileRangeAsAdministrator(path, requestedOffset, kTextChunkSize, &raw, &fileSize)) {
                const int chunkCount = fileSize > 0
                    ? static_cast<int>((fileSize + kTextChunkSize - 1) / kTextChunkSize)
                    : 1;
                const int clampedIndex = qBound(0, chunkIndex, qMax(0, chunkCount - 1));
                data.content = QString::fromUtf8(raw);
                data.lines = data.content.isEmpty() ? 0 : data.content.count('\n') + 1;
                data.truncated = chunkCount > 1;
                data.fullTextAvailable = true;
                data.chunked = chunkCount > 1;
                data.chunkIndex = clampedIndex;
                data.chunkCount = chunkCount;
            } else {
                data.content = QStringLiteral("Cannot read file.");
                data.lines = 0;
                data.truncated = false;
                data.fullTextAvailable = false;
            }
        }

        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(), [self, myGen, previewData = std::move(data)]() mutable {
            if (!self || myGen != self->m_previewGeneration.load()) {
                return;
            }
            self->m_content = std::move(previewData.content);
            self->m_lines = previewData.lines;
            self->m_textTruncated = previewData.truncated;
            self->m_fullTextAvailable = previewData.fullTextAvailable;
            self->m_textChunked = previewData.chunked;
            self->m_textChunkIndex = previewData.chunkIndex;
            self->m_textChunkCount = previewData.chunkCount;
            if (self->m_loading) {
                self->m_loading = false;
                emit self->loadingChanged();
            }
            emit self->linesChanged();
            emit self->textStateChanged();
            emit self->contentChanged();
        }, Qt::QueuedConnection);
    });
}

void QuickLookController::loadBookContent()
{
    if (m_type != QStringLiteral("book") || m_path.isEmpty() || m_bookContentLoading || !m_bookPages.isEmpty()) {
        return;
    }

    const QString path = !m_materializedPreviewFile.isEmpty() ? m_materializedPreviewFile : m_path;
    const QString displayPath = m_path;
    const int myGen = m_previewGeneration.load();
    const int myBookGen = ++m_bookContentGeneration;
    m_bookContentLoading = true;
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged();
    }

    QPointer<QuickLookController> self(this);
    (void)QtConcurrent::run([self, path, displayPath, myGen, myBookGen]() {
#ifdef HAS_UNOFFICIAL_BIT7Z
        Fb2PreviewData data = ArchiveSupport::isArchivePath(path)
            ? loadFb2ArchiveEntryPreviewData(path, true)
            : (isFb2ZipPath(path)
            ? loadFb2ZipPreviewData(path, true)
            : loadFb2PreviewData(path, true));
#else
        Fb2PreviewData data = loadFb2PreviewData(path, true);
#endif
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(), [self, displayPath, myGen, myBookGen, data = std::move(data)]() mutable {
            if (!self
                || myGen != self->m_previewGeneration.load()
                || myBookGen != self->m_bookContentGeneration
                || self->m_path != displayPath) {
                return;
            }

            self->m_content = std::move(data.content);
            self->m_extraProperties = std::move(data.extraProperties);
            self->m_bookPages = std::move(data.pages);
            self->m_bookParagraphs = std::move(data.paragraphs);
            self->m_bookCoverSource = std::move(data.coverSource);
            self->m_bookTitle = std::move(data.title);
            self->m_bookAuthor = std::move(data.author);
            self->m_bookPageIndex = data.pageIndex;
            self->m_bookReaderPixelSize = kFb2DefaultReaderPixelSize;
            self->m_lines = data.lines;
            self->m_bookContentLoading = false;
            if (self->m_loading) {
                self->m_loading = false;
                emit self->loadingChanged();
            }

            emit self->contentChanged();
            emit self->extraPropertiesChanged();
            emit self->linesChanged();
            emit self->bookPageStateChanged();
        }, Qt::QueuedConnection);
    });
}

void QuickLookController::loadBookPage(int pageIndex)
{
    if (m_type != QStringLiteral("book") || m_bookPages.isEmpty()) {
        return;
    }

    const int clampedIndex = qBound(0, pageIndex, m_bookPages.size() - 1);
    if (clampedIndex == m_bookPageIndex && m_content == m_bookPages.at(clampedIndex)) {
        return;
    }

    m_bookPageIndex = clampedIndex;
    m_content = m_bookPages.at(clampedIndex);
    m_lines = m_content.isEmpty() ? 0 : m_content.count(QLatin1Char('\n')) + 1;
    setPropertyValue(m_extraProperties,
                     QStringLiteral("Page"),
                     QStringLiteral("%1 / %2").arg(m_bookPageIndex + 1).arg(m_bookPages.size()));

    emit contentChanged();
    emit linesChanged();
    emit extraPropertiesChanged();
    emit bookPageStateChanged();
}

void QuickLookController::unloadBookContent()
{
    if (m_type != QStringLiteral("book")
        || (m_content.isEmpty() && m_bookPages.isEmpty() && m_bookParagraphs.isEmpty() && !m_bookContentLoading)) {
        return;
    }

    m_content.clear();
    m_lines = 0;
    m_bookPages.clear();
    m_bookParagraphs.clear();
    m_bookPageIndex = 0;
    m_bookReaderPixelSize = kFb2DefaultReaderPixelSize;
    m_bookContentLoading = false;
    ++m_bookContentGeneration;
    if (m_loading) {
        m_loading = false;
        emit loadingChanged();
    }
    removePropertyValue(m_extraProperties, QStringLiteral("Pages"));
    removePropertyValue(m_extraProperties, QStringLiteral("Page"));

    emit contentChanged();
    emit linesChanged();
    emit extraPropertiesChanged();
    emit bookPageStateChanged();
}

void QuickLookController::setBookReaderPixelSize(int pixelSize)
{
    if (m_type != QStringLiteral("book") || m_bookParagraphs.isEmpty()) {
        return;
    }

    const int normalizedSize = qBound(10, pixelSize, 28);
    if (normalizedSize == m_bookReaderPixelSize) {
        return;
    }

    const int oldCount = m_bookPages.size();
    const double position = oldCount > 1
        ? static_cast<double>(m_bookPageIndex) / static_cast<double>(oldCount - 1)
        : 0.0;

    m_bookReaderPixelSize = normalizedSize;
    m_bookPages = buildFb2Pages(m_bookParagraphs, fb2PageCharLimitForPixelSize(m_bookReaderPixelSize));
    if (m_bookPages.isEmpty()) {
        m_content.clear();
        m_lines = 0;
        setPropertyValue(m_extraProperties, QStringLiteral("Pages"), QStringLiteral("0"));
        setPropertyValue(m_extraProperties, QStringLiteral("Page"), QStringLiteral("0 / 0"));
        emit contentChanged();
        emit linesChanged();
        emit extraPropertiesChanged();
        emit bookPageStateChanged();
        return;
    }

    m_bookPageIndex = m_bookPages.size() > 1
        ? qBound(0, qRound(position * static_cast<double>(m_bookPages.size() - 1)), m_bookPages.size() - 1)
        : 0;
    m_content = m_bookPages.at(m_bookPageIndex);
    m_lines = m_content.isEmpty() ? 0 : m_content.count(QLatin1Char('\n')) + 1;

    setPropertyValue(m_extraProperties, QStringLiteral("Pages"), QString::number(m_bookPages.size()));
    setPropertyValue(m_extraProperties,
                     QStringLiteral("Page"),
                     QStringLiteral("%1 / %2").arg(m_bookPageIndex + 1).arg(m_bookPages.size()));

    emit contentChanged();
    emit linesChanged();
    emit extraPropertiesChanged();
    emit bookPageStateChanged();
}

void QuickLookController::previewSelection(const QStringList &paths)
{
    if (paths.size() <= 1) {
        previewPath(paths.isEmpty() ? QString() : paths.first(), false);
        return;
    }

    const int myGen = ++m_previewGeneration;
    clearMaterializedPreview();

    m_path = QStringLiteral("selection://");
    m_content.clear();
    m_type = QStringLiteral("info");
    m_extension.clear();
    m_name = QStringLiteral("%1 items selected").arg(paths.size());
    m_sizeText = QStringLiteral("Calculating...");
    m_modifiedText = QStringLiteral("Multiple selection");
    m_mimeName = QStringLiteral("selection");
    m_directory = false;
    m_hidden = false;
    m_symlink = false;
    m_readable = true;
    m_writable = false;
    m_executable = false;
    m_absolutePath.clear();
    m_parentPath.clear();
    m_permissionsText.clear();
    m_attributesText.clear();
    m_lines = 0;
    m_textTruncated = false;
    m_fullTextAvailable = false;
    m_textChunked = false;
    m_textChunkIndex = 0;
    m_textChunkCount = 0;
    m_loading = true;
    resetImageInfo();
    resetBookInfo();
    resetAudioProperties();

    m_extraProperties.clear();
    m_extraProperties.append(prop(QStringLiteral("Selected"), QStringLiteral("%1 items").arg(paths.size())));

    emit extensionChanged();
    emit nameChanged();
    emit sizeTextChanged();
    emit modifiedTextChanged();
    emit mimeNameChanged();
    emit directoryChanged();
    emit hiddenChanged();
    emit symlinkChanged();
    emit readableChanged();
    emit writableChanged();
    emit executableChanged();
    emit absolutePathChanged();
    emit parentPathChanged();
    emit permissionsTextChanged();
    emit attributesTextChanged();
    emit linesChanged();
    emit textStateChanged();
    emit loadingChanged();
    emit typeChanged();
    emit pathChanged();
    emit mediaSourceUrlChanged();
    emit contentChanged();
    emit extraPropertiesChanged();
    emit audioPropertiesChanged();
    emit imageSizeChanged();
    emit imageInfoChanged();
    emit bookPageStateChanged();

    QPointer<QuickLookController> self(this);
    (void)QtConcurrent::run([self, paths, myGen]() {
        qint64 totalSize = 0;
        int files = 0;
        int folders = 0;
        int other = 0;

        for (const QString &path : paths) {
            const QFileInfo info(path);
            if (info.isDir()) {
                ++folders;
            } else if (info.isFile()) {
                ++files;
                totalSize += info.size();
            } else {
                ++other;
            }
        }

        const QString sizeText = files > 0
            ? DriveUtils::formatSize(totalSize)
            : QString();
        QVariantList properties;
        properties.append(prop(QStringLiteral("Selected"), QStringLiteral("%1 items").arg(paths.size())));
        if (files > 0) {
            properties.append(prop(QStringLiteral("Files"), QString::number(files)));
        }
        if (folders > 0) {
            properties.append(prop(QStringLiteral("Folders"), QString::number(folders)));
        }
        if (other > 0) {
            properties.append(prop(QStringLiteral("Other"), QString::number(other)));
        }
        if (files > 0) {
            properties.append(prop(QStringLiteral("File Size Total"), sizeText));
        }

        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(), [self, myGen, sizeText, properties = std::move(properties)]() mutable {
            if (!self || myGen != self->m_previewGeneration.load()) {
                return;
            }
            self->m_sizeText = sizeText;
            self->m_extraProperties = std::move(properties);
            self->m_loading = false;
            emit self->sizeTextChanged();
            emit self->extraPropertiesChanged();
            emit self->loadingChanged();
        }, Qt::QueuedConnection);
    });
}

void QuickLookController::refresh()
{
    if (m_path.isEmpty()) {
        return;
    }
    previewPath(m_path, true);
}

bool QuickLookController::previewVirtualRoot(const QString &path)
{
    if (path.isEmpty()
        || path == QStringLiteral("devices://")
        || path == QStringLiteral("favorites://")
        || path == QStringLiteral("gdrive://")) {
        const int myGen = ++m_previewGeneration;
        clearMaterializedPreview();
        if (path.isEmpty()) {
            m_path.clear();
        } else {
            m_path = path; // keep virtual roots to prevent re-triggering
        }
        m_content.clear();
        m_type = QStringLiteral("info");
        const bool favoritesRoot = path == QStringLiteral("favorites://");
        const bool googleDriveRoot = path == QStringLiteral("gdrive://");
        const QString googleDriveAccount = googleDriveRoot ? googleDriveAccountLabel() : QString();
        m_extension = googleDriveRoot ? QStringLiteral("cloud") : QString();
        m_name = googleDriveRoot
            ? QStringLiteral("Google Drive")
            : (favoritesRoot ? QStringLiteral("Favorites") : QStringLiteral("Devices and Drives"));
        m_sizeText = googleDriveRoot
            ? (googleDriveAccount.isEmpty()
                   ? QStringLiteral("My Drive and shared files")
                   : googleDriveAccount)
            : (favoritesRoot ? QStringLiteral("Pinned and frequent locations") : QStringLiteral("Detecting drives..."));
        m_modifiedText.clear();
        m_mimeName = googleDriveRoot ? QStringLiteral("Google Drive") : QString();
        m_directory = false;
        m_hidden = false;
        m_symlink = false;
        m_readable = true;
        m_writable = false;
        m_executable = false;
        m_absolutePath.clear();
        m_parentPath.clear();
        m_permissionsText.clear();
        m_attributesText.clear();
        m_lines = 0;
        m_textTruncated = false;
        m_fullTextAvailable = false;
        m_textChunked = false;
        m_textChunkIndex = 0;
        m_textChunkCount = 0;
        resetImageInfo();
        resetBookInfo();
        m_extraProperties.clear();
        if (googleDriveRoot) {
            m_extraProperties.append(prop(QStringLiteral("Provider"), QStringLiteral("Google Drive")));
            if (!googleDriveAccount.isEmpty()) {
                m_extraProperties.append(prop(QStringLiteral("Account"), googleDriveAccount));
            }
            m_extraProperties.append(prop(QStringLiteral("Location"), QStringLiteral("gdrive://")));
            m_extraProperties.append(prop(QStringLiteral("Contents"), QStringLiteral("My Drive, Shared with me")));
        }
        resetAudioProperties();
        if (favoritesRoot || googleDriveRoot) {
            if (m_loading) {
                m_loading = false;
                emit loadingChanged();
            }
        } else if (!m_loading) {
            m_loading = true;
            emit loadingChanged();
        }

        emit extensionChanged();
        emit nameChanged();
        emit sizeTextChanged();
        emit modifiedTextChanged();
        emit mimeNameChanged();
        emit directoryChanged();
        emit hiddenChanged();
        emit symlinkChanged();
        emit readableChanged();
        emit writableChanged();
        emit executableChanged();
        emit absolutePathChanged();
        emit parentPathChanged();
        emit permissionsTextChanged();
        emit attributesTextChanged();
        emit linesChanged();
        emit textStateChanged();
        emit typeChanged();
        emit pathChanged();
        emit mediaSourceUrlChanged();
        emit contentChanged();
        emit extraPropertiesChanged();
        emit audioPropertiesChanged();
        emit imageSizeChanged();
        emit imageInfoChanged();
        emit bookPageStateChanged();

        if (favoritesRoot || googleDriveRoot) {
            return true;
        }

        QPointer<QuickLookController> self(this);
        (void)QtConcurrent::run([self, myGen]() {
            DevicesPreviewData data;
            const QFileInfoList drives = QDir::drives();
            data.sizeText = QStringLiteral("%1 drive(s)").arg(drives.size());

            for (const QFileInfo &drive : drives) {
                QStorageInfo storage(drive.absolutePath());
                QVariantMap m;
                m.insert(QStringLiteral("label"), drive.absolutePath());
                if (storage.isValid()) {
                    const qint64 total = storage.bytesTotal();
                    const qint64 free  = storage.bytesFree();
                    const qint64 used  = total - free;
                    const QString fs = QString::fromLatin1(storage.fileSystemType());
                    QString val = fs;
                    if (total > 0) {
                        val += QStringLiteral("  |  Total: ");
                        val += DriveUtils::formatSize(total);
                        val += QStringLiteral("  |  Free: ");
                        val += DriveUtils::formatSize(free);
                        if (used > 0) {
                            const int pct = static_cast<int>(used * 100 / total);
                            val += QStringLiteral("  |  %1% used").arg(pct);
                        }
                    } else {
                        val += QStringLiteral("  (no media)");
                    }
                    m.insert(QStringLiteral("value"), val);
                } else {
                    m.insert(QStringLiteral("value"), QStringLiteral("—"));
                }
                data.extraProperties.append(QVariant::fromValue(m));
            }

            if (!self) return;
            QMetaObject::invokeMethod(self.data(), [self, myGen, data = std::move(data)]() mutable {
                if (!self || myGen != self->m_previewGeneration.load()) {
                    return;
                }
                self->m_sizeText = std::move(data.sizeText);
                self->m_extraProperties = std::move(data.extraProperties);
                self->m_loading = false;
                emit self->sizeTextChanged();
                emit self->extraPropertiesChanged();
                emit self->loadingChanged();
            });
        });
        return true;
    }

    return false;
}

void QuickLookController::previewPath(const QString &path, bool forceReload)
{
    if (previewVirtualRoot(path)) {
        return;
    }

    if (path == m_path && !forceReload) {
        return;
    }

    const int myGen = ++m_previewGeneration;
    clearMaterializedPreview();
    resetImageInfo();
    resetBookInfo();
    m_imageMetadataLoading = false;
    m_imageMetadataLoadedPath.clear();
    m_path = path;
    const bool archivePath = ArchiveSupport::isArchivePath(path);
    if (!archivePath) {
        previewLocalOrMaterializedFile(path, myGen);
        return;
    }
    previewArchiveEntry(path, myGen);
}

void QuickLookController::previewLocalOrMaterializedFile(const QString &path, int myGen)
{
        const QString displayName = cheapFileName(path);
        const int dot = displayName.lastIndexOf(QLatin1Char('.'));
        m_content.clear();
        m_type = QStringLiteral("info");
        m_extension = dot > 0 ? displayName.mid(dot + 1).toLower() : QString();
        m_name = displayName;
        m_sizeText = QStringLiteral("Loading preview...");
        m_modifiedText.clear();
        m_mimeName.clear();
        m_directory = false;
        m_hidden = false;
        m_symlink = false;
        m_readable = false;
        m_writable = false;
        m_executable = false;
        m_absolutePath = path;
        m_parentPath.clear();
        m_permissionsText.clear();
        m_attributesText.clear();
        m_lines = 0;
        m_textTruncated = false;
        m_fullTextAvailable = false;
        m_textChunked = false;
        m_textChunkIndex = 0;
        m_textChunkCount = 0;
        m_extraProperties.clear();
        resetAudioProperties();
        m_loading = true;

        emit extensionChanged();
        emit nameChanged();
        emit sizeTextChanged();
        emit modifiedTextChanged();
        emit mimeNameChanged();
        emit directoryChanged();
        emit hiddenChanged();
        emit symlinkChanged();
        emit readableChanged();
        emit writableChanged();
        emit executableChanged();
        emit absolutePathChanged();
        emit parentPathChanged();
        emit permissionsTextChanged();
        emit attributesTextChanged();
        emit linesChanged();
        emit textStateChanged();
        emit typeChanged();
        emit pathChanged();
        emit mediaSourceUrlChanged();
        emit contentChanged();
        emit extraPropertiesChanged();
        emit audioPropertiesChanged();
        emit imageSizeChanged();
        emit imageInfoChanged();
        emit bookPageStateChanged();
        emit loadingChanged();

        QPointer<QuickLookController> self(this);
        (void)QtConcurrent::run([self, path, myGen]() {
            const bool adminLocalPreview = !QFileInfo(path).isReadable()
                && !LinuxAdminBroker::activeSessionNonce().isEmpty();
            LocalPreviewData data = (FileProviderFactory::hasPluginProviderForPath(path)
                                     || adminLocalPreview)
                ? loadProviderPreviewData(path)
                : loadLocalPreviewData(path);
            if (!self) {
                if (!data.cleanupLeaseId.isEmpty()) {
                    CleanupSubsystem::instance().scheduleDeleteOnFailure(data.cleanupLeaseId);
                } else {
                    removeRemotePreviewDir(data.cleanupDir);
                }
                return;
            }

            QMetaObject::invokeMethod(self.data(), [self, path, myGen, data = std::move(data)]() mutable {
                if (!self || myGen != self->m_previewGeneration.load()) {
                    if (!data.cleanupLeaseId.isEmpty()) {
                        CleanupSubsystem::instance().scheduleDeleteOnFailure(data.cleanupLeaseId);
                    } else {
                        removeRemotePreviewDir(data.cleanupDir);
                    }
                    return;
                }

                self->m_materializedPreviewDir = std::move(data.cleanupDir);
                self->m_materializedPreviewLeaseId = std::move(data.cleanupLeaseId);
                self->m_materializedPreviewFile = std::move(data.materializedPath);
                self->m_content = std::move(data.content);
                self->m_type = std::move(data.type);
                self->m_extension = std::move(data.extension);
                self->m_name = std::move(data.name);
                self->m_sizeText = std::move(data.sizeText);
                self->m_modifiedText = std::move(data.modifiedText);
                self->m_mimeName = std::move(data.mimeName);
                self->m_absolutePath = std::move(data.absolutePath);
                self->m_parentPath = std::move(data.parentPath);
                self->m_permissionsText = std::move(data.permissionsText);
                self->m_attributesText = std::move(data.attributesText);
                self->m_extraProperties = std::move(data.extraProperties);
                self->m_directory = data.directory;
                self->m_hidden = data.hidden;
                self->m_symlink = data.symlink;
                self->m_readable = data.readable;
                self->m_writable = data.writable;
                self->m_executable = data.executable;
                self->m_lines = data.lines;
                self->m_textTruncated = data.textTruncated;
                self->m_fullTextAvailable = data.fullTextAvailable;
                self->m_textChunked = data.textChunked;
                self->m_textChunkIndex = data.textChunkIndex;
                self->m_textChunkCount = data.textChunkCount;
                self->m_bookPages = std::move(data.bookPages);
                self->m_bookParagraphs = std::move(data.bookParagraphs);
                self->m_bookPageIndex = data.bookPageIndex;
                self->m_bookCoverSource = std::move(data.bookCoverSource);
                self->m_bookTitle = std::move(data.bookTitle);
                self->m_bookAuthor = std::move(data.bookAuthor);
                self->resetAudioProperties();
                self->m_audioCoverSource = std::move(data.audioCoverSource);
                self->m_loading = false;

                emit self->extensionChanged();
                emit self->nameChanged();
                emit self->sizeTextChanged();
                emit self->modifiedTextChanged();
                emit self->mimeNameChanged();
                emit self->directoryChanged();
                emit self->hiddenChanged();
                emit self->symlinkChanged();
                emit self->readableChanged();
                emit self->writableChanged();
                emit self->executableChanged();
                emit self->absolutePathChanged();
                emit self->parentPathChanged();
                emit self->permissionsTextChanged();
                emit self->attributesTextChanged();
                emit self->linesChanged();
                emit self->textStateChanged();
                emit self->typeChanged();
                emit self->contentChanged();
                emit self->extraPropertiesChanged();
                emit self->audioPropertiesChanged();
                emit self->mediaSourceUrlChanged();
                emit self->bookPageStateChanged();
                emit self->loadingChanged();

                if (data.requestImageMetadata) {
                    self->requestImageMetadata();
                }

                if (data.requestMetadata) {
                    const QString metadataPath = data.metadataPath.isEmpty() ? path : data.metadataPath;
                    self->requestMetadata(metadataPath, myGen, 0, path);
                }
            }, Qt::QueuedConnection);
        });
}

void QuickLookController::previewArchiveEntry(const QString &path, int myGen)
{
    QLocale loc;
    const QString displayName = ArchiveSupport::archiveFileName(path);
    const QString displaySuffix = QFileInfo(displayName).suffix().toLower();
    const std::optional<FileEntry> archiveEntry = ArchiveFileProvider::cachedEntryInfo(path);

    if (archiveEntry) {
        m_name = archiveEntry->name;
        m_extension = archiveEntry->suffix;
        m_directory = archiveEntry->isDirectory;
        m_hidden = archiveEntry->isHidden;
        m_symlink = archiveEntry->isSystem;
        m_readable = true;
        m_writable = false;
        m_executable = false;
        m_absolutePath = ArchiveSupport::normalizeArchivePath(path);
        m_parentPath = ArchiveSupport::archiveParentPath(path);
    } else {
        m_name = displayName;
        m_extension = displaySuffix;
        m_directory = ArchiveSupport::archiveBrowsePath(path) == QLatin1String("/");
        m_hidden = false;
        m_symlink = false;
        m_readable = true;
        m_writable = false;
        m_executable = false;
        m_absolutePath = ArchiveSupport::normalizeArchivePath(path);
        m_parentPath = ArchiveSupport::archiveParentPath(path);
    }

    if (m_directory) {
        m_readable = true;
        m_writable = false;
        m_executable = true;
    } else {
        m_readable = true;
        m_writable = false;
        m_executable = false;
    }
    m_permissionsText.clear();
    m_attributesText.clear();

    const QString capabilityPath = m_absolutePath;
    if (!capabilityPath.isEmpty()) {
        QPointer<QuickLookController> self(this);
        (void)QtConcurrent::run([self, capabilityPath, myGen]() {
            FileCapabilityInfo capabilities = FileAccessResolver::resolve(capabilityPath);
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self.data(), [self, myGen, capabilities = std::move(capabilities)]() mutable {
                if (!self || myGen != self->m_previewGeneration.load()) {
                    return;
                }
                self->m_hidden = capabilities.attributes.hidden;
                if (capabilities.isDirectory) {
                    self->m_readable = capabilities.access.canBrowse;
                    self->m_writable = capabilities.access.canCreateChildren;
                    self->m_executable = capabilities.access.canTraverse;
                } else {
                    self->m_readable = capabilities.access.canRead;
                    self->m_writable = capabilities.access.canModify;
                    self->m_executable = capabilities.access.canExecute;
                }
                self->m_permissionsText = capabilities.accessSummary;
                self->m_attributesText = capabilities.attributesSummary;
                emit self->hiddenChanged();
                emit self->readableChanged();
                emit self->writableChanged();
                emit self->executableChanged();
                emit self->permissionsTextChanged();
                emit self->attributesTextChanged();
            }, Qt::QueuedConnection);
        });
    }

    if (archiveEntry) {
        m_sizeText = archiveEntry->isDirectory
            ? QStringLiteral("Folder")
            : DriveUtils::formatSize(archiveEntry->size);
        m_modifiedText = archiveEntry->modified.isValid()
            ? loc.toString(archiveEntry->modified, QLocale::ShortFormat)
            : QString();
    } else {
        if (m_directory) {
            m_sizeText = QStringLiteral("Folder");
        } else {
            m_sizeText.clear();
        }
        const QFileInfo physicalInfo(ArchiveSupport::physicalArchivePath(path));
        m_modifiedText = physicalInfo.exists()
            ? loc.toString(physicalInfo.lastModified(), QLocale::ShortFormat)
            : QString();
    }

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(displayName, QMimeDatabase::MatchDefault);
    m_mimeName = mime.name();
    m_extraProperties.clear();
    resetAudioProperties();
    m_textTruncated = false;
    m_fullTextAvailable = false;
    m_textChunked = false;
    m_textChunkIndex = 0;
    m_textChunkCount = 0;
    emit extraPropertiesChanged();
    emit audioPropertiesChanged();
    emit textStateChanged();

    QPointer<QuickLookController> self(this);
    const bool archiveEntryTooLarge = archiveEntry
        && !archiveEntry->isDirectory
        && archiveEntry->size > kArchivePreviewExtractLimit;
    const qint64 archiveEntrySize = archiveEntry && !archiveEntry->isDirectory
        ? archiveEntry->size
        : -1;
    const bool archiveTextPreviewAvailable = archiveEntry
        && !archiveEntry->isDirectory
        && isTextSuffix(m_extension)
        && !archiveEntryTooLarge
        && archiveEntry->size <= kArchivePreviewExtractLimit;

    if (m_directory) {
        m_mimeName = QStringLiteral("inode/directory");
        m_type = "info";

        m_content = QString("Folder: %1\nSize: %2\nModified: %3")
                        .arg(m_name)
                        .arg(m_sizeText)
                        .arg(m_modifiedText);
        m_lines = 0;

        const QFileInfo rootCheck(path);
        if (rootCheck.isRoot()) {
            if (!m_loading) {
                m_loading = true;
                emit loadingChanged();
            }
            m_extraProperties.clear();
            resetAudioProperties();
            emit extraPropertiesChanged();
            emit audioPropertiesChanged();

            (void)QtConcurrent::run([self, path, myGen]() {
                DrivePreviewData data;
                QStorageInfo storage(path);
                if (storage.isValid()) {
                    const qint64 total = storage.bytesTotal();
                    const qint64 free  = storage.bytesFree();
                    const qint64 used  = total - free;

                    {
                        QString n = path;
                        while (n.endsWith(QChar('/')) || n.endsWith(QChar('\\')))
                            n.chop(1);
                        data.name = n;
                    }
                    data.mimeName = QStringLiteral("drive");
                    data.extension = DriveUtils::detectDriveType(storage);
                    data.sizeText = DriveUtils::formatSize(total);
                    if (total > 0) {
                        const int freePct = static_cast<int>(free * 100 / total);
                        data.modifiedText = QStringLiteral("%1% free").arg(freePct);
                    } else {
                        data.modifiedText = QStringLiteral("no media");
                    }

                    data.extraProperties.append(prop(QStringLiteral("File System"), QString::fromLatin1(storage.fileSystemType())));
                    data.extraProperties.append(prop(QStringLiteral("Total Space"), DriveUtils::formatSize(total)));
                    data.extraProperties.append(prop(QStringLiteral("Free Space"),  DriveUtils::formatSize(free)));
                    data.extraProperties.append(prop(QStringLiteral("Used Space"),  DriveUtils::formatSize(used)));
                    if (total > 0) {
                        const int pct = static_cast<int>(used * 100 / total);
                        data.extraProperties.append(prop(QStringLiteral("Usage"), QStringLiteral("%1%").arg(pct)));
                    }
                    data.extraProperties.append(prop(QStringLiteral("Drive Type"), data.extension));
                }

                if (!self) return;
                QMetaObject::invokeMethod(self.data(), [self, myGen, data = std::move(data)]() mutable {
                    if (!self || myGen != self->m_previewGeneration.load()) {
                        return;
                    }
                    if (!data.name.isEmpty()) {
                        self->m_name = std::move(data.name);
                        emit self->nameChanged();
                    }
                    self->m_mimeName = std::move(data.mimeName);
                    self->m_extension = std::move(data.extension);
                    self->m_sizeText = std::move(data.sizeText);
                    self->m_modifiedText = std::move(data.modifiedText);
                    self->m_extraProperties = std::move(data.extraProperties);
                    self->resetAudioProperties();
                    self->m_loading = false;

                    emit self->mimeNameChanged();
                    emit self->extensionChanged();
                    emit self->sizeTextChanged();
                    emit self->modifiedTextChanged();
                    emit self->extraPropertiesChanged();
                    emit self->audioPropertiesChanged();
                    emit self->loadingChanged();

                    self->m_content = QString("Folder: %1\nSize: %2\nModified: %3")
                                    .arg(self->m_name)
                                    .arg(self->m_sizeText)
                                    .arg(self->m_modifiedText);
                    emit self->contentChanged();
                });
            });
        } else if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    }
#ifdef HAS_UNOFFICIAL_BIT7Z
    else if (isFb2Suffix(m_extension)) {
        m_type = QStringLiteral("book");
        m_mimeName = QStringLiteral("application/x-fictionbook+xml");
        m_content.clear();
        m_lines = 0;
        m_textTruncated = false;
        m_fullTextAvailable = false;
        m_textChunked = false;
        m_textChunkIndex = 0;
        m_textChunkCount = 0;
        emit linesChanged();
        emit textStateChanged();
        emit contentChanged();
        if (!m_loading) {
            m_loading = true;
            emit loadingChanged();
        }

        (void)QtConcurrent::run([self, path, myGen]() {
            Fb2PreviewData data = loadFb2ArchiveEntryPreviewData(path, false);
            if (!self) {
                return;
            }

            QMetaObject::invokeMethod(self.data(), [self, myGen, data = std::move(data)]() mutable {
                if (!self || myGen != self->m_previewGeneration.load()) {
                    return;
                }
                self->m_content = std::move(data.content);
                self->m_extraProperties = std::move(data.extraProperties);
                self->m_bookPages = std::move(data.pages);
                self->m_bookParagraphs = std::move(data.paragraphs);
                self->m_bookCoverSource = std::move(data.coverSource);
                self->m_bookTitle = std::move(data.title);
                self->m_bookAuthor = std::move(data.author);
                self->m_lines = data.lines;
                self->m_bookPageIndex = data.pageIndex;
                if (self->m_loading) {
                    self->m_loading = false;
                    emit self->loadingChanged();
                }
                emit self->contentChanged();
                emit self->extraPropertiesChanged();
                emit self->linesChanged();
                emit self->bookPageStateChanged();
            }, Qt::QueuedConnection);
        });
    }
#endif
    else if ((mime.name().startsWith("text/") || mime.inherits("text/plain") || mime.inherits("application/json") || mime.inherits("application/javascript") || mime.inherits("application/xml") || isTextSuffix(m_extension))
               && archiveTextPreviewAvailable) {
        m_type = "text";
        m_content.clear();
        m_lines = 0;
        m_textTruncated = false;
        m_fullTextAvailable = false;
        m_textChunked = false;
        m_textChunkIndex = 0;
        m_textChunkCount = 0;
        emit linesChanged();
        emit textStateChanged();
        emit contentChanged();
        if (!m_loading) {
            m_loading = true;
            emit loadingChanged();
        }

        (void)QtConcurrent::run([self, path, myGen, archiveEntrySize]() {
            PreviewData data;
            bool archiveEntryTooLarge = false;
            const QByteArray archiveBytes = ArchiveFileProvider::readCachedFilePrefix(
                path,
                kArchivePreviewExtractLimit,
                kTextPreviewLimit + 1,
                &archiveEntryTooLarge);
            QByteArray raw = archiveBytes.left(kTextPreviewLimit);
            data.content = QString::fromUtf8(raw);
            data.lines = data.content.isEmpty() ? 0 : data.content.count('\n') + 1;
            if (archiveEntryTooLarge || archiveBytes.size() > kTextPreviewLimit) {
                data.truncated = true;
                data.fullTextAvailable = false;
                if (!data.content.isEmpty() && !data.content.endsWith('\n')) {
                    data.content.append('\n');
                }
                data.content.append(QStringLiteral("..."));
            }
            if (archiveBytes.isEmpty() && !archiveEntryTooLarge && archiveEntrySize != 0) {
                data.content = QStringLiteral("Cannot read file.");
                data.lines = 0;
            }

            if (!self) {
                return;
            }

            QMetaObject::invokeMethod(self.data(), [self, myGen, previewData = std::move(data)]() mutable {
                if (!self || myGen != self->m_previewGeneration.load()) {
                    return;
                }
                self->m_content = std::move(previewData.content);
                self->m_lines = previewData.lines;
                self->m_textTruncated = previewData.truncated;
                self->m_fullTextAvailable = previewData.fullTextAvailable;
                self->m_textChunked = previewData.chunked;
                self->m_textChunkIndex = previewData.chunkIndex;
                self->m_textChunkCount = previewData.chunkCount;
                if (self->m_loading) {
                    self->m_loading = false;
                    emit self->loadingChanged();
                }
                emit self->linesChanged();
                emit self->textStateChanged();
                emit self->contentChanged();
            }, Qt::QueuedConnection);
        });
    } else {
        m_type = "info";
        m_content = QString("Name: %1\nSize: %2\nModified: %3")
                        .arg(m_name)
                        .arg(archiveEntryTooLarge ? QStringLiteral("Large file (%1)").arg(m_sizeText) : m_sizeText)
                        .arg(m_modifiedText);
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    }

    emit extensionChanged();
    emit nameChanged();
    emit sizeTextChanged();
    emit modifiedTextChanged();
    emit mimeNameChanged();
    emit directoryChanged();
    emit hiddenChanged();
    emit symlinkChanged();
    emit readableChanged();
    emit writableChanged();
    emit executableChanged();
    emit absolutePathChanged();
    emit parentPathChanged();
    emit permissionsTextChanged();
    emit attributesTextChanged();
    emit linesChanged();
    emit textStateChanged();
    emit typeChanged();
    emit pathChanged();
    emit mediaSourceUrlChanged();
    emit contentChanged();
    emit extraPropertiesChanged();
    emit audioPropertiesChanged();
    emit imageSizeChanged();
    emit imageInfoChanged();
    emit bookPageStateChanged();
}

void QuickLookController::setVisible(bool visible)
{
    if (m_visible == visible) return;
    m_visible = visible;
    emit visibleChanged();
}
