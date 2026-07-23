#include "EpubPreviewLoader.h"

#include <algorithm>
#include <memory>
#include <QDir>
#include <QHash>
#include <QUrl>
#include <QXmlStreamReader>

#ifndef FM_EPUB_PREVIEW_PARSER_ONLY
#include <QIODevice>
#include <QImage>
#include <QVariantMap>
#include "Fb2PreviewLoader.h"
#include "../core/ArchiveFileProvider.h"
#include "../core/ArchiveSupport.h"
#endif

namespace PreviewInternal {
namespace {

QString normalizedText(QString text)
{
    text.replace(QChar::Nbsp, QLatin1Char(' '));
    return text.simplified();
}

QString resolveEpubPath(const QString &packagePath, QString href)
{
    const int fragmentIndex = href.indexOf(QLatin1Char('#'));
    if (fragmentIndex >= 0) {
        href.truncate(fragmentIndex);
    }
    const int queryIndex = href.indexOf(QLatin1Char('?'));
    if (queryIndex >= 0) {
        href.truncate(queryIndex);
    }
    href = QUrl::fromPercentEncoding(href.toUtf8());
    if (href.isEmpty()) {
        return {};
    }

    const int slash = packagePath.lastIndexOf(QLatin1Char('/'));
    const QString basePath = slash >= 0 ? packagePath.left(slash + 1) : QString();
    QString path = QDir::cleanPath(basePath + href);
    while (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }
    return path;
}

bool hasProperty(const QString &properties, QStringView property)
{
    const QStringList values = properties.split(QChar::Space, Qt::SkipEmptyParts);
    return std::any_of(values.cbegin(), values.cend(), [property](const QString &value) {
        return value.compare(property, Qt::CaseInsensitive) == 0;
    });
}

QString readElementText(QXmlStreamReader &xml)
{
    return normalizedText(xml.readElementText(QXmlStreamReader::IncludeChildElements));
}

#ifndef FM_EPUB_PREVIEW_PARSER_ONLY
QVariant property(const QString &label, const QString &value)
{
    QVariantMap valueMap;
    valueMap.insert(QStringLiteral("label"), label);
    valueMap.insert(QStringLiteral("value"), value);
    return valueMap;
}

constexpr qint64 kEpubEntryReadLimit = 8 * 1024 * 1024;

QByteArray readEpubEntry(ArchiveFileProvider &provider, const QString &path, QString *error)
{
    std::unique_ptr<QIODevice> device = provider.openRead(path);
    if (!device) {
        if (error) {
            *error = QStringLiteral("EPUB entry was not found.");
        }
        return {};
    }

    QByteArray contents = device->read(kEpubEntryReadLimit + 1);
    if (contents.size() > kEpubEntryReadLimit) {
        if (error) {
            *error = QStringLiteral("EPUB entry is too large.");
        }
        return {};
    }
    return contents;
}

QStringList readEpubXhtmlParagraphs(const QByteArray &contents)
{
    QStringList paragraphs;
    QXmlStreamReader xml(contents);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) {
            continue;
        }

        const QStringView name = xml.name();
        if (name == QLatin1String("p")
            || name == QLatin1String("li")
            || name == QLatin1String("blockquote")
            || name == QLatin1String("h1")
            || name == QLatin1String("h2")
            || name == QLatin1String("h3")
            || name == QLatin1String("h4")
            || name == QLatin1String("h5")
            || name == QLatin1String("h6")) {
            const QString text = readElementText(xml);
            if (!text.isEmpty()) {
                paragraphs.append(text);
            }
        }
    }
    return paragraphs;
}
#endif

} // namespace

EpubPackageData parseEpubPackageData(const QByteArray &containerXml,
                                     const QByteArray &packageXml)
{
    EpubPackageData data;

    QXmlStreamReader container(containerXml);
    while (!container.atEnd()) {
        container.readNext();
        if (!container.isStartElement() || container.name() != QLatin1String("rootfile")) {
            continue;
        }

        data.packagePath = container.attributes().value(QStringLiteral("full-path")).toString().trimmed();
        if (!data.packagePath.isEmpty()) {
            break;
        }
    }
    if (data.packagePath.isEmpty()) {
        data.error = container.hasError()
            ? QStringLiteral("Cannot parse EPUB container.")
            : QStringLiteral("EPUB package document was not found.");
        return data;
    }
    if (container.hasError()) {
        data.error = QStringLiteral("Cannot parse EPUB container.");
        return data;
    }
    if (packageXml.isEmpty()) {
        return data;
    }

    struct ManifestItem {
        QString href;
        QString properties;
    };
    QHash<QString, ManifestItem> manifest;
    QString legacyCoverId;
    QStringList creators;
    bool inMetadata = false;
    bool inSpine = false;

    QXmlStreamReader package(packageXml);
    while (!package.atEnd()) {
        package.readNext();
        if (package.isStartElement()) {
            const QStringView name = package.name();
            if (name == QLatin1String("metadata")) {
                inMetadata = true;
                continue;
            }
            if (name == QLatin1String("spine")) {
                inSpine = true;
                continue;
            }
            if (name == QLatin1String("item")) {
                const QString id = package.attributes().value(QStringLiteral("id")).toString().trimmed();
                const QString href = package.attributes().value(QStringLiteral("href")).toString().trimmed();
                if (!id.isEmpty() && !href.isEmpty()) {
                    manifest.insert(id, {href, package.attributes().value(QStringLiteral("properties")).toString()});
                }
                continue;
            }
            if (inSpine && name == QLatin1String("itemref")) {
                const QString idref = package.attributes().value(QStringLiteral("idref")).toString().trimmed();
                if (!idref.isEmpty()) {
                    data.spinePaths.append(idref);
                }
                continue;
            }
            if (inMetadata && name == QLatin1String("meta")) {
                const QString metaName = package.attributes().value(QStringLiteral("name")).toString();
                if (metaName.compare(QStringLiteral("cover"), Qt::CaseInsensitive) == 0) {
                    legacyCoverId = package.attributes().value(QStringLiteral("content")).toString().trimmed();
                }
                package.skipCurrentElement();
                continue;
            }
            if (!inMetadata) {
                continue;
            }

            const QString text = readElementText(package);
            if (name == QLatin1String("title") && data.title.isEmpty()) {
                data.title = text;
            } else if (name == QLatin1String("creator")) {
                if (!text.isEmpty()) {
                    creators.append(text);
                }
            } else if (name == QLatin1String("subject") && data.genre.isEmpty()) {
                data.genre = text;
            } else if (name == QLatin1String("date") && data.date.isEmpty()) {
                data.date = text;
            } else if (name == QLatin1String("language") && data.language.isEmpty()) {
                data.language = text;
            } else if (name == QLatin1String("description") && data.annotation.isEmpty()) {
                data.annotation = text;
            }
        } else if (package.isEndElement()) {
            if (package.name() == QLatin1String("metadata")) {
                inMetadata = false;
            } else if (package.name() == QLatin1String("spine")) {
                inSpine = false;
            }
        }
    }

    if (package.hasError()) {
        data.error = QStringLiteral("Cannot parse EPUB package document.");
        return data;
    }

    data.author = creators.join(QStringLiteral(", "));
    for (QString &idref : data.spinePaths) {
        const auto item = manifest.constFind(idref);
        idref = item == manifest.cend() ? QString() : resolveEpubPath(data.packagePath, item->href);
    }
    data.spinePaths.removeAll({});

    for (auto item = manifest.cbegin(); item != manifest.cend(); ++item) {
        if (hasProperty(item->properties, QStringLiteral("cover-image"))) {
            data.coverPath = resolveEpubPath(data.packagePath, item->href);
            break;
        }
    }
    if (data.coverPath.isEmpty()) {
        const auto item = manifest.constFind(legacyCoverId);
        if (item != manifest.cend()) {
            data.coverPath = resolveEpubPath(data.packagePath, item->href);
        }
    }

    return data;
}

#ifndef FM_EPUB_PREVIEW_PARSER_ONLY
EpubPreviewData loadEpubPreviewData(const QString &path, bool includeContent)
{
    EpubPreviewData data;
    ArchiveFileProvider provider;
    const QString rootPath = ArchiveSupport::archiveRootPath(path);
    QString error;
    const QByteArray containerXml = readEpubEntry(
        provider, ArchiveSupport::archiveChildPath(rootPath, QStringLiteral("META-INF/container.xml")), &error);
    if (containerXml.isEmpty()) {
        data.content = error.isEmpty() ? QStringLiteral("Cannot read EPUB container.") : error;
        data.lines = 1;
        return data;
    }

    EpubPackageData package = parseEpubPackageData(containerXml, {});
    if (!package.error.isEmpty()) {
        data.content = package.error;
        data.lines = 1;
        return data;
    }

    const QByteArray packageXml = readEpubEntry(
        provider, ArchiveSupport::archiveChildPath(rootPath, package.packagePath), &error);
    if (packageXml.isEmpty()) {
        data.content = error.isEmpty() ? QStringLiteral("Cannot read EPUB package document.") : error;
        data.lines = 1;
        return data;
    }
    package = parseEpubPackageData(containerXml, packageXml);
    if (!package.error.isEmpty()) {
        data.content = package.error;
        data.lines = 1;
        return data;
    }

    auto addProperty = [&data](const QString &label, const QString &value) {
        if (!value.isEmpty()) {
            data.extraProperties.append(property(label, value));
        }
    };
    addProperty(QStringLiteral("Title"), package.title);
    addProperty(QStringLiteral("Author"), package.author);
    addProperty(QStringLiteral("Genre"), package.genre);
    addProperty(QStringLiteral("Date"), package.date);
    addProperty(QStringLiteral("Language"), package.language);
    addProperty(QStringLiteral("Annotation"), package.annotation);
    data.title = package.title;
    data.author = package.author;
    if (!package.coverPath.isEmpty()) {
        data.coverSource = QStringLiteral("image://thumbnail/")
            + QString::fromUtf8(QUrl::toPercentEncoding(path + QStringLiteral("::cover")));
    }

    if (!includeContent) {
        data.content = package.annotation;
        data.lines = data.content.isEmpty() ? 0 : data.content.count(QLatin1Char('\n')) + 1;
        return data;
    }

    for (const QString &spinePath : package.spinePaths) {
        const QByteArray chapter = readEpubEntry(
            provider, ArchiveSupport::archiveChildPath(rootPath, spinePath), &error);
        if (chapter.isEmpty()) {
            continue;
        }
        data.paragraphs.append(readEpubXhtmlParagraphs(chapter));
    }
    data.pages = buildFb2Pages(data.paragraphs, fb2PageCharLimitForPixelSize(kFb2DefaultReaderPixelSize));
    if (!data.pages.isEmpty()) {
        data.extraProperties.append(property(QStringLiteral("Pages"), QString::number(data.pages.size())));
        data.extraProperties.append(property(QStringLiteral("Page"), QStringLiteral("1 / %1").arg(data.pages.size())));
        data.content = data.pages.first();
    } else if (!package.annotation.isEmpty()) {
        data.content = package.annotation;
    } else {
        data.content = QStringLiteral("No readable book text found.");
    }
    data.lines = data.content.count(QLatin1Char('\n')) + 1;
    return data;
}

QImage extractEpubCoverArt(const QString &path)
{
    ArchiveFileProvider provider;
    const QString rootPath = ArchiveSupport::archiveRootPath(path);
    QString error;
    const QByteArray containerXml = readEpubEntry(
        provider, ArchiveSupport::archiveChildPath(rootPath, QStringLiteral("META-INF/container.xml")), &error);
    if (containerXml.isEmpty()) {
        return {};
    }

    EpubPackageData package = parseEpubPackageData(containerXml, {});
    if (!package.error.isEmpty()) {
        return {};
    }
    const QByteArray packageXml = readEpubEntry(
        provider, ArchiveSupport::archiveChildPath(rootPath, package.packagePath), &error);
    if (packageXml.isEmpty()) {
        return {};
    }
    package = parseEpubPackageData(containerXml, packageXml);
    if (!package.error.isEmpty() || package.coverPath.isEmpty()) {
        return {};
    }

    return QImage::fromData(readEpubEntry(
        provider, ArchiveSupport::archiveChildPath(rootPath, package.coverPath), &error));
}
#endif

} // namespace PreviewInternal
