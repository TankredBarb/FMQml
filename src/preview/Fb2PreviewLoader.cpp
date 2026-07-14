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


} // namespace PreviewInternal
