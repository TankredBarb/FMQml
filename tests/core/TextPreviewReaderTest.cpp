#include "preview/text/TextPreviewReader.h"

#include <QDebug>

namespace {
int fail(const QString &message)
{
    qCritical().noquote() << message;
    return 1;
}
}

int main()
{
    using namespace TextPreview;

    const QString fixturePath = QStringLiteral(FM_TEST_SOURCE_DIR "/tests/fixtures/text-preview/python-source.py");
    const Snapshot fixture = readFile(fixturePath);
    if (fixture.state != State::Ready || fixture.mode != Mode::Complete
        || !fixture.classification.languageId.isEmpty()
        || !fixture.content.contains(QStringLiteral("Привіт"))) {
        return fail(QStringLiteral("Python file fixture did not load through the file reader"));
    }

    ReadOptions fixtureWindowOptions;
    fixtureWindowOptions.fullDocumentLimit = 8;
    fixtureWindowOptions.windowBytes = 48;
    const Snapshot firstFixturePage = readFile(fixturePath, fixtureWindowOptions);
    const Snapshot secondFixturePage = readFilePage(
        fixturePath, firstFixturePage.byteLength,
        firstFixturePage.firstLine + firstFixturePage.visibleLineCount,
        firstFixturePage.encoding, fixtureWindowOptions);
    if (firstFixturePage.state != State::Ready || firstFixturePage.mode != Mode::Windowed
        || !firstFixturePage.hasNextPage || firstFixturePage.hasPreviousPage
        || secondFixturePage.state != State::Ready || !secondFixturePage.hasPreviousPage
        || secondFixturePage.byteOffset != firstFixturePage.byteLength
        || secondFixturePage.firstLine != firstFixturePage.visibleLineCount + 1) {
        return fail(QStringLiteral("sequential file pages do not preserve offsets and line base"));
    }

    const Classification python = classify(QStringLiteral("tool.py"), QStringLiteral("text/x-python"));
    if (!python.text || !python.languageId.isEmpty()
        || !python.defaultWrap || python.defaultLineNumbers) {
        return fail(QStringLiteral("core text classification contains language-specific behavior"));
    }

    const Classification prose = classify(QStringLiteral("notes.txt"), QStringLiteral("text/plain"));
    if (!prose.text || prose.kind != Kind::Plain || !prose.defaultWrap || prose.defaultLineNumbers) {
        return fail(QStringLiteral("plain-text defaults are inconsistent"));
    }

    const Classification shebang = classify(QStringLiteral("runner"), {}, QByteArray("#!/usr/bin/env python3\n"));
    if (shebang.text || !shebang.languageId.isEmpty()) {
        return fail(QStringLiteral("core classified a plugin-owned shebang"));
    }

    const QByteArray utf8 = QStringLiteral("alpha\nПривіт 🌍\nomega").toUtf8();
    const Snapshot utf8Snapshot = readBytes(utf8, QStringLiteral("sample.txt"), QStringLiteral("text/plain"));
    if (utf8Snapshot.state != State::Ready || utf8Snapshot.mode != Mode::Complete
        || utf8Snapshot.content != QString::fromUtf8(utf8)
        || utf8Snapshot.encodingLabel != QLatin1String("UTF-8")
        || utf8Snapshot.visibleLineCount != 3 || utf8Snapshot.hasNextPage) {
        return fail(QStringLiteral("UTF-8 complete snapshot is inconsistent"));
    }
    if (countBytesLines(utf8) != 3) {
        return fail(QStringLiteral("UTF-8 total line count is inconsistent"));
    }

    QByteArray utf16Le("\xFF\xFE", 2);
    const QString utf16Text = QStringLiteral("one\nдва");
    for (const QChar character : utf16Text) {
        const ushort value = character.unicode();
        utf16Le.append(static_cast<char>(value & 0xff));
        utf16Le.append(static_cast<char>((value >> 8) & 0xff));
    }
    const Snapshot utf16Snapshot = readBytes(utf16Le, QStringLiteral("utf16.txt"), QStringLiteral("text/plain"));
    if (utf16Snapshot.state != State::Ready || utf16Snapshot.content != utf16Text
        || utf16Snapshot.encodingLabel != QLatin1String("UTF-16 LE")) {
        return fail(QStringLiteral("UTF-16 LE decoding failed"));
    }
    if (countBytesLines(utf16Le) != 2) {
        return fail(QStringLiteral("UTF-16 total line count is inconsistent"));
    }

    const Snapshot binary = readBytes(QByteArray("abc\0def", 7), QStringLiteral("fake.txt"), QStringLiteral("text/plain"));
    if (binary.state != State::NotText || !binary.content.isEmpty()) {
        return fail(QStringLiteral("binary input was exposed as text"));
    }

    QByteArray invalidUtf8("valid ");
    invalidUtf8.append(char(0xc3));
    invalidUtf8.append(char(0x28));
    const Snapshot invalid = readBytes(invalidUtf8, QStringLiteral("broken.txt"), QStringLiteral("text/plain"));
    if (invalid.state != State::DecodeError || !invalid.content.isEmpty()) {
        return fail(QStringLiteral("invalid UTF-8 was not rejected"));
    }

    const Snapshot extensionless = readBytes(QByteArray("ordinary text\n"), QStringLiteral("README"));
    if (extensionless.state != State::Ready || !extensionless.classification.text
        || extensionless.classification.kind != Kind::Plain) {
        return fail(QStringLiteral("valid extensionless text was not recognized"));
    }

    const QByteArray longLine(256 * 1024, 'A');
    const Snapshot boundedLongLine = readBytes(
        longLine, QStringLiteral("long-line.txt"), QStringLiteral("text/plain"));
    if (boundedLongLine.state != State::Ready || boundedLongLine.mode != Mode::Windowed
        || boundedLongLine.byteLength > 8 * 1024 || boundedLongLine.content.size() > 8 * 1024
        || !boundedLongLine.hasNextPage || boundedLongLine.visibleLineCount != 1
        || boundedLongLine.lineAdvance != 0) {
        return fail(QStringLiteral("a long line exceeded the rendered snapshot budget"));
    }

    ReadOptions windowOptions;
    windowOptions.fullDocumentLimit = 8;
    windowOptions.windowBytes = 13;
    const QByteArray windowedBytes("first\nsecond\nthird\nfourth\n");
    const Snapshot windowed = readBytes(windowedBytes, QStringLiteral("large.log"), QStringLiteral("text/plain"), windowOptions);
    if (windowed.state != State::Ready || windowed.mode != Mode::Windowed
        || windowed.content != QLatin1String("first\nsecond\n")
        || windowed.byteLength != 13 || !windowed.hasNextPage
        || windowed.visibleLineCount != 2) {
        return fail(QStringLiteral("windowed snapshot did not stop on a line boundary"));
    }
    ReadOptions utf8BoundaryOptions;
    utf8BoundaryOptions.fullDocumentLimit = 2;
    utf8BoundaryOptions.windowBytes = 8;
    const QByteArray boundaryBytes = QStringLiteral("123456🌍tail").toUtf8();
    const Snapshot boundary = readBytes(boundaryBytes, QStringLiteral("boundary.txt"),
                                        QStringLiteral("text/plain"), utf8BoundaryOptions);
    if (boundary.state != State::Ready || boundary.content != QLatin1String("123456")
        || boundary.byteLength != 6 || !boundary.hasNextPage) {
        return fail(QStringLiteral("windowed snapshot split a UTF-8 sequence"));
    }

    const Snapshot ranged = readBytesPage(
        windowedBytes.mid(13), windowedBytes.size(), QStringLiteral("large.log"),
        QStringLiteral("text/plain"), 13, 3, Encoding::Utf8, windowOptions);
    if (ranged.state != State::Ready || ranged.byteOffset != 13
        || ranged.firstLine != 3 || !ranged.hasPreviousPage
        || ranged.content != QLatin1String("third\nfourth\n")) {
        return fail(QStringLiteral("bounded source page lost its global position"));
    }

    return 0;
}
