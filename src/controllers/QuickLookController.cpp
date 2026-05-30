#include "QuickLookController.h"
#include <QFileInfo>
#include <QFileDevice>
#include <QFile>
#include <QMimeDatabase>
#include <QMimeType>
#include <QDateTime>
#include <QLocale>
#include <QStringList>
#include <QMetaObject>
#include <QPointer>
#include <QImageReader>
#include <QtConcurrent/QtConcurrentRun>
#include <QBuffer>
#include <memory>
#include <utility>
#include "../core/ArchiveFileProvider.h"
#include "../core/ArchiveSupport.h"
#include "../core/FileAccessResolver.h"
#include "../core/MetadataExtractor.h"
#include "../core/DriveUtils.h"
#include "../core/IsoMountManager.h"
#include <QStorageInfo>
#include <QDir>


namespace {
struct PreviewData {
    QString content;
    int lines = 0;
};

struct DevicesPreviewData {
    QString sizeText;
    QVariantList extraProperties;
};

struct DrivePreviewData {
    QString name;
    QString extension;
    QString sizeText;
    QString modifiedText;
    QString mimeName;
    QVariantList extraProperties;
};

QVariant prop(const QString &label, const QString &value)
{
    QVariantMap m;
    m.insert(QStringLiteral("label"), label);
    m.insert(QStringLiteral("value"), value);
    return QVariant::fromValue(m);
}

static constexpr qint64 kTextPreviewLimit = 8192;
static constexpr qint64 kArchivePreviewExtractLimit = 1024 * 1024;

bool isImageSuffix(const QString &suffix)
{
    static const QStringList imageSuffixes = {
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("png"),
        QStringLiteral("gif"),
        QStringLiteral("bmp"),
        QStringLiteral("webp"),
        QStringLiteral("ico"),
        QStringLiteral("tif"),
        QStringLiteral("tiff")
    };
    return imageSuffixes.contains(suffix.toLower());
}

bool isTextSuffix(const QString &suffix)
{
    static const QStringList textSuffixes = {
        QStringLiteral("txt"),
        QStringLiteral("log"),
        QStringLiteral("md"),
        QStringLiteral("json"),
        QStringLiteral("xml"),
        QStringLiteral("csv"),
        QStringLiteral("ini"),
        QStringLiteral("conf"),
        QStringLiteral("cfg"),
        QStringLiteral("yaml"),
        QStringLiteral("yml"),
        QStringLiteral("toml"),
        QStringLiteral("js"),
        QStringLiteral("ts"),
        QStringLiteral("css"),
        QStringLiteral("html"),
        QStringLiteral("qml"),
        QStringLiteral("cpp"),
        QStringLiteral("c"),
        QStringLiteral("h"),
        QStringLiteral("hpp"),
        QStringLiteral("py"),
        QStringLiteral("java"),
        QStringLiteral("cs"),
        QStringLiteral("sh"),
        QStringLiteral("ps1"),
        QStringLiteral("svg")
    };
    return textSuffixes.contains(suffix.toLower());
}
}

QuickLookController::QuickLookController(QObject *parent)
    : QObject(parent)
{
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
QString QuickLookController::canonicalPath() const { return m_canonicalPath; }
QString QuickLookController::permissionsText() const { return m_permissionsText; }
QString QuickLookController::attributesText() const { return m_attributesText; }
int QuickLookController::lines() const { return m_lines; }
bool QuickLookController::loading() const { return m_loading; }
bool QuickLookController::visible() const { return m_visible; }
QVariantList QuickLookController::extraProperties() const { return m_extraProperties; }
bool QuickLookController::hasPdfSupport() const
{
#ifdef HAS_QT_PDF
    return true;
#else
    return false;
#endif
}

int QuickLookController::imageWidth() const { return m_imageWidth; }
int QuickLookController::imageHeight() const { return m_imageHeight; }

void QuickLookController::preview(const QString &path)
{
    previewPath(path, false);
}

void QuickLookController::refresh()
{
    if (m_path.isEmpty()) {
        return;
    }
    previewPath(m_path, true);
}

void QuickLookController::previewPath(const QString &path, bool forceReload)
{
    if (path.isEmpty() || path == QStringLiteral("devices://") || path == QStringLiteral("favorites://")) {
        const int myGen = ++m_previewGeneration;
        if (path.isEmpty()) {
            m_path.clear();
        } else {
            m_path = path; // keep virtual roots to prevent re-triggering
        }
        m_content.clear();
        m_type = QStringLiteral("info");
        m_extension.clear();
        const bool favoritesRoot = path == QStringLiteral("favorites://");
        m_name = favoritesRoot ? QStringLiteral("Favorites") : QStringLiteral("Devices and Drives");
        m_sizeText = favoritesRoot ? QStringLiteral("Pinned and frequent locations") : QStringLiteral("Detecting drives...");
        m_modifiedText.clear();
        m_mimeName.clear();
        m_directory = false;
        m_hidden = false;
        m_symlink = false;
        m_readable = true;
        m_writable = false;
        m_executable = false;
        m_absolutePath.clear();
        m_parentPath.clear();
        m_canonicalPath.clear();
        m_permissionsText.clear();
        m_attributesText.clear();
        m_lines = 0;
        m_imageWidth = 0;
        m_imageHeight = 0;
        m_extraProperties.clear();
        if (!m_loading) {
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
        emit canonicalPathChanged();
        emit permissionsTextChanged();
        emit attributesTextChanged();
        emit linesChanged();
        emit typeChanged();
        emit pathChanged();
        emit contentChanged();
        emit extraPropertiesChanged();
        emit imageSizeChanged();

        QPointer<QuickLookController> self(this);
        (void)QtConcurrent::run([self, myGen]() {
            DevicesPreviewData data;
            const QFileInfoList drives = QDir::drives();
            data.sizeText = QStringLiteral("%1 drive(s)").arg(drives.size());

            QLocale loc;
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
                        val += loc.formattedDataSize(total, 1, QLocale::DataSizeTraditionalFormat);
                        val += QStringLiteral("  |  Free: ");
                        val += loc.formattedDataSize(free, 1, QLocale::DataSizeTraditionalFormat);
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
        return;
    }

    if (path == m_path && !forceReload) {
        return;
    }

    const int myGen = ++m_previewGeneration;
    m_imageWidth = 0;
    m_imageHeight = 0;
    m_path = path;
    const bool archivePath = ArchiveSupport::isArchivePath(path);
    QLocale loc;
    const QString displayName = archivePath ? ArchiveSupport::archiveFileName(path) : QFileInfo(path).fileName();
    const QString displaySuffix = QFileInfo(displayName).suffix().toLower();
    QFileInfo info(path);
    const std::optional<FileEntry> archiveEntry = archivePath
        ? ArchiveFileProvider::cachedEntryInfo(path)
        : std::nullopt;

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
        m_canonicalPath = ArchiveSupport::physicalArchivePath(path);
    } else if (archivePath) {
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
        m_canonicalPath = ArchiveSupport::physicalArchivePath(path);
    } else {
        m_name = displayName;
        m_extension = displaySuffix;
        m_directory = info.isDir();
        m_hidden = info.isHidden();
        m_symlink = info.isSymLink();
        m_absolutePath = info.absoluteFilePath();
        m_parentPath = info.absolutePath();
        m_canonicalPath = info.canonicalFilePath();
    }

    const QString capabilityPath = archivePath ? m_absolutePath : path;
    const FileCapabilityInfo capabilities = FileAccessResolver::resolve(capabilityPath);
    m_hidden = capabilities.attributes.hidden;
    if (m_directory) {
        m_readable = capabilities.access.canBrowse;
        m_writable = capabilities.access.canCreateChildren;
        m_executable = capabilities.access.canTraverse;
    } else {
        m_readable = capabilities.access.canRead;
        m_writable = capabilities.access.canModify;
        m_executable = capabilities.access.canExecute;
    }
    m_permissionsText = capabilities.accessSummary;
    m_attributesText = capabilities.attributesSummary;

    if (archiveEntry) {
        m_sizeText = archiveEntry->isDirectory
            ? QStringLiteral("Folder")
            : loc.formattedDataSize(archiveEntry->size, 1, QLocale::DataSizeTraditionalFormat);
        m_modifiedText = archiveEntry->modified.isValid()
            ? loc.toString(archiveEntry->modified, QLocale::ShortFormat)
            : QString();
    } else if (archivePath) {
        if (m_directory) {
            m_sizeText = QStringLiteral("Folder");
        } else {
            m_sizeText.clear();
        }
        const QFileInfo physicalInfo(ArchiveSupport::physicalArchivePath(path));
        m_modifiedText = physicalInfo.exists()
            ? loc.toString(physicalInfo.lastModified(), QLocale::ShortFormat)
            : QString();
    } else {
        m_sizeText = m_directory
            ? QStringLiteral("Folder")
            : loc.formattedDataSize(info.size(), 1, QLocale::DataSizeTraditionalFormat);
        m_modifiedText = loc.toString(info.lastModified(), QLocale::ShortFormat);
    }

    QMimeDatabase db;
    QMimeType mime = archivePath
        ? db.mimeTypeForFile(displayName, QMimeDatabase::MatchDefault)
        : db.mimeTypeForFile(path);
    m_mimeName = mime.name();
    m_extraProperties.clear();
    emit extraPropertiesChanged();

    QPointer<QuickLookController> self(this);
    QByteArray archiveBytes;
    bool archiveEntryTooLarge = false;
    if (archivePath && archiveEntry && !archiveEntry->isDirectory && isTextSuffix(m_extension)) {
        archiveBytes = ArchiveFileProvider::readCachedFilePrefix(
            path,
            kArchivePreviewExtractLimit,
            kTextPreviewLimit + 1,
            &archiveEntryTooLarge);
    }
    const bool archiveTextPreviewAvailable = archivePath
        && archiveEntry
        && !archiveEntry->isDirectory
        && isTextSuffix(m_extension)
        && !archiveEntryTooLarge
        && archiveEntry->size <= kArchivePreviewExtractLimit;

    const bool isDriveRoot = QFileInfo(path).isRoot();
    if (!isDriveRoot) {
        const bool isDir = info.isDir();
        if (archivePath) {
            // Archive previews must not synchronously rescan or extract while browsing.
        } else if (isDir) {
            if (!m_loading) {
                m_loading = true;
                emit loadingChanged();
            }
        }
        if (!archivePath) {
            (void)QtConcurrent::run([self, path, myGen, isDir]() {
                QVariantList props = MetadataExtractor::extract(path);
                if (!self) return;
                QMetaObject::invokeMethod(self.data(), [self, myGen, props = std::move(props), isDir]() {
                    if (!self || myGen != self->m_previewGeneration.load()) {
                        return;
                    }
                    self->m_extraProperties = props;
                    emit self->extraPropertiesChanged();
                    if (isDir && self->m_loading) {
                        self->m_loading = false;
                        emit self->loadingChanged();
                    }
                });
            });
        }
    }

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
            emit extraPropertiesChanged();

            QPointer<QuickLookController> self(this);
            (void)QtConcurrent::run([self, path, myGen]() {
                DrivePreviewData data;
                QStorageInfo storage(path);
                if (storage.isValid()) {
                    QLocale loc;
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
                    data.sizeText = loc.formattedDataSize(total, 1, QLocale::DataSizeTraditionalFormat);
                    if (total > 0) {
                        const int freePct = static_cast<int>(free * 100 / total);
                        data.modifiedText = QStringLiteral("%1% free").arg(freePct);
                    } else {
                        data.modifiedText = QStringLiteral("no media");
                    }

                    data.extraProperties.append(prop(QStringLiteral("File System"), QString::fromLatin1(storage.fileSystemType())));
                    data.extraProperties.append(prop(QStringLiteral("Total Space"), loc.formattedDataSize(total, 1, QLocale::DataSizeTraditionalFormat)));
                    data.extraProperties.append(prop(QStringLiteral("Free Space"),  loc.formattedDataSize(free,  1, QLocale::DataSizeTraditionalFormat)));
                    data.extraProperties.append(prop(QStringLiteral("Used Space"),  loc.formattedDataSize(used,  1, QLocale::DataSizeTraditionalFormat)));
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
                    self->m_loading = false;

                    emit self->mimeNameChanged();
                    emit self->extensionChanged();
                    emit self->sizeTextChanged();
                    emit self->modifiedTextChanged();
                    emit self->extraPropertiesChanged();
                    emit self->loadingChanged();

                    self->m_content = QString("Folder: %1\nSize: %2\nModified: %3")
                                    .arg(self->m_name)
                                    .arg(self->m_sizeText)
                                    .arg(self->m_modifiedText);
                    emit self->contentChanged();
                });
            });
        }
    } else if (!archivePath && (mime.name() == "image/svg+xml" || m_extension == "svg" || m_extension == "svgz")) {
        m_type = "svg";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (mime.name().startsWith("image/") && (!archivePath || !archiveBytes.isEmpty())) {
        m_type = "image";
        m_content = path;
        m_lines = 0;
        
        QImageReader reader;
        QBuffer imageBuffer;
        if (archivePath && !archiveBytes.isEmpty()) {
            imageBuffer.setData(archiveBytes);
            imageBuffer.open(QIODevice::ReadOnly);
            reader.setDevice(&imageBuffer);
        } else {
            reader.setFileName(path);
        }
        QSize sz = reader.size();
        if (sz.isValid()) {
            m_imageWidth = sz.width();
            m_imageHeight = sz.height();
        }
        
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (mime.name() == "application/pdf" || m_extension == "pdf") {
        m_type = "pdf";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (m_extension == "ttf" || m_extension == "otf" || m_extension == "woff" || m_extension == "woff2"
               || mime.name() == "font/ttf" || mime.name() == "font/otf"
               || mime.name() == "application/font-woff" || mime.name() == "font/woff2") {
        m_type = "font";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (m_extension == "exe" || m_extension == "dll") {
        m_type = "executable";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (m_extension == "lnk") {
        m_type = "shortcut";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if ((mime.name().startsWith("text/") || mime.inherits("text/plain") || mime.inherits("application/json") || mime.inherits("application/javascript") || mime.inherits("application/xml") || isTextSuffix(m_extension))
               && (!archivePath || archiveTextPreviewAvailable)) {
        m_type = "text";
        m_content.clear();
        m_lines = 0;
        emit linesChanged();
        emit contentChanged();
        if (!m_loading) {
            m_loading = true;
            emit loadingChanged();
        }

        QPointer<QuickLookController> self(this);
        (void)QtConcurrent::run([self, path, myGen, archivePath, archiveBytes]() {
            PreviewData data;
            if (archivePath && !archiveBytes.isEmpty()) {
                QByteArray raw = archiveBytes.left(kTextPreviewLimit);
                data.content = QString::fromUtf8(raw);
                data.lines = data.content.count('\n') + 1;
                if (archiveBytes.size() > kTextPreviewLimit) {
                    if (!data.content.isEmpty() && !data.content.endsWith('\n')) {
                        data.content.append('\n');
                    }
                    data.content.append(QStringLiteral("..."));
                }
            } else if (archivePath) {
                data.content.clear();
                data.lines = 0;
            } else {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QByteArray raw = file.read(kTextPreviewLimit);
                    data.content = QString::fromUtf8(raw);
                    data.lines = data.content.count('\n') + 1;
                    if (file.size() > kTextPreviewLimit) {
                        if (!data.content.isEmpty() && !data.content.endsWith('\n')) {
                            data.content.append('\n');
                        }
                        data.content.append(QStringLiteral("..."));
                    }
                } else {
                    data.content = QStringLiteral("Cannot read file.");
                    data.lines = 0;
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
                if (self->m_loading) {
                    self->m_loading = false;
                    emit self->loadingChanged();
                }
                emit self->linesChanged();
                emit self->contentChanged();
            }, Qt::QueuedConnection);
        });
    } else if (mime.name().startsWith("audio/")) {
        m_type = "audio";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (mime.name().startsWith("video/")) {
        m_type = "video";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (mime.inherits("application/zip") || mime.inherits("application/x-tar") || mime.inherits("application/x-7z-compressed") || mime.inherits("application/x-rar-compressed")) {
        m_type = "archive";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else {
        m_type = "info";
        if (archivePath) {
            m_content = QString("Name: %1\nSize: %2\nModified: %3")
                            .arg(m_name)
                            .arg(archiveEntryTooLarge ? QStringLiteral("Large file (%1)").arg(m_sizeText) : m_sizeText)
                            .arg(m_modifiedText);
        } else {
            m_content = QString("Name: %1\nSize: %2 bytes\nModified: %3")
                            .arg(info.fileName())
                            .arg(info.size())
                            .arg(info.lastModified().toString());
        }
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
    emit canonicalPathChanged();
    emit permissionsTextChanged();
    emit attributesTextChanged();
    emit linesChanged();
    emit typeChanged();
    emit pathChanged();
    emit contentChanged();
    emit extraPropertiesChanged();
    emit imageSizeChanged();
}

void QuickLookController::setVisible(bool visible)
{
    if (m_visible == visible) return;
    m_visible = visible;
    emit visibleChanged();
}
