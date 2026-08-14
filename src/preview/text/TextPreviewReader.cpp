#include "TextPreviewReader.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStringDecoder>

#include <limits>

namespace TextPreview {
namespace {

bool containsBinaryNul(const QByteArray &bytes)
{
    return bytes.contains('\0');
}

qsizetype lineAlignedLength(const QByteArray &bytes, qsizetype requestedLength)
{
    if (requestedLength >= bytes.size()) {
        return bytes.size();
    }
    const qsizetype newline = bytes.lastIndexOf('\n', requestedLength - 1);
    return newline >= 0 ? newline + 1 : requestedLength;
}

qsizetype utf8AlignedLength(const QByteArray &bytes, qsizetype length)
{
    if (length <= 0 || length >= bytes.size()) {
        return length;
    }
    qsizetype lead = length - 1;
    while (lead >= 0 && (static_cast<unsigned char>(bytes.at(lead)) & 0xc0) == 0x80) {
        --lead;
    }
    if (lead < 0) {
        return 0;
    }
    const unsigned char first = static_cast<unsigned char>(bytes.at(lead));
    const qsizetype expected = (first & 0x80) == 0x00 ? 1
        : (first & 0xe0) == 0xc0 ? 2
        : (first & 0xf0) == 0xe0 ? 3
        : (first & 0xf8) == 0xf0 ? 4 : 1;
    return lead + expected <= length ? length : lead;
}

qsizetype utf16LineAlignedLength(const QByteArray &bytes, qsizetype requestedLength,
                                 bool littleEndian)
{
    requestedLength -= requestedLength % 2;
    if (requestedLength >= bytes.size()) {
        return bytes.size() - bytes.size() % 2;
    }
    for (qsizetype pos = requestedLength - 2; pos >= 0; pos -= 2) {
        const unsigned char first = static_cast<unsigned char>(bytes.at(pos));
        const unsigned char second = static_cast<unsigned char>(bytes.at(pos + 1));
        const bool newline = littleEndian ? first == 0x0a && second == 0x00
                                          : first == 0x00 && second == 0x0a;
        if (newline) {
            return pos + 2;
        }
    }
    return requestedLength;
}

Snapshot decodeBytes(const QByteArray &sourceBytes, qint64 totalBytes,
                     const QString &fileName, const QString &mimeName,
                     qint64 byteOffset, qint64 firstLine, Encoding requestedEncoding,
                     const ReadOptions &options)
{
    Snapshot result;
    result.totalBytes = totalBytes;
    result.byteOffset = byteOffset;
    result.firstLine = firstLine;
    result.classification = classify(fileName, mimeName, sourceBytes.left(4096));

    const bool utf16Le = requestedEncoding == Encoding::Utf16Le
        || (requestedEncoding == Encoding::Auto && sourceBytes.startsWith("\xFF\xFE"));
    const bool utf16Be = requestedEncoding == Encoding::Utf16Be
        || (requestedEncoding == Encoding::Auto && sourceBytes.startsWith("\xFE\xFF"));
    if (!utf16Le && !utf16Be && containsBinaryNul(sourceBytes.left(4096))) {
        result.state = State::NotText;
        result.errorText = QStringLiteral("Binary data is not shown as text.");
        return result;
    }

    const qint64 safeFullLimit = qMax<qint64>(1, options.fullDocumentLimit);
    const qint64 safeDecodedBytes = qMax<qint64>(1, options.maximumDecodedBytes);
    const qint64 safeWindowBytes = qMin(qMax<qint64>(1, options.windowBytes), safeDecodedBytes);
    result.mode = byteOffset == 0 && totalBytes <= qMin(safeFullLimit, safeDecodedBytes)
        ? Mode::Complete : Mode::Windowed;
    const qint64 requestedBytes = result.mode == Mode::Complete
        ? qMin<qint64>(sourceBytes.size(), totalBytes)
        : qMin<qint64>(sourceBytes.size(), safeWindowBytes);
    qsizetype decodeLength = static_cast<qsizetype>(requestedBytes);
    if (result.mode == Mode::Windowed && !utf16Le && !utf16Be) {
        const qsizetype maximumUnbrokenBytes = static_cast<qsizetype>(
            qMax<qint64>(1, options.maximumUnbrokenBytes));
        const qsizetype firstNewline = sourceBytes.indexOf('\n');
        if (decodeLength > maximumUnbrokenBytes
            && (firstNewline < 0 || firstNewline >= maximumUnbrokenBytes)) {
            decodeLength = maximumUnbrokenBytes;
        }
        decodeLength = lineAlignedLength(sourceBytes, decodeLength);
        decodeLength = utf8AlignedLength(sourceBytes, decodeLength);
        if (decodeLength > 0 && decodeLength < sourceBytes.size()
            && sourceBytes.at(decodeLength - 1) == '\r') {
            --decodeLength;
        }
    } else if (result.mode == Mode::Windowed && (utf16Le || utf16Be)) {
        decodeLength = utf16LineAlignedLength(sourceBytes, decodeLength, utf16Le);
    } else if ((utf16Le || utf16Be) && decodeLength % 2 != 0) {
        --decodeLength;
    }
    QByteArray bytes = sourceBytes.left(decodeLength);

    QStringDecoder decoder(utf16Le ? QStringDecoder::Utf16LE
                                   : utf16Be ? QStringDecoder::Utf16BE
                                             : QStringDecoder::Utf8);
    result.content = decoder.decode(bytes);
    if (decoder.hasError()) {
        result.content.clear();
        result.state = State::DecodeError;
        result.errorText = QStringLiteral("Text encoding is not supported or the file is damaged.");
        return result;
    }

    result.encodingLabel = utf16Le ? QStringLiteral("UTF-16 LE")
                                   : utf16Be ? QStringLiteral("UTF-16 BE")
                                             : QStringLiteral("UTF-8");
    result.encoding = utf16Le ? Encoding::Utf16Le
                              : utf16Be ? Encoding::Utf16Be
                                        : Encoding::Utf8;
    result.state = State::Ready;
    if (!result.classification.text) {
        result.classification.text = true;
    }
    result.byteLength = bytes.size();
    result.visibleLineCount = result.content.isEmpty()
        ? 0
        : result.content.count(QLatin1Char('\n')) + (result.content.endsWith(QLatin1Char('\n')) ? 0 : 1);
    result.lineAdvance = result.content.count(QLatin1Char('\n'));
    result.hasPreviousPage = byteOffset > 0;
    result.hasNextPage = byteOffset + result.byteLength < totalBytes;
    return result;
}

} // namespace

Classification classify(const QString &fileName, const QString &mimeName, const QByteArray &prefix)
{
    Q_UNUSED(prefix)
    Classification result;
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    const QString lowerMime = mimeName.toLower();
    result.text = lowerMime.startsWith(QStringLiteral("text/"))
        || lowerMime == QStringLiteral("application/javascript")
        || lowerMime == QStringLiteral("application/json")
        || lowerMime.endsWith(QStringLiteral("+json"))
        || lowerMime == QStringLiteral("application/xml")
        || lowerMime.endsWith(QStringLiteral("+xml"))
        || suffix == QStringLiteral("txt")
        || suffix == QStringLiteral("md");
    result.defaultWrap = true;
    result.defaultLineNumbers = false;
    return result;
}

Snapshot readFile(const QString &path, const ReadOptions &options)
{
    return readFilePage(path, 0, 1, Encoding::Auto, options);
}

Snapshot readFilePage(const QString &path, qint64 byteOffset, qint64 firstLine,
                      Encoding encoding, const ReadOptions &options)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Snapshot result;
        result.state = State::ReadError;
        result.errorText = file.errorString();
        return result;
    }

    const qint64 totalBytes = file.size();
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchDefault);
    if (totalBytes == 0 && byteOffset == 0) {
        return decodeBytes({}, 0, QFileInfo(path).fileName(), mime.name(),
                           0, 1, Encoding::Auto, options);
    }
    if (byteOffset < 0 || byteOffset >= totalBytes) {
        Snapshot result;
        result.state = State::ReadError;
        result.errorText = QStringLiteral("Requested text page is outside the file.");
        return result;
    }
    if (encoding == Encoding::Auto && byteOffset > 0) {
        const QByteArray bom = file.read(2);
        encoding = bom.startsWith("\xFF\xFE") ? Encoding::Utf16Le
            : bom.startsWith("\xFE\xFF") ? Encoding::Utf16Be : Encoding::Utf8;
    }
    if (!file.seek(byteOffset)) {
        Snapshot result;
        result.state = State::ReadError;
        result.errorText = file.errorString();
        return result;
    }
    const qint64 remainingBytes = totalBytes - byteOffset;
    const qint64 maximumDecodedBytes = qMax<qint64>(1, options.maximumDecodedBytes);
    const qint64 readLimit = byteOffset == 0
        && totalBytes <= qMin(qMax<qint64>(1, options.fullDocumentLimit), maximumDecodedBytes)
        ? remainingBytes
        : qMin(qMax<qint64>(1, options.windowBytes), maximumDecodedBytes) + 4;
    const QByteArray bytes = file.read(readLimit);
    return decodeBytes(bytes, totalBytes, QFileInfo(path).fileName(), mime.name(),
                       byteOffset, firstLine, encoding, options);
}

Snapshot readBytes(const QByteArray &bytes, const QString &fileName,
                   const QString &mimeName, const ReadOptions &options)
{
    return decodeBytes(bytes, bytes.size(), fileName, mimeName, 0, 1, Encoding::Auto, options);
}

Snapshot readBytesPage(const QByteArray &bytes, qint64 totalBytes,
                       const QString &fileName, const QString &mimeName,
                       qint64 byteOffset, qint64 firstLine, Encoding encoding,
                       const ReadOptions &options)
{
    if (byteOffset < 0 || byteOffset > totalBytes) {
        Snapshot result;
        result.state = State::ReadError;
        result.errorText = QStringLiteral("Requested text page is outside the source.");
        return result;
    }
    return decodeBytes(bytes, totalBytes, fileName, mimeName,
                       byteOffset, firstLine, encoding, options);
}

int countBytesLines(const QByteArray &bytes)
{
    if (bytes.isEmpty()) {
        return 0;
    }
    const bool utf16Le = bytes.startsWith("\xFF\xFE");
    const bool utf16Be = bytes.startsWith("\xFE\xFF");
    qint64 newlineCount = 0;
    bool endsWithNewline = false;
    if (utf16Le || utf16Be) {
        for (qsizetype pos = 2; pos + 1 < bytes.size(); pos += 2) {
            const unsigned char first = static_cast<unsigned char>(bytes.at(pos));
            const unsigned char second = static_cast<unsigned char>(bytes.at(pos + 1));
            const bool newline = utf16Le ? first == 0x0a && second == 0x00
                                         : first == 0x00 && second == 0x0a;
            newlineCount += newline ? 1 : 0;
            endsWithNewline = newline;
        }
    } else {
        newlineCount = bytes.count('\n');
        endsWithNewline = bytes.endsWith('\n');
    }
    const qint64 lineCount = newlineCount + (endsWithNewline ? 0 : 1);
    return static_cast<int>(qMin<qint64>(lineCount, std::numeric_limits<int>::max()));
}

int countFileLines(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QByteArray bom = file.peek(2);
    const bool utf16Le = bom.startsWith("\xFF\xFE");
    const bool utf16Be = bom.startsWith("\xFE\xFF");
    qint64 newlineCount = 0;
    bool hasContent = false;
    bool endsWithNewline = false;
    QByteArray carry;
    while (!file.atEnd()) {
        QByteArray chunk = file.read(64 * 1024);
        if (chunk.isEmpty()) {
            break;
        }
        hasContent = true;
        if (!utf16Le && !utf16Be) {
            newlineCount += chunk.count('\n');
            endsWithNewline = chunk.endsWith('\n');
            continue;
        }
        if (!carry.isEmpty()) {
            chunk.prepend(carry);
            carry.clear();
        }
        if (chunk.size() % 2 != 0) {
            carry = chunk.right(1);
            chunk.chop(1);
        }
        for (qsizetype pos = 0; pos + 1 < chunk.size(); pos += 2) {
            const unsigned char first = static_cast<unsigned char>(chunk.at(pos));
            const unsigned char second = static_cast<unsigned char>(chunk.at(pos + 1));
            const bool newline = utf16Le ? first == 0x0a && second == 0x00
                                         : first == 0x00 && second == 0x0a;
            newlineCount += newline ? 1 : 0;
            endsWithNewline = newline;
        }
    }
    if (!hasContent) {
        return 0;
    }
    const qint64 lineCount = newlineCount + (endsWithNewline ? 0 : 1);
    return static_cast<int>(qMin<qint64>(lineCount, std::numeric_limits<int>::max()));
}

} // namespace TextPreview
