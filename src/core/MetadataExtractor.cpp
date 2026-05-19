#include "MetadataExtractor.h"

#include <QFileInfo>
#include <QFile>
#include <QImageReader>
#include <QImage>
#include <QMimeDatabase>
#include <QRawFont>
#include <QXmlStreamReader>
#include <QLocale>
#include <QDataStream>
#include <QTextStream>

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

QVariantList MetadataExtractor::extract(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists() || fi.isDir())
        return {};

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(path);
    const QString suffix = fi.suffix().toLower();
    const QString mimeName = mime.name();

    // Image
    if (mimeName.startsWith("image/") && mimeName != "image/svg+xml") {
        return extractImage(path, mime);
    }

    // SVG (special: image but we extract differently)
    if (mimeName == "image/svg+xml" || suffix == "svg" || suffix == "svgz") {
        return extractSvg(path);
    }

    // Font
    if (suffix == "ttf" || suffix == "otf" || suffix == "woff" || suffix == "woff2"
        || mimeName == "font/ttf" || mimeName == "font/otf"
        || mimeName == "application/font-woff" || mimeName == "font/woff2") {
        return extractFont(path);
    }

    // PDF
    if (mimeName == "application/pdf" || suffix == "pdf") {
        return extractPdf(path);
    }

    // ZIP archive
    if (mimeName == "application/zip" || suffix == "zip") {
        return extractArchiveZip(path);
    }

#ifdef Q_OS_WIN
    // Windows executable
    if (suffix == "exe" || suffix == "dll") {
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

    // Color depth from format
    QImage::Format fmt = reader.imageFormat();
    if (fmt != QImage::Format_Invalid) {
        int depth = QImage::toPixelFormat(fmt).bitsPerPixel();
        if (depth > 0) {
            add(props, "Color Depth", QString::number(depth) + " bit");
        }

        // Alpha channel
        bool hasAlpha = QImage::toPixelFormat(fmt).alphaUsage() == QPixelFormat::UsesAlpha;
        add(props, "Alpha Channel", hasAlpha ? "Yes" : "No");
    }

    // Image count (animated GIF, APNG)
    int imageCount = reader.imageCount();
    if (imageCount > 1) {
        add(props, "Frames", QString::number(imageCount));
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
                    add(props, "Size", w + " × " + h);
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

// ─── ZIP Archive ─────────────────────────────────────────────────────────────

QVariantList MetadataExtractor::extractArchiveZip(const QString &path)
{
    QVariantList props;
    add(props, "Format", "ZIP");

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return props;

    // Find End of Central Directory (EOCD) record in last 65KB
    qint64 searchStart = qMax(qint64(0), file.size() - 65536);
    file.seek(searchStart);
    QByteArray tail = file.readAll();

    // EOCD signature: 0x06054b50
    static const char eocdSig[] = { 0x50, 0x4b, 0x05, 0x06 };
    int eocdPos = -1;
    for (int i = tail.size() - 22; i >= 0; --i) {
        if (tail[i] == eocdSig[0] && tail[i+1] == eocdSig[1]
            && tail[i+2] == eocdSig[2] && tail[i+3] == eocdSig[3]) {
            eocdPos = i;
            break;
        }
    }

    if (eocdPos >= 0 && eocdPos + 22 <= tail.size()) {
        // Total entries (offset 10 in EOCD, 2 bytes LE)
        quint16 totalEntries = static_cast<quint8>(tail[eocdPos + 10])
                             | (static_cast<quint8>(tail[eocdPos + 11]) << 8);
        add(props, "Entries", QString::number(totalEntries));

        // Central directory size (offset 12, 4 bytes LE)
        quint32 cdSize = static_cast<quint8>(tail[eocdPos + 12])
                       | (static_cast<quint8>(tail[eocdPos + 13]) << 8)
                       | (static_cast<quint8>(tail[eocdPos + 14]) << 16)
                       | (static_cast<quint8>(tail[eocdPos + 15]) << 24);

        QLocale loc;
        add(props, "Compressed", loc.formattedDataSize(file.size()));
        Q_UNUSED(cdSize)
    }

    // ZIP comment
    if (eocdPos >= 0 && eocdPos + 22 <= tail.size()) {
        quint16 commentLen = static_cast<quint8>(tail[eocdPos + 20])
                           | (static_cast<quint8>(tail[eocdPos + 21]) << 8);
        if (commentLen > 0 && eocdPos + 22 + commentLen <= tail.size()) {
            QString comment = QString::fromUtf8(tail.mid(eocdPos + 22, commentLen));
            if (!comment.isEmpty()) {
                add(props, "Comment", comment);
            }
        }
    }

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
