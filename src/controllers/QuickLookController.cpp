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
#include <utility>
#include "../core/MetadataExtractor.h"


namespace {
struct PreviewData {
    QString content;
    int lines = 0;
};

static constexpr qint64 kTextPreviewLimit = 8192;

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

void QuickLookController::preview(const QString &path)
{
    if (path.isEmpty()) {
        ++m_previewGeneration;
        m_path.clear();
        m_content.clear();
        m_type.clear();
        m_extension.clear();
        m_name.clear();
        m_sizeText.clear();
        m_modifiedText.clear();
        m_mimeName.clear();
        m_directory = false;
        m_hidden = false;
        m_symlink = false;
        m_readable = false;
        m_writable = false;
        m_executable = false;
        m_absolutePath.clear();
        m_parentPath.clear();
        m_canonicalPath.clear();
        m_permissionsText.clear();
        m_lines = 0;
        m_extraProperties.clear();
        if (m_loading) {
            m_loading = false;
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
        emit linesChanged();
        emit typeChanged();
        emit pathChanged();
        emit contentChanged();
        emit extraPropertiesChanged();
        return;
    }

    if (path == m_path) {
        return;
    }

    const int myGen = ++m_previewGeneration;
    m_path = path;
    QFileInfo info(path);
    m_name = info.fileName();
    m_extension = info.suffix().toLower();
    m_directory = info.isDir();
    m_hidden = info.isHidden();
    m_symlink = info.isSymLink();
    m_readable = info.isReadable();
    m_writable = info.isWritable();
    m_executable = info.isExecutable();
    m_absolutePath = info.absoluteFilePath();
    m_parentPath = info.absolutePath();
    m_canonicalPath = info.canonicalFilePath();
    QLocale loc;
    m_sizeText = m_directory
        ? QStringLiteral("Folder")
        : loc.formattedDataSize(info.size(), 1, QLocale::DataSizeTraditionalFormat);
    m_modifiedText = loc.toString(info.lastModified(), QLocale::ShortFormat);
    QStringList permissionBits;
    if (m_readable) permissionBits << QStringLiteral("Read");
    if (m_writable) permissionBits << QStringLiteral("Write");
    if (m_executable) permissionBits << QStringLiteral("Execute");
    if (permissionBits.isEmpty()) {
        permissionBits << QStringLiteral("No access");
    }
    m_permissionsText = permissionBits.join(QStringLiteral(", "));
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(path);
    m_mimeName = mime.name();
    m_extraProperties.clear();
    emit extraPropertiesChanged();

    QPointer<QuickLookController> self(this);
    (void)QtConcurrent::run([self, path, myGen]() {
        QVariantList props = MetadataExtractor::extract(path);
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, myGen, props = std::move(props)]() {
            if (!self || myGen != self->m_previewGeneration.load()) {
                return;
            }
            self->m_extraProperties = props;
            emit self->extraPropertiesChanged();
        });
    });

    if (m_directory) {
        m_mimeName = QStringLiteral("inode/directory");
        m_type = "info";
        m_content = QString("Folder: %1\nSize: %2\nModified: %3")
                        .arg(m_name)
                        .arg(m_sizeText)
                        .arg(m_modifiedText);
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (mime.name() == "image/svg+xml" || m_extension == "svg" || m_extension == "svgz") {
        m_type = "svg";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (mime.name().startsWith("image/")) {
        m_type = "image";
        m_content = path;
        m_lines = 0;
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
    } else if (mime.name().startsWith("text/") || mime.inherits("text/plain") || mime.inherits("application/json") || mime.inherits("application/javascript") || mime.inherits("application/xml") || isTextSuffix(m_extension)) {
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
        (void)QtConcurrent::run([self, path, myGen]() {
            PreviewData data;
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
        m_content = QString("Name: %1\nSize: %2 bytes\nModified: %3")
                        .arg(info.fileName())
                        .arg(info.size())
                        .arg(info.lastModified().toString());
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
    emit linesChanged();
    emit typeChanged();
    emit pathChanged();
    emit contentChanged();
    emit extraPropertiesChanged();
}

void QuickLookController::setVisible(bool visible)
{
    if (m_visible == visible) return;
    m_visible = visible;
    emit visibleChanged();
}
