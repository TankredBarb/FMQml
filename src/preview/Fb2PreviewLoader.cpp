#include "Fb2PreviewLoader.h"
#include "BookPagination.h"
#include <QFileInfo>
#include <QFile>
#include <QByteArray>
#include <QStringList>
#include <QImage>
#include <QUrl>
#include <QVariantMap>
#include <QXmlStreamReader>
#include <memory>
#include "../core/ArchiveFileProvider.h"
#include "../core/ArchiveSupport.h"
#include <QDir>

namespace PreviewInternal {
QString fb2AttributeValue(const QXmlStreamAttributes &attributes, QStringView name);
QVariant bookProperty(const QString &label, const QString &value)
{
    QVariantMap item;
    item.insert(QStringLiteral("label"), label);
    item.insert(QStringLiteral("value"), value);
    return item;
}
QImage extractFb2CoverArt(QIODevice *device)
{
    if (!device || !device->isOpen()) return {};
    QString coverId;
    QXmlStreamReader xml(device);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString name = xml.name().toString();
        if (name == QLatin1String("image") && coverId.isEmpty()) {
            coverId = fb2AttributeValue(xml.attributes(), QStringLiteral("href"));
            if (coverId.startsWith(QLatin1Char('#'))) coverId.remove(0, 1);
        } else if (name == QLatin1String("binary")
                   && xml.attributes().value(QStringLiteral("id")) == coverId) {
            return QImage::fromData(QByteArray::fromBase64(
                xml.readElementText(QXmlStreamReader::IncludeChildElements).toLatin1()));
        }
    }
    return {};
}

QImage extractFb2CoverArt(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return extractFb2CoverArt(&file);
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
        data.extraProperties.append(bookProperty(QStringLiteral("Title"), title));
    }
    if (!author.isEmpty()) {
        data.extraProperties.append(bookProperty(QStringLiteral("Author"), author));
    }
    if (!genre.isEmpty()) {
        data.extraProperties.append(bookProperty(QStringLiteral("Genre"), genre));
    }
    if (!date.isEmpty()) {
        data.extraProperties.append(bookProperty(QStringLiteral("Date"), date));
    }
    if (!language.isEmpty()) {
        data.extraProperties.append(bookProperty(QStringLiteral("Language"), language));
    }
    if (!sequence.isEmpty()) {
        data.extraProperties.append(bookProperty(QStringLiteral("Series"), sequence));
    }
    if (!annotation.isEmpty()) {
        data.extraProperties.append(bookProperty(QStringLiteral("Annotation"), annotation));
    }
    if (!coverId.isEmpty()) {
        data.extraProperties.append(bookProperty(QStringLiteral("Cover"), coverId));
        data.coverSource = QStringLiteral("image://thumbnail/")
            + QString::fromUtf8(QUrl::toPercentEncoding(sourcePath + QStringLiteral("::cover")));
    }
    data.title = title;
    data.author = author;

    if (includeContent) {
        data.paragraphs = paragraphs;
        data.pages = buildBookPages(paragraphs, bookPageCharLimitForPixelSize(kFb2DefaultReaderPixelSize));
        if (!data.pages.isEmpty()) {
            data.extraProperties.append(bookProperty(QStringLiteral("Pages"), QString::number(data.pages.size())));
            data.extraProperties.append(bookProperty(QStringLiteral("Page"), QStringLiteral("1 / %1").arg(data.pages.size())));
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
            if (QFileInfo(ArchiveSupport::archiveFileName(child)).suffix()
                    .compare(QStringLiteral("fb2"), Qt::CaseInsensitive) == 0) {
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

QImage extractFb2ZipCoverArt(const QString &path)
{
    const QString entryPath = findFb2EntryInArchive(path);
    if (entryPath.isEmpty()) return {};
    ArchiveFileProvider provider;
    auto device = provider.openRead(entryPath);
    return device ? extractFb2CoverArt(device.get()) : QImage{};
}
#endif


} // namespace PreviewInternal
