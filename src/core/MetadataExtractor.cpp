#include "MetadataExtractor.h"
#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#include "DriveUtils.h"

#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QImage>
#include <QColorSpace>
#include <QPixelFormat>
#include <QMimeDatabase>
#include <QRawFont>
#include <QXmlStreamReader>
#include <QDataStream>
#include <QTextStream>
#include <QtMath>
#include <limits>

#ifdef HAS_UNOFFICIAL_BIT7Z
#include <bit7z/bit7z.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitarchiveitem.hpp>
#include <exception>
#endif

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#pragma comment(lib, "Version.lib")
#endif

#ifdef HAS_QT_PDF
#include <QPdfDocument>
#endif

#ifdef HAS_TAGLIB
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#endif

// ─── Dispatch ────────────────────────────────────────────────────────────────

namespace {
bool metadataIsDjvuDocument(const QString &suffix, const QString &mimeName)
{
    const QString lowerSuffix = suffix.toLower();
    const QString lowerMime = mimeName.toLower();
    return lowerSuffix == QLatin1String("djvu")
        || lowerSuffix == QLatin1String("djv")
        || lowerMime == QLatin1String("image/vnd.djvu")
        || lowerMime == QLatin1String("image/vnd.djvu+multipage")
        || lowerMime == QLatin1String("image/x-djvu")
        || lowerMime == QLatin1String("application/x-djvu");
}

QString metadataImageFormatName(QImage::Format format)
{
    switch (format) {
    case QImage::Format_Invalid: return {};
    case QImage::Format_Mono: return QStringLiteral("Mono");
    case QImage::Format_MonoLSB: return QStringLiteral("Mono LSB");
    case QImage::Format_Indexed8: return QStringLiteral("Indexed8");
    case QImage::Format_RGB32: return QStringLiteral("RGB32");
    case QImage::Format_ARGB32: return QStringLiteral("ARGB32");
    case QImage::Format_ARGB32_Premultiplied: return QStringLiteral("ARGB32 Premultiplied");
    case QImage::Format_RGB16: return QStringLiteral("RGB16");
    case QImage::Format_ARGB8565_Premultiplied: return QStringLiteral("ARGB8565 Premultiplied");
    case QImage::Format_RGB666: return QStringLiteral("RGB666");
    case QImage::Format_ARGB6666_Premultiplied: return QStringLiteral("ARGB6666 Premultiplied");
    case QImage::Format_RGB555: return QStringLiteral("RGB555");
    case QImage::Format_ARGB8555_Premultiplied: return QStringLiteral("ARGB8555 Premultiplied");
    case QImage::Format_RGB888: return QStringLiteral("RGB888");
    case QImage::Format_RGB444: return QStringLiteral("RGB444");
    case QImage::Format_ARGB4444_Premultiplied: return QStringLiteral("ARGB4444 Premultiplied");
    case QImage::Format_RGBX8888: return QStringLiteral("RGBX8888");
    case QImage::Format_RGBA8888: return QStringLiteral("RGBA8888");
    case QImage::Format_RGBA8888_Premultiplied: return QStringLiteral("RGBA8888 Premultiplied");
    case QImage::Format_BGR30: return QStringLiteral("BGR30");
    case QImage::Format_A2BGR30_Premultiplied: return QStringLiteral("A2BGR30 Premultiplied");
    case QImage::Format_RGB30: return QStringLiteral("RGB30");
    case QImage::Format_A2RGB30_Premultiplied: return QStringLiteral("A2RGB30 Premultiplied");
    case QImage::Format_Alpha8: return QStringLiteral("Alpha8");
    case QImage::Format_Grayscale8: return QStringLiteral("Grayscale8");
    case QImage::Format_RGBX64: return QStringLiteral("RGBX64");
    case QImage::Format_RGBA64: return QStringLiteral("RGBA64");
    case QImage::Format_RGBA64_Premultiplied: return QStringLiteral("RGBA64 Premultiplied");
    case QImage::Format_Grayscale16: return QStringLiteral("Grayscale16");
    case QImage::Format_BGR888: return QStringLiteral("BGR888");
    case QImage::Format_RGBX16FPx4: return QStringLiteral("RGBX16FPx4");
    case QImage::Format_RGBA16FPx4: return QStringLiteral("RGBA16FPx4");
    case QImage::Format_RGBA16FPx4_Premultiplied: return QStringLiteral("RGBA16FPx4 Premultiplied");
    case QImage::Format_RGBX32FPx4: return QStringLiteral("RGBX32FPx4");
    case QImage::Format_RGBA32FPx4: return QStringLiteral("RGBA32FPx4");
    case QImage::Format_RGBA32FPx4_Premultiplied: return QStringLiteral("RGBA32FPx4 Premultiplied");
    case QImage::Format_CMYK8888: return QStringLiteral("CMYK8888");
    default: return QStringLiteral("Format %1").arg(static_cast<int>(format));
    }
}

QString colorSpaceName(const QColorSpace &colorSpace)
{
    if (!colorSpace.isValid()) {
        return {};
    }

    const QString description = colorSpace.description();
    if (!description.isEmpty()) {
        return description;
    }

    switch (colorSpace.primaries()) {
    case QColorSpace::Primaries::SRgb: return QStringLiteral("sRGB");
    case QColorSpace::Primaries::AdobeRgb: return QStringLiteral("Adobe RGB");
    case QColorSpace::Primaries::DciP3D65: return QStringLiteral("Display P3");
    case QColorSpace::Primaries::ProPhotoRgb: return QStringLiteral("ProPhoto RGB");
    case QColorSpace::Primaries::Bt2020: return QStringLiteral("BT.2020");
    default: return QStringLiteral("Custom");
    }
}

QString dpiText(const QImage &image)
{
    const int dpmX = image.dotsPerMeterX();
    const int dpmY = image.dotsPerMeterY();
    if (dpmX <= 0 || dpmY <= 0) {
        return {};
    }

    const int dpiX = qRound(dpmX * 0.0254);
    const int dpiY = qRound(dpmY * 0.0254);
    if (dpiX <= 0 || dpiY <= 0) {
        return {};
    }
    return dpiX == dpiY
        ? QStringLiteral("%1 DPI").arg(dpiX)
        : QStringLiteral("%1 x %2 DPI").arg(dpiX).arg(dpiY);
}
}

QVariantList MetadataExtractor::extract(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists())
        return {};

    if (fi.isDir()) {
        return extractDirectory(path);
    }

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(path);
    const QString suffix = fi.suffix().toLower();
    const QString mimeName = mime.name();

    // Image
    if (mimeName.startsWith("image/")
        && mimeName != "image/svg+xml"
        && !metadataIsDjvuDocument(suffix, mimeName)) {
        return extractImage(path, mime);
    }

    // SVG (special: image but we extract differently)
    if (mimeName == "image/svg+xml" || suffix == "svg" || suffix == "svgz") {
        return extractSvg(path);
    }

    // Font
    if (suffix == "ttf" || suffix == "otf" || suffix == "woff" || suffix == "woff2"
        || (suffix != "fon"
            && (mimeName == "font/ttf" || mimeName == "font/otf"
                || mimeName == "application/font-woff" || mimeName == "font/woff2"))) {
        return extractFont(path);
    }

    // PDF
    if (mimeName == "application/pdf" || suffix == "pdf") {
        return extractPdf(path);
    }

    // Archive
    if (ArchiveSupport::isArchiveExtension(suffix)) {
        return extractArchive(path);
    }

#ifdef Q_OS_WIN
    // Windows executable
    if (suffix == "exe" || suffix == "dll" || suffix == "msi") {
        return extractExecutable(path);
    }

    // Windows shortcut
    if (suffix == "lnk") {
        return extractShortcut(path);
    }
#endif

    // Audio files
    if (suffix == "mp3" || suffix == "flac" || suffix == "ogg" || suffix == "m4a" || suffix == "mp4" || suffix == "m4b" || suffix == "wav" || suffix == "wma"
        || mimeName.startsWith("audio/")) {
        return extractAudio(path);
    }

    // Text / code files
    if (mimeName.startsWith("text/") || mime.inherits("text/plain")
        || mime.inherits("application/json") || mime.inherits("application/javascript")
        || mime.inherits("application/xml")) {
        return extractText(path);
    }

    // Fallback: no extra metadata
    return {};
}

// ─── Image ───────────────────────────────────────────────────────────────────

QVariantList MetadataExtractor::extractImage(const QString &path, const QMimeType &mime)
{
    QVariantList props;

    QImageReader reader(path);
    reader.setAutoTransform(false); // read raw info
    if (!reader.canRead())
        return props;

    QSize size = reader.size();
    if (size.isValid()) {
        add(props, "Dimensions", QString("%1 × %2").arg(size.width()).arg(size.height()));

        // Megapixels for large images
        double mp = (size.width() * (double)size.height()) / 1'000'000.0;
        if (mp >= 0.1) {
            add(props, "Megapixels", QString::number(mp, 'f', 1) + " MP");
        }
    }

    add(props, "Format", QString::fromLatin1(reader.format()).toUpper());

    QImage::Format fmt = reader.imageFormat();
    QImage image;
    int depth = 0;
    bool hasAlpha = false;
    bool hasPixelInfo = false;
    if (fmt != QImage::Format_Invalid) {
        const QPixelFormat pixelFormat = QImage::toPixelFormat(fmt);
        depth = pixelFormat.bitsPerPixel();
        hasAlpha = pixelFormat.alphaUsage() == QPixelFormat::UsesAlpha;
        hasPixelInfo = depth > 0;
    } else {
        QImageReader probeReader(path);
        probeReader.setAutoTransform(false);
        image = probeReader.read();
        if (!image.isNull()) {
            fmt = image.format();
            depth = image.depth();
            hasAlpha = image.hasAlphaChannel();
            hasPixelInfo = depth > 0;
        }
    }

    if (image.isNull()) {
        QImageReader detailReader(path);
        detailReader.setAutoTransform(false);
        image = detailReader.read();
    }

    add(props, "Pixel Format", metadataImageFormatName(fmt));
    if (hasPixelInfo) {
        add(props, "Color Depth", QString::number(depth) + " bit");
        add(props, "Alpha Channel", hasAlpha ? "Yes" : "No");
    }

    if (!image.isNull()) {
        add(props, "DPI", dpiText(image));
        add(props, "Color Space", colorSpaceName(image.colorSpace()));
    }

    // Image count (animated GIF, APNG)
    int imageCount = reader.imageCount();
    if (imageCount > 1) {
        add(props, "Animated", "Yes");
        add(props, "Frames", QString::number(imageCount));
        const int loopCount = reader.loopCount();
        if (loopCount >= 0) {
            add(props, "Loop Count", loopCount == 0 ? QStringLiteral("Forever") : QString::number(loopCount));
        }
        const int delay = reader.nextImageDelay();
        if (delay > 0) {
            add(props, "Frame Delay", QStringLiteral("%1 ms").arg(delay));
        }
    }

    return props;
}

// ─── SVG ─────────────────────────────────────────────────────────────────────

QVariantList MetadataExtractor::extractSvg(const QString &path)
{
    QVariantList props;
    add(props, "Format", "SVG");

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return props;

    // Read first 8KB to parse width/height/viewBox quickly
    QByteArray head = file.read(8192);
    QXmlStreamReader xml(head);

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("svg")) {
            auto attrs = xml.attributes();

            // viewBox
            if (attrs.hasAttribute("viewBox")) {
                QString vb = attrs.value("viewBox").toString();
                add(props, "viewBox", vb);

                // Parse viewBox "minX minY width height"
                QStringList parts = vb.split(QRegularExpression("[\\s,]+"));
                if (parts.size() == 4) {
                    add(props, "Dimensions", parts[2] + " × " + parts[3]);
                }
            }

            // Explicit width/height
            if (attrs.hasAttribute("width") && attrs.hasAttribute("height")) {
                QString w = attrs.value("width").toString();
                QString h = attrs.value("height").toString();
                if (!w.isEmpty() && !h.isEmpty()) {
                    add(props, "Dimensions", w + " × " + h);
                }
            }

            break; // Only need <svg> element
        }
    }

    // Line count
    file.seek(0);
    if (file.size() < 1024 * 1024) {
        int lineCount = 0;
        while (!file.atEnd()) {
            file.readLine();
            lineCount++;
        }
        add(props, "Lines", QString::number(lineCount));
    }

    return props;
}

// ─── Text ────────────────────────────────────────────────────────────────────

QVariantList MetadataExtractor::extractText(const QString &path)
{
    QVariantList props;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return props;

    // Encoding detection from BOM
    QByteArray head = file.read(4);
    QString encoding = "UTF-8";
    if (head.startsWith("\xEF\xBB\xBF")) {
        encoding = "UTF-8 (BOM)";
    } else if (head.startsWith("\xFF\xFE")) {
        encoding = "UTF-16 LE";
    } else if (head.startsWith("\xFE\xFF")) {
        encoding = "UTF-16 BE";
    } else {
        // Heuristic: check for non-UTF-8 bytes
        file.seek(0);
        QByteArray sample = file.read(qMin(file.size(), qint64(32768)));
        bool isAscii = true;
        for (char c : sample) {
            if (static_cast<unsigned char>(c) > 127) {
                isAscii = false;
                break;
            }
        }
        encoding = isAscii ? "ASCII" : "UTF-8";
    }
    add(props, "Encoding", encoding);

    // Line count (limit to 10 MB)
    if (file.size() < 10 * 1024 * 1024) {
        file.seek(0);
        int lineCount = 0;
        while (!file.atEnd()) {
            file.readLine();
            lineCount++;
        }
        add(props, "Lines", QString::number(lineCount));
    }

    return props;
}

// ─── Font ────────────────────────────────────────────────────────────────────

QVariantList MetadataExtractor::extractFont(const QString &path)
{
    QVariantList props;

    QRawFont rawFont(path, 16.0);
    if (!rawFont.isValid())
        return props;

    add(props, "Family", rawFont.familyName());
    add(props, "Style", rawFont.styleName());

    // Weight
    int weight = rawFont.weight();
    QString weightName;
    if (weight <= 100) weightName = "Thin";
    else if (weight <= 200) weightName = "Extra Light";
    else if (weight <= 300) weightName = "Light";
    else if (weight <= 400) weightName = "Regular";
    else if (weight <= 500) weightName = "Medium";
    else if (weight <= 600) weightName = "Semi Bold";
    else if (weight <= 700) weightName = "Bold";
    else if (weight <= 800) weightName = "Extra Bold";
    else weightName = "Black";
    add(props, "Weight", weightName + " (" + QString::number(weight) + ")");

    // Glyph count
    // QRawFont doesn't expose glyph count directly, but we can check
    // supported characters by trying a reasonable sample
    add(props, "Units per Em", QString::number(rawFont.unitsPerEm()));

    // Ascent / Descent
    add(props, "Ascent", QString::number(rawFont.ascent(), 'f', 1));
    add(props, "Descent", QString::number(rawFont.descent(), 'f', 1));

    return props;
}

// ─── PDF ─────────────────────────────────────────────────────────────────────

QVariantList MetadataExtractor::extractPdf(const QString &path)
{
    QVariantList props;
    add(props, "Format", "PDF");

#ifdef HAS_QT_PDF
    QPdfDocument pdf;
    if (pdf.load(path) == QPdfDocument::Error::None) {
        add(props, "Pages", QString::number(pdf.pageCount()));
        
        QString title = pdf.metaData(QPdfDocument::MetaDataField::Title).toString().trimmed();
        add(props, "Title", title);

        QString author = pdf.metaData(QPdfDocument::MetaDataField::Author).toString().trimmed();
        add(props, "Author", author);

        QString subject = pdf.metaData(QPdfDocument::MetaDataField::Subject).toString().trimmed();
        add(props, "Subject", subject);

        QString keywords = pdf.metaData(QPdfDocument::MetaDataField::Keywords).toString().trimmed();
        add(props, "Keywords", keywords);

        QString creator = pdf.metaData(QPdfDocument::MetaDataField::Creator).toString().trimmed();
        add(props, "Creator", creator);

        QString producer = pdf.metaData(QPdfDocument::MetaDataField::Producer).toString().trimmed();
        add(props, "Producer", producer);

        return props;
    }
#endif

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return props;

    // Read header for version
    QByteArray header = file.read(32);
    if (header.startsWith("%PDF-")) {
        int dashPos = 5;
        int endPos = header.indexOf('\n', dashPos);
        if (endPos < 0) endPos = header.indexOf('\r', dashPos);
        if (endPos < 0) endPos = qMin(header.size(), 12);
        QString version = QString::fromLatin1(header.mid(dashPos, endPos - dashPos)).trimmed();
        add(props, "PDF Version", version);
    }

    // Scan last 4KB for page count and metadata
    qint64 tailStart = qMax(qint64(0), file.size() - 4096);
    file.seek(tailStart);
    QByteArray tail = file.readAll();

    // Try to find /Type /Pages ... /Count N
    // This is a simplified heuristic — works for most PDFs
    {
        int countIdx = tail.lastIndexOf("/Count ");
        if (countIdx >= 0) {
            int start = countIdx + 7;
            int end = start;
            while (end < tail.size() && (tail[end] >= '0' && tail[end] <= '9'))
                end++;
            if (end > start) {
                QString pages = QString::fromLatin1(tail.mid(start, end - start));
                add(props, "Pages", pages);
            }
        }
    }

    // Search first 8KB for /Title and /Author in info dict
    file.seek(0);
    QByteArray headChunk = file.read(8192);
    auto extractPdfString = [&](const QByteArray &data, const char *key) -> QString {
        int idx = data.indexOf(key);
        if (idx < 0) return {};
        idx += static_cast<int>(strlen(key));
        // Skip to opening paren
        while (idx < data.size() && data[idx] != '(') idx++;
        if (idx >= data.size()) return {};
        idx++; // skip '('
        int depth = 1;
        int start = idx;
        while (idx < data.size() && depth > 0) {
            if (data[idx] == '(' && (idx == 0 || data[idx-1] != '\\')) depth++;
            else if (data[idx] == ')' && (idx == 0 || data[idx-1] != '\\')) depth--;
            idx++;
        }
        return QString::fromLatin1(data.mid(start, idx - start - 1));
    };

    QString title = extractPdfString(headChunk, "/Title ");
    add(props, "Title", title);

    QString author = extractPdfString(headChunk, "/Author ");
    add(props, "Author", author);

    return props;
}

// ─── Archive ─────────────────────────────────────────────────────────────────

QVariantList MetadataExtractor::extractArchive(const QString &path)
{
    QVariantList props;
#ifdef HAS_UNOFFICIAL_BIT7Z
    const QFileInfo info(path);
    const QString archivePath = QDir::toNativeSeparators(info.absoluteFilePath());

    const auto readArchive = [&](const bit7z::Bit7zLibrary &library) {
        bit7z::BitArchiveReader reader(
            library,
            archivePath.toStdString(),
            bit7z::ArchiveStartOffset::FileStart,
            bit7z::BitFormat::Auto,
            ArchiveFileProvider::archivePasswordForPath(path).toStdString());

        const uint32_t itemCount = reader.itemsCount();
        uint32_t fileCount = 0;
        uint32_t folderCount = 0;
        quint64 unpackedSize = 0;
        quint64 packedSize = 0;
        bool encrypted = false;

        for (uint32_t i = 0; i < itemCount; ++i) {
            const auto item = reader.itemAt(i);
            if (item.isDir()) {
                ++folderCount;
                continue;
            }
            ++fileCount;
            unpackedSize += item.size();
            try {
                packedSize += item.packSize();
            } catch (const std::exception &) {
            }
            try {
                encrypted = encrypted || item.isEncrypted();
            } catch (const std::exception &) {
            }
        }

        const auto formattedSize = [&](quint64 bytes) {
            const quint64 capped = qMin<quint64>(bytes, std::numeric_limits<qint64>::max());
            return DriveUtils::formatSize(static_cast<qint64>(capped));
        };
        const QString suffix = info.suffix().toUpper();
        add(props, "Format", suffix.isEmpty() ? QStringLiteral("Archive") : suffix);
        add(props, "Entries", QString::number(itemCount));
        add(props, "Files", QString::number(fileCount));
        if (folderCount > 0) {
            add(props, "Folders", QString::number(folderCount));
        }
        if (unpackedSize > 0) {
            add(props, "Uncompressed", formattedSize(unpackedSize));
        }
        if (packedSize > 0) {
            add(props, "Packed", formattedSize(packedSize));
        }
        add(props, "Compressed", DriveUtils::formatSize(info.size()));
        if (unpackedSize > 0 && info.size() > 0) {
            const double ratio = 100.0 * static_cast<double>(info.size()) / static_cast<double>(unpackedSize);
            add(props, "Archive Ratio", QStringLiteral("%1%").arg(ratio, 0, 'f', 1));
        }
        if (encrypted) {
            add(props, "Encrypted", QStringLiteral("Yes"));
        }
    };

    try {
        bit7z::Bit7zLibrary library;
        readArchive(library);
    } catch (const std::exception &) {
        const QString libraryPath = ArchiveSupport::archiveLibraryPath();
        if (libraryPath.isEmpty()) {
            return {};
        }
        try {
            bit7z::Bit7zLibrary library(libraryPath.toStdString());
            readArchive(library);
        } catch (const std::exception &) {
            return {};
        }
    }
#else
    Q_UNUSED(path)
#endif
    return props;
}

// ─── Audio ───────────────────────────────────────────────────────────────────

#ifdef HAS_TAGLIB
QVariantList MetadataExtractor::extractAudio(const QString &path)
{
    QVariantList props;
#ifdef Q_OS_WIN
    const wchar_t *wpath = reinterpret_cast<const wchar_t *>(path.utf16());
    TagLib::FileRef f(wpath);
#else
    TagLib::FileRef f(path.toUtf8().constData());
#endif

    if (!f.isNull() && f.tag()) {
        TagLib::Tag *tag = f.tag();
        
        QString title = QString::fromStdWString(tag->title().toWString());
        QString artist = QString::fromStdWString(tag->artist().toWString());
        QString album = QString::fromStdWString(tag->album().toWString());
        QString comment = QString::fromStdWString(tag->comment().toWString());
        QString genre = QString::fromStdWString(tag->genre().toWString());
        unsigned int year = tag->year();
        unsigned int track = tag->track();

        add(props, "Title", title);
        add(props, "Artist", artist);
        add(props, "Album", album);
        if (year > 0) {
            add(props, "Year", QString::number(year));
        }
        if (track > 0) {
            add(props, "Track", QString::number(track));
        }
        add(props, "Genre", genre);
        add(props, "Comment", comment);
    }

    if (!f.isNull() && f.audioProperties()) {
        TagLib::AudioProperties *properties = f.audioProperties();
        
        int durationSec = properties->lengthInSeconds();
        int bitrate = properties->bitrate();
        int sampleRate = properties->sampleRate();
        int channels = properties->channels();

        if (durationSec > 0) {
            int minutes = durationSec / 60;
            int seconds = durationSec % 60;
            add(props, "Duration", QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0')));
        }
        if (bitrate > 0) {
            add(props, "Bitrate", QString("%1 kbps").arg(bitrate));
        }
        if (sampleRate > 0) {
            add(props, "Sample Rate", QString("%1 kHz").arg(static_cast<double>(sampleRate) / 1000.0, 0, 'f', 1));
        }
        if (channels > 0) {
            QString chanText;
            if (channels == 1) chanText = "Mono";
            else if (channels == 2) chanText = "Stereo";
            else chanText = QString("%1 channels").arg(channels);
            add(props, "Channels", chanText);
        }
    }

    return props;
}
#else
QVariantList MetadataExtractor::extractAudio(const QString &path)
{
    Q_UNUSED(path);
    return {};
}
#endif

// ─── Windows: Executable version info ────────────────────────────────────────

#ifdef Q_OS_WIN

QVariantList MetadataExtractor::extractExecutable(const QString &path)
{
    QVariantList props;

    std::wstring wpath = path.toStdWString();

    DWORD dummy = 0;
    DWORD versionInfoSize = GetFileVersionInfoSizeW(wpath.c_str(), &dummy);
    if (versionInfoSize == 0)
        return props;

    QByteArray versionData(static_cast<int>(versionInfoSize), '\0');
    if (!GetFileVersionInfoW(wpath.c_str(), 0, versionInfoSize,
                             versionData.data())) {
        return props;
    }

    // Query language-codepage pairs
    struct LangCodePage {
        WORD language;
        WORD codePage;
    } *pTranslate = nullptr;
    UINT cbTranslate = 0;

    if (!VerQueryValueW(versionData.data(),
                        L"\\VarFileInfo\\Translation",
                        reinterpret_cast<LPVOID *>(&pTranslate),
                        &cbTranslate) || cbTranslate == 0) {
        return props;
    }

    // Use first language/codepage
    wchar_t subBlock[256];
    auto queryString = [&](const wchar_t *name) -> QString {
        wsprintfW(subBlock, L"\\StringFileInfo\\%04x%04x\\%s",
                  pTranslate[0].language, pTranslate[0].codePage, name);
        wchar_t *value = nullptr;
        UINT len = 0;
        if (VerQueryValueW(versionData.data(), subBlock,
                           reinterpret_cast<LPVOID *>(&value), &len) && len > 0) {
            return QString::fromWCharArray(value).trimmed();
        }
        return {};
    };

    add(props, "Product", queryString(L"ProductName"));
    add(props, "File Version", queryString(L"FileVersion"));
    add(props, "Product Version", queryString(L"ProductVersion"));
    add(props, "Company", queryString(L"CompanyName"));
    add(props, "Description", queryString(L"FileDescription"));
    add(props, "Copyright", queryString(L"LegalCopyright"));
    add(props, "Original Name", queryString(L"OriginalFilename"));

    return props;
}

// ─── Windows: Shortcut (.lnk) target info ────────────────────────────────────

QVariantList MetadataExtractor::extractShortcut(const QString &path)
{
    QVariantList props;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hr);

    IShellLinkW *psl = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IShellLinkW, reinterpret_cast<void **>(&psl));
    if (FAILED(hr)) {
        if (needUninit) CoUninitialize();
        return props;
    }

    IPersistFile *ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&ppf));
    if (SUCCEEDED(hr)) {
        hr = ppf->Load(path.toStdWString().c_str(), STGM_READ);
        if (SUCCEEDED(hr)) {
            wchar_t buf[MAX_PATH + 1];

            // Target path
            WIN32_FIND_DATAW wfd;
            hr = psl->GetPath(buf, MAX_PATH, &wfd, SLGP_RAWPATH);
            if (SUCCEEDED(hr) && buf[0] != L'\0') {
                add(props, "Target", QString::fromWCharArray(buf));
            }

            // Working directory
            hr = psl->GetWorkingDirectory(buf, MAX_PATH);
            if (SUCCEEDED(hr) && buf[0] != L'\0') {
                add(props, "Working Directory", QString::fromWCharArray(buf));
            }

            // Arguments
            hr = psl->GetArguments(buf, MAX_PATH);
            if (SUCCEEDED(hr) && buf[0] != L'\0') {
                add(props, "Arguments", QString::fromWCharArray(buf));
            }

            // Description
            hr = psl->GetDescription(buf, MAX_PATH);
            if (SUCCEEDED(hr) && buf[0] != L'\0') {
                add(props, "Comment", QString::fromWCharArray(buf));
            }
        }
        ppf->Release();
    }
    psl->Release();

    if (needUninit) CoUninitialize();
    return props;
}

#endif // Q_OS_WIN

QVariantList MetadataExtractor::extractDirectory(const QString &path)
{
    QVariantList props;
    QDir dir(path);
    QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::System | QDir::Hidden);
    int filesCount = 0;
    int foldersCount = 0;
    for (const QFileInfo &info : list) {
        if (info.isDir()) {
            foldersCount++;
        } else {
            filesCount++;
        }
    }
    add(props, QStringLiteral("Contains"), QStringLiteral("%1 items (%2 files, %3 folders)")
        .arg(list.size())
        .arg(filesCount)
        .arg(foldersCount));
    return props;
}

