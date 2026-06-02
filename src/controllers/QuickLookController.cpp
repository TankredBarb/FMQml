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
#include <QImage>
#include <QPixelFormat>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtConcurrent/QtConcurrentRun>
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
    bool truncated = false;
    bool fullTextAvailable = false;
    bool chunked = false;
    int chunkIndex = 0;
    int chunkCount = 0;
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

struct ImageMetadataData {
    QVariantList extraProperties;
    int width = 0;
    int height = 0;
    QString formatText;
    QString colorDepthText;
    QString alphaChannelText;
    QString dpiText;
    QString colorSpaceText;
    QString pixelFormatText;
};

struct LocalPreviewData {
    QString content;
    QString type;
    QString extension;
    QString name;
    QString sizeText;
    QString modifiedText;
    QString mimeName;
    QString absolutePath;
    QString parentPath;
    QString canonicalPath;
    QString permissionsText;
    QString attributesText;
    QVariantList extraProperties;
    QStringList bookPages;
    QStringList bookParagraphs;
    QString bookCoverSource;
    QString bookTitle;
    QString bookAuthor;
    bool directory = false;
    bool hidden = false;
    bool symlink = false;
    bool readable = false;
    bool writable = false;
    bool executable = false;
    int lines = 0;
    bool textTruncated = false;
    bool fullTextAvailable = false;
    bool textChunked = false;
    int textChunkIndex = 0;
    int textChunkCount = 0;
    int bookPageIndex = 0;
    bool requestMetadata = false;
    bool requestImageMetadata = false;
};

struct Fb2PreviewData {
    QString content;
    QVariantList extraProperties;
    QStringList pages;
    QStringList paragraphs;
    QString coverSource;
    QString title;
    QString author;
    int lines = 0;
    int pageIndex = 0;
};

QVariant prop(const QString &label, const QString &value)
{
    QVariantMap m;
    m.insert(QStringLiteral("label"), label);
    m.insert(QStringLiteral("value"), value);
    return QVariant::fromValue(m);
}

QString propertyValue(const QVariantList &properties, const QString &label)
{
    for (const QVariant &property : properties) {
        const QVariantMap map = property.toMap();
        if (map.value(QStringLiteral("label")).toString() == label) {
            return map.value(QStringLiteral("value")).toString();
        }
    }
    return {};
}

void setPropertyValue(QVariantList &properties, const QString &label, const QString &value)
{
    for (QVariant &property : properties) {
        QVariantMap map = property.toMap();
        if (map.value(QStringLiteral("label")).toString() == label) {
            map.insert(QStringLiteral("value"), value);
            property = map;
            return;
        }
    }
    properties.append(prop(label, value));
}

void removePropertyValue(QVariantList &properties, const QString &label)
{
    for (qsizetype i = properties.size() - 1; i >= 0; --i) {
        const QVariantMap map = properties.at(i).toMap();
        if (map.value(QStringLiteral("label")).toString() == label) {
            properties.removeAt(i);
        }
    }
}

static constexpr qint64 kTextPreviewLimit = 8192;
static constexpr qint64 kTextFullLoadLimit = 1024 * 1024;
static constexpr qint64 kTextChunkSize = 384 * 1024;
static constexpr qint64 kArchivePreviewExtractLimit = 1024 * 1024;
static constexpr int kFb2DefaultReaderPixelSize = 17;
static constexpr qsizetype kFb2PageCharLimit = 3500;
static constexpr qsizetype kFb2MaxPages = 2000;

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
        QStringLiteral("fb2"),
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

bool isFb2Suffix(const QString &suffix)
{
    return suffix.compare(QStringLiteral("fb2"), Qt::CaseInsensitive) == 0;
}

QString normalizedFb2Text(QString text)
{
    text.replace(QChar::Nbsp, QLatin1Char(' '));
    return text.simplified();
}

QString readFb2ElementText(QXmlStreamReader &xml)
{
    return normalizedFb2Text(xml.readElementText(QXmlStreamReader::IncludeChildElements));
}

QString readFb2Author(QXmlStreamReader &xml)
{
    QStringList parts;
    while (xml.readNextStartElement()) {
        const QString name = xml.name().toString();
        if (name == QLatin1String("first-name")
            || name == QLatin1String("middle-name")
            || name == QLatin1String("last-name")
            || name == QLatin1String("nickname")) {
            const QString text = readFb2ElementText(xml);
            if (!text.isEmpty()) {
                parts.append(text);
            }
        } else {
            xml.skipCurrentElement();
        }
    }
    return parts.join(QLatin1Char(' ')).simplified();
}

QString readFb2Annotation(QXmlStreamReader &xml)
{
    QStringList paragraphs;
    while (xml.readNextStartElement()) {
        const QString name = xml.name().toString();
        if (name == QLatin1String("p")
            || name == QLatin1String("subtitle")
            || name == QLatin1String("text-author")) {
            const QString text = readFb2ElementText(xml);
            if (!text.isEmpty()) {
                paragraphs.append(text);
            }
        } else {
            xml.skipCurrentElement();
        }
    }
    return paragraphs.join(QStringLiteral("\n\n")).trimmed();
}

QString fb2AttributeValue(const QXmlStreamAttributes &attributes, QStringView name)
{
    for (const QXmlStreamAttribute &attribute : attributes) {
        if (attribute.name() == name) {
            return attribute.value().toString();
        }
    }
    return {};
}

int fb2PageCharLimitForPixelSize(int pixelSize)
{
    const int normalizedSize = qBound(10, pixelSize, 28);
    return qBound(1200, (static_cast<int>(kFb2PageCharLimit) * kFb2DefaultReaderPixelSize) / normalizedSize, 7000);
}

QStringList buildFb2Pages(const QStringList &paragraphs, int pageCharLimit)
{
    QStringList pages;
    QString page;
    for (const QString &paragraph : paragraphs) {
        if (paragraph.isEmpty()) {
            continue;
        }
        const qsizetype nextSize = page.size() + paragraph.size() + (page.isEmpty() ? 0 : 2);
        if (!page.isEmpty() && nextSize > pageCharLimit) {
            pages.append(page.trimmed());
            page.clear();
            if (pages.size() >= kFb2MaxPages) {
                break;
            }
        }
        if (!page.isEmpty()) {
            page.append(QStringLiteral("\n\n"));
        }
        page.append(paragraph);
    }
    if (!page.trimmed().isEmpty() && pages.size() < kFb2MaxPages) {
        pages.append(page.trimmed());
    }
    return pages;
}

Fb2PreviewData loadFb2PreviewData(QIODevice *device, const QString &sourcePath, bool includeContent);

Fb2PreviewData loadFb2PreviewData(const QString &path, bool includeContent)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Fb2PreviewData data;
        data.content = QStringLiteral("Cannot read FB2 book.");
        data.lines = 1;
        return data;
    }
    return loadFb2PreviewData(&file, path, includeContent);
}

Fb2PreviewData loadFb2PreviewData(QIODevice *device, const QString &sourcePath, bool includeContent)
{
    Fb2PreviewData data;

    if (!device || !device->isOpen()) {
        data.content = QStringLiteral("Cannot read FB2 book.");
        data.lines = 1;
        return data;
    }

    QString title;
    QString author;
    QString genre;
    QString date;
    QString language;
    QString sequence;
    QString annotation;
    QString coverId;
    QStringList paragraphs;
    bool inTitleInfo = false;
    bool inBody = false;

    QXmlStreamReader xml(device);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (name == QLatin1String("title-info")) {
                inTitleInfo = true;
                continue;
            }
            if (name == QLatin1String("body")) {
                inBody = true;
                continue;
            }

            if (inTitleInfo) {
                if (name == QLatin1String("book-title")) {
                    title = readFb2ElementText(xml);
                } else if (name == QLatin1String("author")) {
                    author = readFb2Author(xml);
                } else if (name == QLatin1String("genre")) {
                    genre = readFb2ElementText(xml);
                } else if (name == QLatin1String("date")) {
                    date = readFb2ElementText(xml);
                } else if (name == QLatin1String("lang")) {
                    language = readFb2ElementText(xml);
                } else if (name == QLatin1String("sequence")) {
                    const QXmlStreamAttributes attributes = xml.attributes();
                    sequence = attributes.value(QStringLiteral("name")).toString().trimmed();
                    const QString number = attributes.value(QStringLiteral("number")).toString().trimmed();
                    if (!sequence.isEmpty() && !number.isEmpty()) {
                        sequence += QStringLiteral(" #") + number;
                    }
                } else if (name == QLatin1String("image") && coverId.isEmpty()) {
                    coverId = fb2AttributeValue(xml.attributes(), QStringLiteral("href"));
                    if (coverId.startsWith(QLatin1Char('#'))) {
                        coverId.remove(0, 1);
                    }
                } else if (name == QLatin1String("annotation")) {
                    annotation = readFb2Annotation(xml);
                }
                continue;
            }

            if (inBody
                && includeContent
                && (name == QLatin1String("p")
                    || name == QLatin1String("subtitle")
                    || name == QLatin1String("text-author"))) {
                const QString text = readFb2ElementText(xml);
                if (!text.isEmpty()) {
                    paragraphs.append(text);
                }
            }
        } else if (xml.isEndElement()) {
            const QString name = xml.name().toString();
            if (name == QLatin1String("title-info")) {
                inTitleInfo = false;
                if (!includeContent) {
                    break;
                }
            } else if (name == QLatin1String("body")) {
                inBody = false;
            }
        }
    }

    if (!title.isEmpty()) {
        data.extraProperties.append(prop(QStringLiteral("Title"), title));
    }
    if (!author.isEmpty()) {
        data.extraProperties.append(prop(QStringLiteral("Author"), author));
    }
    if (!genre.isEmpty()) {
        data.extraProperties.append(prop(QStringLiteral("Genre"), genre));
    }
    if (!date.isEmpty()) {
        data.extraProperties.append(prop(QStringLiteral("Date"), date));
    }
    if (!language.isEmpty()) {
        data.extraProperties.append(prop(QStringLiteral("Language"), language));
    }
    if (!sequence.isEmpty()) {
        data.extraProperties.append(prop(QStringLiteral("Series"), sequence));
    }
    if (!annotation.isEmpty()) {
        data.extraProperties.append(prop(QStringLiteral("Annotation"), annotation));
    }
    if (!coverId.isEmpty()) {
        data.extraProperties.append(prop(QStringLiteral("Cover"), coverId));
        data.coverSource = QStringLiteral("image://thumbnail/")
            + QString::fromUtf8(QUrl::toPercentEncoding(sourcePath + QStringLiteral("::cover")));
    }
    data.title = title;
    data.author = author;

    if (includeContent) {
        data.paragraphs = paragraphs;
        data.pages = buildFb2Pages(paragraphs, fb2PageCharLimitForPixelSize(kFb2DefaultReaderPixelSize));
        if (!data.pages.isEmpty()) {
            data.extraProperties.append(prop(QStringLiteral("Pages"), QString::number(data.pages.size())));
            data.extraProperties.append(prop(QStringLiteral("Page"), QStringLiteral("1 / %1").arg(data.pages.size())));
        }

        data.content = data.pages.isEmpty() ? QString() : data.pages.first();
        if (data.content.isEmpty() && !annotation.isEmpty()) {
            data.content = annotation;
        }
        if (data.content.isEmpty()) {
            data.content = xml.hasError()
                ? QStringLiteral("Cannot parse FB2 book.")
                : QStringLiteral("No readable book text found.");
        }
    }

    data.lines = data.content.isEmpty() ? 0 : data.content.count(QLatin1Char('\n')) + 1;
    return data;
}

bool isFb2ZipPath(const QString &path)
{
#ifdef HAS_UNOFFICIAL_BIT7Z
    const QString normalized = QDir::fromNativeSeparators(path).toLower();
    return normalized.endsWith(QStringLiteral(".fb2.zip"));
#else
    Q_UNUSED(path)
    return false;
#endif
}

#ifdef HAS_UNOFFICIAL_BIT7Z
Fb2PreviewData loadFb2ArchiveEntryPreviewData(const QString &entryPath, bool includeContent)
{
    Fb2PreviewData data;
    ArchiveFileProvider provider;
    std::unique_ptr<QIODevice> device = provider.openRead(entryPath);
    if (!device) {
        data.content = QStringLiteral("Cannot read FB2 book from archive.");
        data.lines = 1;
        return data;
    }

    return loadFb2PreviewData(device.get(), entryPath, includeContent);
}

QString findFb2EntryInArchive(const QString &archivePath)
{
    ArchiveFileProvider provider;
    const QString rootPath = ArchiveSupport::archiveRootPath(archivePath);
    QStringList pending{rootPath};
    QString firstFb2;

    while (!pending.isEmpty()) {
        const QString current = pending.takeFirst();
        const QStringList children = provider.childPaths(current, true);
        for (const QString &child : children) {
            if (provider.isDirectory(child)) {
                pending.append(child);
                continue;
            }
            if (isFb2Suffix(QFileInfo(ArchiveSupport::archiveFileName(child)).suffix())) {
                if (firstFb2.isEmpty()) {
                    firstFb2 = child;
                }
                const QString baseName = QFileInfo(archivePath).completeBaseName();
                const QString entryBaseName = QFileInfo(ArchiveSupport::archiveFileName(child)).completeBaseName();
                if (entryBaseName.compare(baseName, Qt::CaseInsensitive) == 0) {
                    return child;
                }
            }
        }
    }

    return firstFb2;
}

Fb2PreviewData loadFb2ZipPreviewData(const QString &path, bool includeContent)
{
    Fb2PreviewData data;
    const QString entryPath = findFb2EntryInArchive(path);
    if (entryPath.isEmpty()) {
        data.content = QStringLiteral("No FB2 book found in archive.");
        data.lines = 1;
        return data;
    }

    return loadFb2ArchiveEntryPreviewData(entryPath, includeContent);
}
#endif

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
    if (imageFormat != QImage::Format_Invalid) {
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
    data.canonicalPath = info.canonicalFilePath();
    data.readable = info.isReadable();
    data.writable = info.isWritable();
    data.executable = info.isExecutable();
    data.mimeName = mime.name();
    data.sizeText = data.directory
        ? QStringLiteral("Folder")
        : loc.formattedDataSize(info.size(), 1, QLocale::DataSizeTraditionalFormat);
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
                data.sizeText = loc.formattedDataSize(total, 1, QLocale::DataSizeTraditionalFormat);
                data.modifiedText = total > 0
                    ? QStringLiteral("%1% free").arg(static_cast<int>(free * 100 / total))
                    : QStringLiteral("no media");
                data.extraProperties.append(prop(QStringLiteral("File System"), QString::fromLatin1(storage.fileSystemType())));
                data.extraProperties.append(prop(QStringLiteral("Total Space"), loc.formattedDataSize(total, 1, QLocale::DataSizeTraditionalFormat)));
                data.extraProperties.append(prop(QStringLiteral("Free Space"),  loc.formattedDataSize(free,  1, QLocale::DataSizeTraditionalFormat)));
                data.extraProperties.append(prop(QStringLiteral("Used Space"),  loc.formattedDataSize(used,  1, QLocale::DataSizeTraditionalFormat)));
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
    const bool isImage = mime.name().startsWith(QStringLiteral("image/"));
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
            data.content = QStringLiteral("Cannot read file.");
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
QString QuickLookController::mediaSourceUrl() const
{
    if (m_path.isEmpty() || ArchiveSupport::isArchivePath(m_path)) {
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

void QuickLookController::syncImageInfo(const QString &path)
{
    resetImageInfo();

    QImageReader reader(path);
    reader.setAutoTransform(false);

    const QByteArray format = reader.format();
    if (!format.isEmpty()) {
        m_imageFormatText = QString::fromLatin1(format).toUpper();
    }

    const QSize size = reader.size();
    if (size.isValid()) {
        m_imageWidth = size.width();
        m_imageHeight = size.height();
    }

    QImage::Format imageFormat = reader.imageFormat();
    if (imageFormat != QImage::Format_Invalid) {
        m_imagePixelFormatText = imageFormatName(imageFormat);
        const QPixelFormat pixelFormat = QImage::toPixelFormat(imageFormat);
        const int depth = pixelFormat.bitsPerPixel();
        if (depth > 0) {
            m_imageColorDepthText = QStringLiteral("%1 bit").arg(depth);
            m_imageAlphaChannelText = pixelFormat.alphaUsage() == QPixelFormat::UsesAlpha
                ? QStringLiteral("Yes")
                : QStringLiteral("No");
        }
    }
}

void QuickLookController::syncImageProperties(const QVariantList &properties)
{
    const QString format = propertyValue(properties, QStringLiteral("Format"));
    const QString colorDepth = propertyValue(properties, QStringLiteral("Color Depth"));
    const QString alpha = propertyValue(properties, QStringLiteral("Alpha Channel"));
    const QString dpi = propertyValue(properties, QStringLiteral("DPI"));
    const QString colorSpace = propertyValue(properties, QStringLiteral("Color Space"));
    const QString pixelFormat = propertyValue(properties, QStringLiteral("Pixel Format"));

    if (!format.isEmpty()) {
        m_imageFormatText = format;
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
    if (!imageMetadataRequested()
        || m_imageMetadataLoading
        || m_type != QStringLiteral("image")
        || m_path.isEmpty()
        || ArchiveSupport::isArchivePath(m_path)
        || m_imageMetadataLoadedPath == m_path) {
        return;
    }

    const QString path = m_path;
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

void QuickLookController::preview(const QString &path)
{
    previewPath(path, false);
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
            data.content = QStringLiteral("Cannot read file.");
            data.lines = 0;
            data.truncated = false;
            data.fullTextAvailable = false;
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
            data.content = QStringLiteral("Cannot read file.");
            data.lines = 0;
            data.truncated = false;
            data.fullTextAvailable = false;
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

    const QString path = m_path;
    const int myGen = m_previewGeneration.load();
    const int myBookGen = ++m_bookContentGeneration;
    m_bookContentLoading = true;
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged();
    }

    QPointer<QuickLookController> self(this);
    (void)QtConcurrent::run([self, path, myGen, myBookGen]() {
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

        QMetaObject::invokeMethod(self.data(), [self, path, myGen, myBookGen, data = std::move(data)]() mutable {
            if (!self
                || myGen != self->m_previewGeneration.load()
                || myBookGen != self->m_bookContentGeneration
                || self->m_path != path) {
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
    m_canonicalPath.clear();
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
    emit canonicalPathChanged();
    emit permissionsTextChanged();
    emit attributesTextChanged();
    emit linesChanged();
    emit textStateChanged();
    emit loadingChanged();
    emit typeChanged();
    emit pathChanged();
    emit contentChanged();
    emit extraPropertiesChanged();
    emit audioPropertiesChanged();
    emit imageSizeChanged();
    emit imageInfoChanged();
    emit bookPageStateChanged();

    QPointer<QuickLookController> self(this);
    (void)QtConcurrent::run([self, paths, myGen]() {
        QLocale loc;
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
            ? loc.formattedDataSize(totalSize, 1, QLocale::DataSizeTraditionalFormat)
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
        m_textTruncated = false;
        m_fullTextAvailable = false;
        m_textChunked = false;
        m_textChunkIndex = 0;
        m_textChunkCount = 0;
        resetImageInfo();
        resetBookInfo();
        m_extraProperties.clear();
        resetAudioProperties();
        if (favoritesRoot) {
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
        emit canonicalPathChanged();
        emit permissionsTextChanged();
        emit attributesTextChanged();
        emit linesChanged();
        emit textStateChanged();
        emit typeChanged();
        emit pathChanged();
        emit contentChanged();
        emit extraPropertiesChanged();
        emit audioPropertiesChanged();
        emit imageSizeChanged();
        emit imageInfoChanged();
        emit bookPageStateChanged();

        if (favoritesRoot) {
            return;
        }

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
    resetImageInfo();
    resetBookInfo();
    m_imageMetadataLoading = false;
    m_imageMetadataLoadedPath.clear();
    m_path = path;
    const bool archivePath = ArchiveSupport::isArchivePath(path);
    if (!archivePath) {
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
        m_canonicalPath.clear();
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
        emit canonicalPathChanged();
        emit permissionsTextChanged();
        emit attributesTextChanged();
        emit linesChanged();
        emit textStateChanged();
        emit typeChanged();
        emit pathChanged();
        emit contentChanged();
        emit extraPropertiesChanged();
        emit audioPropertiesChanged();
        emit imageSizeChanged();
        emit imageInfoChanged();
        emit bookPageStateChanged();
        emit loadingChanged();

        QPointer<QuickLookController> self(this);
        (void)QtConcurrent::run([self, path, myGen]() {
            LocalPreviewData data = loadLocalPreviewData(path);
            if (!self) {
                return;
            }

            QMetaObject::invokeMethod(self.data(), [self, path, myGen, data = std::move(data)]() mutable {
                if (!self || myGen != self->m_previewGeneration.load()) {
                    return;
                }

                self->m_content = std::move(data.content);
                self->m_type = std::move(data.type);
                self->m_extension = std::move(data.extension);
                self->m_name = std::move(data.name);
                self->m_sizeText = std::move(data.sizeText);
                self->m_modifiedText = std::move(data.modifiedText);
                self->m_mimeName = std::move(data.mimeName);
                self->m_absolutePath = std::move(data.absolutePath);
                self->m_parentPath = std::move(data.parentPath);
                self->m_canonicalPath = std::move(data.canonicalPath);
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
                emit self->canonicalPathChanged();
                emit self->permissionsTextChanged();
                emit self->attributesTextChanged();
                emit self->linesChanged();
                emit self->textStateChanged();
                emit self->typeChanged();
                emit self->contentChanged();
                emit self->extraPropertiesChanged();
                emit self->audioPropertiesChanged();
                emit self->bookPageStateChanged();
                emit self->loadingChanged();

                if (data.requestImageMetadata) {
                    self->requestImageMetadata();
                }

                if (data.requestMetadata) {
                    QPointer<QuickLookController> metaSelf(self.data());
                    (void)QtConcurrent::run([metaSelf, path, myGen]() {
                        QVariantList props = MetadataExtractor::extract(path);
                        if (!metaSelf) return;
                        QMetaObject::invokeMethod(metaSelf.data(), [metaSelf, myGen, props = std::move(props)]() mutable {
                            if (!metaSelf || myGen != metaSelf->m_previewGeneration.load()) {
                                return;
                            }
                            metaSelf->m_extraProperties = props;
                            if (metaSelf->m_type == QStringLiteral("audio")) {
                                metaSelf->syncAudioProperties(props);
                                emit metaSelf->audioPropertiesChanged();
                            } else if (metaSelf->m_type == QStringLiteral("image")) {
                                metaSelf->syncImageProperties(props);
                                emit metaSelf->imageInfoChanged();
                            }
                            emit metaSelf->extraPropertiesChanged();
                        });
                    });
                }
            }, Qt::QueuedConnection);
        });
        return;
    }

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

    if (m_directory) {
        m_readable = archivePath || info.isReadable();
        m_writable = !archivePath && info.isWritable();
        m_executable = archivePath || info.isExecutable();
    } else {
        m_readable = archivePath || info.isReadable();
        m_writable = !archivePath && info.isWritable();
        m_executable = !archivePath && info.isExecutable();
    }
    m_permissionsText.clear();
    m_attributesText.clear();

    const QString capabilityPath = archivePath ? m_absolutePath : path;
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
    const bool archiveEntryTooLarge = archivePath
        && archiveEntry
        && !archiveEntry->isDirectory
        && archiveEntry->size > kArchivePreviewExtractLimit;
    const qint64 archiveEntrySize = archiveEntry && !archiveEntry->isDirectory
        ? archiveEntry->size
        : -1;
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
        }
        const bool isImageMetadataFile = mime.name().startsWith("image/")
            && mime.name() != QStringLiteral("image/svg+xml")
            && displaySuffix != QStringLiteral("svg")
            && displaySuffix != QStringLiteral("svgz");
        if (!archivePath && !isDir && !isImageMetadataFile) {
            (void)QtConcurrent::run([self, path, myGen]() {
                QVariantList props = MetadataExtractor::extract(path);
                if (!self) return;
                QMetaObject::invokeMethod(self.data(), [self, myGen, props = std::move(props)]() {
                    if (!self || myGen != self->m_previewGeneration.load()) {
                        return;
                    }
                    self->m_extraProperties = props;
                    if (self->m_type == QStringLiteral("audio")) {
                        self->syncAudioProperties(props);
                        emit self->audioPropertiesChanged();
                    } else if (self->m_type == QStringLiteral("image")) {
                        self->syncImageProperties(props);
                        emit self->imageInfoChanged();
                    }
                    emit self->extraPropertiesChanged();
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
            resetAudioProperties();
            emit extraPropertiesChanged();
            emit audioPropertiesChanged();

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
    } else if (!archivePath && (mime.name() == "image/svg+xml" || m_extension == "svg" || m_extension == "svgz")) {
        m_type = "svg";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (!archivePath && mime.name().startsWith("image/")) {
        m_type = "image";
        m_content = path;
        m_lines = 0;
        requestImageMetadata();
        
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (!archivePath && (mime.name() == "application/pdf" || m_extension == "pdf")) {
        m_type = "pdf";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (!archivePath
               && (m_extension == "ttf" || m_extension == "otf" || m_extension == "woff" || m_extension == "woff2"
               || (m_extension != "fon"
                   && (mime.name() == "font/ttf" || mime.name() == "font/otf"
                       || mime.name() == "application/font-woff" || mime.name() == "font/woff2")))) {
        m_type = "font";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (!archivePath && (m_extension == "exe" || m_extension == "dll" || m_extension == "msi")) {
        m_type = "executable";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    } else if (!archivePath && m_extension == "lnk") {
        m_type = "shortcut";
        m_content = path;
        m_lines = 0;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
    }
#ifdef HAS_UNOFFICIAL_BIT7Z
    else if (archivePath && isFb2Suffix(m_extension)) {
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

        QPointer<QuickLookController> self(this);
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
               && (!archivePath || archiveTextPreviewAvailable)) {
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

        QPointer<QuickLookController> self(this);
        (void)QtConcurrent::run([self, path, myGen, archivePath, archiveEntrySize]() {
            PreviewData data;
            if (archivePath) {
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
            } else {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly)) {
                    QByteArray raw = file.read(kTextPreviewLimit);
                    data.content = QString::fromUtf8(raw);
                    data.lines = data.content.count('\n') + 1;
                    if (file.size() > kTextPreviewLimit) {
                        data.truncated = true;
                        data.fullTextAvailable = true;
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
    } else if (archivePath) {
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
    emit textStateChanged();
    emit typeChanged();
    emit pathChanged();
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
