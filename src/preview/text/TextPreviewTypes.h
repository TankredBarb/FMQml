#pragma once

#include <QByteArray>
#include <QString>
#include <QVariantList>

namespace TextPreview {

enum class State {
    Empty,
    Loading,
    Ready,
    NotText,
    ReadError,
    DecodeError
};

enum class Mode {
    Complete,
    Windowed
};

enum class Kind {
    Plain,
    Code,
    Script,
    Structured,
    Log
};

enum class Encoding {
    Auto,
    Utf8,
    Utf16Le,
    Utf16Be
};

struct Classification {
    bool text = false;
    Kind kind = Kind::Plain;
    QString languageId;
    QString languageLabel;
    bool defaultWrap = true;
    bool defaultLineNumbers = false;
};

struct ReadOptions {
    qint64 fullDocumentLimit = 512 * 1024;
    qint64 windowBytes = 128 * 1024;
    qint64 maximumDecodedBytes = 32 * 1024;
    qint64 maximumUnbrokenBytes = 8 * 1024;
};

struct Snapshot {
    State state = State::Empty;
    Mode mode = Mode::Complete;
    Classification classification;
    QString content;
    QString encodingLabel;
    Encoding encoding = Encoding::Auto;
    QString errorText;
    qint64 totalBytes = 0;
    qint64 byteOffset = 0;
    qint64 byteLength = 0;
    qint64 firstLine = 1;
    int visibleLineCount = 0;
    int lineAdvance = 0;
    bool hasPreviousPage = false;
    bool hasNextPage = false;
    QString fontFamilyOverride;
    QString tokenColor1;
    QString tokenColor2;
    QString tokenColor3;
    QString tokenColor4;
    QVariantList styleRanges;
    QByteArray decorationInitialState;
    QByteArray decorationFinalState;
};

} // namespace TextPreview
