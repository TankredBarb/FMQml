#include "CodeHighlighter.h"

#include <QFileInfo>
#include <QSet>
#include <QSettings>

namespace CodeHighlighter {
namespace {

enum class Language : unsigned char { None, Cpp, Python, Bash, Java, Kotlin };
enum Role { Keyword = 1, String = 2, Comment = 3, LiteralOrType = 4 };

void addSpan(QVector<TextDecorationSpan> &spans, int start, int end, int role)
{
    if (end > start) {
        spans.append({start, end - start, role});
    }
}

bool identifierStart(QChar ch)
{
    return ch == QLatin1Char('_') || ch.isLetter();
}

bool identifierPart(QChar ch)
{
    return ch == QLatin1Char('_') || ch.isLetterOrNumber();
}

bool isHexDigit(QChar ch)
{
    return ch.isDigit()
        || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'))
        || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'));
}

int scanNumberLiteral(const QString &text, int start)
{
    const bool hasBasePrefix = text.at(start) == QLatin1Char('0') && start + 1 < text.size();
    const QChar prefixChar = hasBasePrefix ? text.at(start + 1).toLower() : QChar();
    const bool isHex = prefixChar == QLatin1Char('x');
    const bool isBinary = prefixChar == QLatin1Char('b');
    int i = start + 1;
    while (i < text.size()) {
        const QChar ch = text.at(i);
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_')) {
            ++i;
            continue;
        }
        if (ch == QLatin1Char('.') && i + 1 < text.size()
            && (isHex ? isHexDigit(text.at(i + 1)) : text.at(i + 1).isDigit())) {
            ++i;
            continue;
        }
        if ((ch == QLatin1Char('+') || ch == QLatin1Char('-')) && i > start) {
            const QChar prev = text.at(i - 1).toLower();
            const bool exponentSign = (isHex && prev == QLatin1Char('p'))
                || (!isHex && !isBinary && prev == QLatin1Char('e'));
            if (exponentSign && i + 1 < text.size() && text.at(i + 1).isDigit()) {
                ++i;
                continue;
            }
        }
        break;
    }
    return i;
}

bool hasShebangCommand(QStringView firstLine, QLatin1StringView command)
{
    int i = 2;
    while (i < firstLine.size()) {
        while (i < firstLine.size() && firstLine.at(i).isSpace()) {
            ++i;
        }
        const int start = i;
        while (i < firstLine.size() && !firstLine.at(i).isSpace()) {
            ++i;
        }
        const QStringView token = firstLine.mid(start, i - start);
        if (token == command
            || (token.endsWith(command) && token.size() > command.size()
                && token.at(token.size() - command.size() - 1) == QLatin1Char('/'))) {
            return true;
        }
    }
    return false;
}

int scanSingleLineLiteral(const QString &text, int start)
{
    const QChar quote = text.at(start);
    int i = start + 1;
    bool escaped = false;
    while (i < text.size()) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('\n')) {
            break;
        }
        ++i;
        if (!escaped && ch == quote) {
            break;
        }
        escaped = !escaped && ch == QLatin1Char('\\');
    }
    return i;
}

bool scanJavaTextBlockBody(const QString &text, int &i)
{
    bool escaped = false;
    while (i < text.size()) {
        if (!escaped && QStringView(text).mid(i, 3) == QLatin1String("\"\"\"")) {
            i += 3;
            return true;
        }
        const QChar ch = text.at(i++);
        escaped = !escaped && ch == QLatin1Char('\\');
    }
    return false;
}

bool scanKotlinRawStringBody(const QString &text, int &i)
{
    const int end = text.indexOf(QLatin1String("\"\"\""), i);
    if (end < 0) {
        i = text.size();
        return false;
    }
    i = end + 3;
    while (i < text.size() && text.at(i) == QLatin1Char('"')) {
        ++i;
    }
    return true;
}

int scanKotlinNestedComment(const QString &text, int &i, int depth)
{
    while (i < text.size() && depth > 0) {
        const QStringView view = QStringView(text).mid(i, 2);
        if (view == QLatin1String("/*")) {
            ++depth;
            i += 2;
        } else if (view == QLatin1String("*/")) {
            --depth;
            i += 2;
        } else {
            ++i;
        }
    }
    return depth > 0 ? qMin(depth + 1, 255) : 0;
}

Language detectLanguage(const TextDecorationRequest &request)
{
    if (!request.initialState.isEmpty()) {
        const int value = static_cast<unsigned char>(request.initialState.at(0));
        if (value >= static_cast<int>(Language::Cpp)
            && value <= static_cast<int>(Language::Kotlin)) {
            return static_cast<Language>(value);
        }
    }
    const QString suffix = QFileInfo(request.fileName).suffix().toLower();
    static const QSet<QString> cppSuffixes = {
        QStringLiteral("c"), QStringLiteral("cc"), QStringLiteral("cpp"),
        QStringLiteral("cxx"), QStringLiteral("h"), QStringLiteral("hh"),
        QStringLiteral("hpp"), QStringLiteral("hxx")};
    if (cppSuffixes.contains(suffix)) {
        return Language::Cpp;
    }
    if (suffix == QLatin1String("py") || suffix == QLatin1String("pyw")) {
        return Language::Python;
    }
    if (suffix == QLatin1String("sh") || suffix == QLatin1String("bash")) {
        return Language::Bash;
    }
    if (suffix == QLatin1String("java")) {
        return Language::Java;
    }
    if (suffix == QLatin1String("kt") || suffix == QLatin1String("kts")) {
        return Language::Kotlin;
    }
    const QString mime = request.mimeName.toLower();
    if (mime == QLatin1String("text/x-java") || mime == QLatin1String("text/x-java-source")) {
        return Language::Java;
    }
    if (mime == QLatin1String("text/x-kotlin") || mime == QLatin1String("text/x-kotlin-source")) {
        return Language::Kotlin;
    }
    if (request.content.startsWith(QLatin1String("#!"))) {
        const QString firstLine = request.content.left(request.content.indexOf(QLatin1Char('\n'))).trimmed().toLower();
        if (firstLine.contains(QLatin1String("python"))) {
            return Language::Python;
        }
        if (firstLine.contains(QLatin1String("bash"))
            || firstLine.contains(QLatin1String("/sh"))) {
            return Language::Bash;
        }
        if (firstLine.contains(QLatin1String("kotlin"))) {
            return Language::Kotlin;
        }
        if (hasShebangCommand(firstLine, QLatin1StringView("java"))) {
            return Language::Java;
        }
    }
    return Language::None;
}

int initialLexerState(const TextDecorationRequest &request)
{
    return request.initialState.size() >= 2
        ? static_cast<unsigned char>(request.initialState.at(1)) : 0;
}

QByteArray stateBytes(Language language, int lexerState)
{
    QByteArray result;
    result.append(static_cast<char>(language));
    result.append(static_cast<char>(lexerState));
    return result;
}

int scanCpp(const QString &text, int state, QVector<TextDecorationSpan> &spans)
{
    static const QSet<QString> keywords = {
        QStringLiteral("alignas"), QStringLiteral("alignof"), QStringLiteral("asm"),
        QStringLiteral("auto"), QStringLiteral("break"), QStringLiteral("case"),
        QStringLiteral("catch"), QStringLiteral("class"), QStringLiteral("concept"),
        QStringLiteral("const"), QStringLiteral("constexpr"), QStringLiteral("continue"),
        QStringLiteral("default"), QStringLiteral("delete"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("enum"), QStringLiteral("explicit"),
        QStringLiteral("export"), QStringLiteral("extern"), QStringLiteral("for"),
        QStringLiteral("friend"), QStringLiteral("if"), QStringLiteral("inline"),
        QStringLiteral("namespace"), QStringLiteral("new"), QStringLiteral("noexcept"),
        QStringLiteral("operator"), QStringLiteral("private"), QStringLiteral("protected"),
        QStringLiteral("public"), QStringLiteral("requires"), QStringLiteral("return"),
        QStringLiteral("sizeof"), QStringLiteral("static"), QStringLiteral("struct"),
        QStringLiteral("switch"), QStringLiteral("template"), QStringLiteral("this"),
        QStringLiteral("throw"), QStringLiteral("try"), QStringLiteral("typedef"),
        QStringLiteral("typename"), QStringLiteral("union"), QStringLiteral("using"),
        QStringLiteral("virtual"), QStringLiteral("while")};
    static const QSet<QString> types = {
        QStringLiteral("bool"), QStringLiteral("char"), QStringLiteral("double"),
        QStringLiteral("float"), QStringLiteral("int"), QStringLiteral("long"),
        QStringLiteral("short"), QStringLiteral("signed"), QStringLiteral("unsigned"),
        QStringLiteral("void"), QStringLiteral("wchar_t"), QStringLiteral("nullptr"),
        QStringLiteral("true"), QStringLiteral("false")};
    int i = 0;
    while (i < text.size()) {
        if (state == 1) {
            const int start = i;
            const int end = text.indexOf(QLatin1String("*/"), i);
            i = end < 0 ? text.size() : end + 2;
            addSpan(spans, start, i, Comment);
            if (end < 0) return 1;
            state = 0;
            continue;
        }
        if (text.mid(i, 2) == QLatin1String("//")) {
            const int end = text.indexOf(QLatin1Char('\n'), i);
            const int stop = end < 0 ? text.size() : end;
            addSpan(spans, i, stop, Comment);
            i = stop;
            continue;
        }
        if (text.mid(i, 2) == QLatin1String("/*")) {
            state = 1;
            continue;
        }
        if (text.at(i) == QLatin1Char('"') || text.at(i) == QLatin1Char('\'')) {
            const QChar quote = text.at(i);
            const int start = i++;
            bool escaped = false;
            while (i < text.size()) {
                const QChar ch = text.at(i++);
                if (!escaped && ch == quote) break;
                escaped = !escaped && ch == QLatin1Char('\\');
                if (ch != QLatin1Char('\\')) escaped = false;
            }
            addSpan(spans, start, i, String);
            continue;
        }
        if (text.at(i).isDigit()) {
            const int start = i++;
            while (i < text.size() && (text.at(i).isLetterOrNumber()
                   || QStringLiteral("._'").contains(text.at(i)))) ++i;
            addSpan(spans, start, i, LiteralOrType);
            continue;
        }
        if (identifierStart(text.at(i))) {
            const int start = i++;
            while (i < text.size() && identifierPart(text.at(i))) ++i;
            const QString word = text.mid(start, i - start);
            if (keywords.contains(word)) addSpan(spans, start, i, Keyword);
            else if (types.contains(word)) addSpan(spans, start, i, LiteralOrType);
            continue;
        }
        ++i;
    }
    return state;
}

int scanPython(const QString &text, int state, QVector<TextDecorationSpan> &spans)
{
    static const QSet<QString> keywords = {
        QStringLiteral("and"), QStringLiteral("as"), QStringLiteral("assert"),
        QStringLiteral("async"), QStringLiteral("await"), QStringLiteral("break"),
        QStringLiteral("class"), QStringLiteral("continue"), QStringLiteral("def"),
        QStringLiteral("del"), QStringLiteral("elif"), QStringLiteral("else"),
        QStringLiteral("except"), QStringLiteral("finally"), QStringLiteral("for"),
        QStringLiteral("from"), QStringLiteral("global"), QStringLiteral("if"),
        QStringLiteral("import"), QStringLiteral("in"), QStringLiteral("is"),
        QStringLiteral("lambda"), QStringLiteral("nonlocal"), QStringLiteral("not"),
        QStringLiteral("or"), QStringLiteral("pass"), QStringLiteral("raise"),
        QStringLiteral("return"), QStringLiteral("try"), QStringLiteral("while"),
        QStringLiteral("with"), QStringLiteral("yield")};
    static const QSet<QString> literals = {
        QStringLiteral("True"), QStringLiteral("False"), QStringLiteral("None"),
        QStringLiteral("int"), QStringLiteral("str"), QStringLiteral("float"),
        QStringLiteral("bool"), QStringLiteral("bytes"), QStringLiteral("list"),
        QStringLiteral("dict"), QStringLiteral("set"), QStringLiteral("tuple")};
    int i = 0;
    while (i < text.size()) {
        if (state == 1 || state == 2) {
            const QString delimiter = state == 1 ? QStringLiteral("'''") : QStringLiteral("\"\"\"");
            const int start = i;
            const int end = text.indexOf(delimiter, i);
            i = end < 0 ? text.size() : end + 3;
            addSpan(spans, start, i, String);
            if (end < 0) return state;
            state = 0;
            continue;
        }
        if (text.at(i) == QLatin1Char('#')) {
            const int end = text.indexOf(QLatin1Char('\n'), i);
            const int stop = end < 0 ? text.size() : end;
            addSpan(spans, i, stop, Comment);
            i = stop;
            continue;
        }
        const QString triple = text.mid(i, 3);
        if (triple == QLatin1String("'''") || triple == QLatin1String("\"\"\"")) {
            const int start = i;
            const int end = text.indexOf(triple, i + 3);
            i = end < 0 ? text.size() : end + 3;
            addSpan(spans, start, i, String);
            if (end < 0) {
                return triple.at(0) == QLatin1Char('\'') ? 1 : 2;
            }
            continue;
        }
        if (text.at(i) == QLatin1Char('"') || text.at(i) == QLatin1Char('\'')) {
            const QChar quote = text.at(i);
            const int start = i++;
            bool escaped = false;
            while (i < text.size()) {
                const QChar ch = text.at(i++);
                if (!escaped && ch == quote) break;
                escaped = !escaped && ch == QLatin1Char('\\');
                if (ch != QLatin1Char('\\')) escaped = false;
            }
            addSpan(spans, start, i, String);
            continue;
        }
        if (text.at(i).isDigit()) {
            const int start = i++;
            while (i < text.size() && (text.at(i).isLetterOrNumber()
                   || text.at(i) == QLatin1Char('.') || text.at(i) == QLatin1Char('_'))) ++i;
            addSpan(spans, start, i, LiteralOrType);
            continue;
        }
        if (identifierStart(text.at(i))) {
            const int start = i++;
            while (i < text.size() && identifierPart(text.at(i))) ++i;
            const QString word = text.mid(start, i - start);
            if (keywords.contains(word)) addSpan(spans, start, i, Keyword);
            else if (literals.contains(word)) addSpan(spans, start, i, LiteralOrType);
            continue;
        }
        ++i;
    }
    return state;
}

int scanBash(const QString &text, int state, QVector<TextDecorationSpan> &spans)
{
    static const QSet<QString> keywords = {
        QStringLiteral("case"), QStringLiteral("do"), QStringLiteral("done"),
        QStringLiteral("elif"), QStringLiteral("else"), QStringLiteral("esac"),
        QStringLiteral("fi"), QStringLiteral("for"), QStringLiteral("function"),
        QStringLiteral("if"), QStringLiteral("in"), QStringLiteral("select"),
        QStringLiteral("then"), QStringLiteral("time"), QStringLiteral("until"),
        QStringLiteral("while")};
    int i = 0;
    while (i < text.size()) {
        if (state >= 1 && state <= 3) {
            const QChar quote = state == 1 ? QLatin1Char('\'')
                : state == 2 ? QLatin1Char('"') : QLatin1Char('`');
            const int start = i;
            bool escaped = false;
            while (i < text.size()) {
                const QChar ch = text.at(i++);
                if (!escaped && ch == quote) { state = 0; break; }
                escaped = quote != QLatin1Char('\'') && !escaped && ch == QLatin1Char('\\');
                if (ch != QLatin1Char('\\')) escaped = false;
            }
            addSpan(spans, start, i, String);
            if (state != 0) return state;
            continue;
        }
        if (text.at(i) == QLatin1Char('#')
            && (i == 0 || text.at(i - 1).isSpace())) {
            const int end = text.indexOf(QLatin1Char('\n'), i);
            const int stop = end < 0 ? text.size() : end;
            addSpan(spans, i, stop, Comment);
            i = stop;
            continue;
        }
        if (text.at(i) == QLatin1Char('\'') || text.at(i) == QLatin1Char('"')
            || text.at(i) == QLatin1Char('`')) {
            const QChar quote = text.at(i);
            const int start = i++;
            bool escaped = false;
            bool closed = false;
            while (i < text.size()) {
                const QChar ch = text.at(i++);
                if (!escaped && ch == quote) { closed = true; break; }
                escaped = quote != QLatin1Char('\'') && !escaped && ch == QLatin1Char('\\');
                if (ch != QLatin1Char('\\')) escaped = false;
            }
            addSpan(spans, start, i, String);
            if (!closed) {
                return quote == QLatin1Char('\'') ? 1
                    : quote == QLatin1Char('"') ? 2 : 3;
            }
            continue;
        }
        if (text.at(i) == QLatin1Char('$')) {
            const int start = i++;
            if (i < text.size() && text.at(i) == QLatin1Char('{')) {
                const int end = text.indexOf(QLatin1Char('}'), ++i);
                i = end < 0 ? text.size() : end + 1;
            } else {
                while (i < text.size() && identifierPart(text.at(i))) ++i;
            }
            addSpan(spans, start, i, LiteralOrType);
            continue;
        }
        if (identifierStart(text.at(i))) {
            const int start = i++;
            while (i < text.size() && identifierPart(text.at(i))) ++i;
            if (keywords.contains(text.mid(start, i - start))) addSpan(spans, start, i, Keyword);
            continue;
        }
        ++i;
    }
    return state;
}

int scanJava(const QString &text, int state, QVector<TextDecorationSpan> &spans)
{
    static const QSet<QString> keywords = {
        QStringLiteral("abstract"), QStringLiteral("assert"), QStringLiteral("break"),
        QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
        QStringLiteral("const"), QStringLiteral("continue"), QStringLiteral("default"),
        QStringLiteral("do"), QStringLiteral("else"), QStringLiteral("enum"),
        QStringLiteral("extends"), QStringLiteral("final"), QStringLiteral("finally"),
        QStringLiteral("for"), QStringLiteral("goto"), QStringLiteral("if"),
        QStringLiteral("implements"), QStringLiteral("import"), QStringLiteral("instanceof"),
        QStringLiteral("interface"), QStringLiteral("native"), QStringLiteral("new"),
        QStringLiteral("package"), QStringLiteral("permits"), QStringLiteral("private"),
        QStringLiteral("protected"), QStringLiteral("public"), QStringLiteral("record"),
        QStringLiteral("return"), QStringLiteral("sealed"), QStringLiteral("static"),
        QStringLiteral("strictfp"), QStringLiteral("super"), QStringLiteral("switch"),
        QStringLiteral("synchronized"), QStringLiteral("this"), QStringLiteral("throw"),
        QStringLiteral("throws"), QStringLiteral("transient"), QStringLiteral("try"),
        QStringLiteral("var"), QStringLiteral("volatile"), QStringLiteral("while"),
        QStringLiteral("yield")};
    static const QSet<QString> types = {
        QStringLiteral("boolean"), QStringLiteral("byte"), QStringLiteral("char"),
        QStringLiteral("double"), QStringLiteral("float"), QStringLiteral("int"),
        QStringLiteral("long"), QStringLiteral("short"), QStringLiteral("void"),
        QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"),
        QStringLiteral("Boolean"), QStringLiteral("Byte"), QStringLiteral("Character"),
        QStringLiteral("Double"), QStringLiteral("Float"), QStringLiteral("Integer"),
        QStringLiteral("Long"), QStringLiteral("Object"), QStringLiteral("Short"),
        QStringLiteral("String"), QStringLiteral("Void")};
    int i = 0;
    while (i < text.size()) {
        if (state == 1) {
            const int start = i;
            const int end = text.indexOf(QLatin1String("*/"), i);
            i = end < 0 ? text.size() : end + 2;
            addSpan(spans, start, i, Comment);
            if (end < 0) {
                return 1;
            }
            state = 0;
            continue;
        }
        if (state == 2) {
            const int start = i;
            const bool closed = scanJavaTextBlockBody(text, i);
            addSpan(spans, start, i, String);
            if (!closed) {
                return 2;
            }
            state = 0;
            continue;
        }
        if (QStringView(text).mid(i, 2) == QLatin1String("//")) {
            const int end = text.indexOf(QLatin1Char('\n'), i);
            const int stop = end < 0 ? text.size() : end;
            addSpan(spans, i, stop, Comment);
            i = stop;
            continue;
        }
        if (QStringView(text).mid(i, 2) == QLatin1String("/*")) {
            const int start = i;
            const int end = text.indexOf(QLatin1String("*/"), i + 2);
            i = end < 0 ? text.size() : end + 2;
            addSpan(spans, start, i, Comment);
            if (end < 0) {
                return 1;
            }
            continue;
        }
        if (QStringView(text).mid(i, 3) == QLatin1String("\"\"\"")) {
            const int start = i;
            i += 3;
            const bool closed = scanJavaTextBlockBody(text, i);
            addSpan(spans, start, i, String);
            if (!closed) {
                return 2;
            }
            continue;
        }
        if (text.at(i) == QLatin1Char('"') || text.at(i) == QLatin1Char('\'')) {
            const int start = i;
            i = scanSingleLineLiteral(text, start);
            addSpan(spans, start, i, String);
            continue;
        }
        if (text.at(i) == QLatin1Char('@')
            && (i == 0 || !identifierPart(text.at(i - 1)))
            && i + 1 < text.size() && identifierStart(text.at(i + 1))) {
            const int atPos = i++;
            const int wordStart = i++;
            while (i < text.size() && identifierPart(text.at(i))) {
                ++i;
            }
            if (QStringView(text).mid(wordStart, i - wordStart) == QLatin1String("interface")) {
                addSpan(spans, wordStart, i, Keyword);
            } else {
                addSpan(spans, atPos, i, LiteralOrType);
            }
            continue;
        }
        if (text.at(i).isDigit()) {
            const int start = i;
            i = scanNumberLiteral(text, start);
            addSpan(spans, start, i, LiteralOrType);
            continue;
        }
        if (identifierStart(text.at(i))) {
            const int start = i++;
            while (i < text.size() && identifierPart(text.at(i))) {
                ++i;
            }
            const QString word = text.mid(start, i - start);
            if (keywords.contains(word)) {
                addSpan(spans, start, i, Keyword);
            } else if (types.contains(word)) {
                addSpan(spans, start, i, LiteralOrType);
            }
            continue;
        }
        ++i;
    }
    return state;
}

int scanKotlin(const QString &text, int state, QVector<TextDecorationSpan> &spans)
{
    static const QSet<QString> keywords = {
        QStringLiteral("abstract"), QStringLiteral("actual"), QStringLiteral("annotation"),
        QStringLiteral("as"), QStringLiteral("break"), QStringLiteral("by"),
        QStringLiteral("catch"), QStringLiteral("class"), QStringLiteral("companion"),
        QStringLiteral("const"), QStringLiteral("constructor"), QStringLiteral("continue"),
        QStringLiteral("crossinline"), QStringLiteral("data"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("enum"), QStringLiteral("expect"),
        QStringLiteral("external"), QStringLiteral("final"), QStringLiteral("finally"),
        QStringLiteral("for"), QStringLiteral("fun"), QStringLiteral("if"),
        QStringLiteral("import"), QStringLiteral("in"), QStringLiteral("infix"),
        QStringLiteral("init"), QStringLiteral("inline"), QStringLiteral("inner"),
        QStringLiteral("interface"), QStringLiteral("internal"), QStringLiteral("is"),
        QStringLiteral("lateinit"), QStringLiteral("noinline"), QStringLiteral("object"),
        QStringLiteral("open"), QStringLiteral("operator"), QStringLiteral("out"),
        QStringLiteral("override"), QStringLiteral("package"), QStringLiteral("private"),
        QStringLiteral("protected"), QStringLiteral("public"), QStringLiteral("reified"),
        QStringLiteral("return"), QStringLiteral("sealed"), QStringLiteral("super"),
        QStringLiteral("suspend"), QStringLiteral("tailrec"), QStringLiteral("this"),
        QStringLiteral("throw"), QStringLiteral("try"), QStringLiteral("typealias"),
        QStringLiteral("typeof"), QStringLiteral("val"), QStringLiteral("var"),
        QStringLiteral("vararg"), QStringLiteral("when"), QStringLiteral("where"),
        QStringLiteral("while")};
    static const QSet<QString> types = {
        QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"),
        QStringLiteral("Any"), QStringLiteral("Array"), QStringLiteral("Boolean"),
        QStringLiteral("Byte"), QStringLiteral("Char"), QStringLiteral("CharSequence"),
        QStringLiteral("Double"), QStringLiteral("Float"), QStringLiteral("Int"),
        QStringLiteral("List"), QStringLiteral("Long"), QStringLiteral("Map"),
        QStringLiteral("MutableList"), QStringLiteral("MutableMap"), QStringLiteral("MutableSet"),
        QStringLiteral("Nothing"), QStringLiteral("Number"), QStringLiteral("Pair"),
        QStringLiteral("Sequence"), QStringLiteral("Set"), QStringLiteral("Short"),
        QStringLiteral("String"), QStringLiteral("Triple"), QStringLiteral("UByte"),
        QStringLiteral("UInt"), QStringLiteral("ULong"), QStringLiteral("UShort"),
        QStringLiteral("Unit")};
    int i = 0;
    while (i < text.size()) {
        if (state == 1) {
            const int start = i;
            const bool closed = scanKotlinRawStringBody(text, i);
            addSpan(spans, start, i, String);
            if (!closed) {
                return 1;
            }
            state = 0;
            continue;
        }
        if (state >= 2) {
            const int start = i;
            const int nextState = scanKotlinNestedComment(text, i, state - 1);
            addSpan(spans, start, i, Comment);
            if (nextState != 0) {
                return nextState;
            }
            state = 0;
            continue;
        }
        if (QStringView(text).mid(i, 2) == QLatin1String("//")) {
            const int end = text.indexOf(QLatin1Char('\n'), i);
            const int stop = end < 0 ? text.size() : end;
            addSpan(spans, i, stop, Comment);
            i = stop;
            continue;
        }
        if (QStringView(text).mid(i, 2) == QLatin1String("/*")) {
            const int start = i;
            i += 2;
            const int nextState = scanKotlinNestedComment(text, i, 1);
            addSpan(spans, start, i, Comment);
            if (nextState != 0) {
                return nextState;
            }
            continue;
        }
        if (QStringView(text).mid(i, 3) == QLatin1String("\"\"\"")) {
            const int start = i;
            i += 3;
            const bool closed = scanKotlinRawStringBody(text, i);
            addSpan(spans, start, i, String);
            if (!closed) {
                return 1;
            }
            continue;
        }
        if (text.at(i) == QLatin1Char('"')) {
            const int start = i++;
            bool escaped = false;
            int templateDepth = 0;
            while (i < text.size()) {
                const QChar ch = text.at(i);
                if (ch == QLatin1Char('\n')) {
                    break;
                }
                if (templateDepth > 0) {
                    if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
                        i = scanSingleLineLiteral(text, i);
                        continue;
                    }
                    ++i;
                    if (ch == QLatin1Char('{')) {
                        ++templateDepth;
                    } else if (ch == QLatin1Char('}')) {
                        --templateDepth;
                    }
                    continue;
                }
                ++i;
                if (!escaped && ch == QLatin1Char('$')
                    && i < text.size() && text.at(i) == QLatin1Char('{')) {
                    ++i;
                    templateDepth = 1;
                    continue;
                }
                if (!escaped && ch == QLatin1Char('"')) {
                    break;
                }
                escaped = !escaped && ch == QLatin1Char('\\');
            }
            addSpan(spans, start, i, String);
            continue;
        }
        if (text.at(i) == QLatin1Char('\'')) {
            const int start = i;
            i = scanSingleLineLiteral(text, start);
            addSpan(spans, start, i, String);
            continue;
        }
        if (text.at(i) == QLatin1Char('`')) {
            ++i;
            while (i < text.size() && text.at(i) != QLatin1Char('\n')) {
                if (text.at(i++) == QLatin1Char('`')) {
                    break;
                }
            }
            continue;
        }
        if (text.at(i) == QLatin1Char('@')
            && (i == 0 || !identifierPart(text.at(i - 1)))
            && i + 1 < text.size() && identifierStart(text.at(i + 1))) {
            const int start = i;
            i += 2;
            while (i < text.size() && identifierPart(text.at(i))) {
                ++i;
            }
            if (i + 1 < text.size() && text.at(i) == QLatin1Char(':')
                && identifierStart(text.at(i + 1))) {
                i += 2;
                while (i < text.size() && identifierPart(text.at(i))) {
                    ++i;
                }
            }
            addSpan(spans, start, i, LiteralOrType);
            continue;
        }
        if (text.at(i).isDigit()) {
            const int start = i;
            i = scanNumberLiteral(text, start);
            addSpan(spans, start, i, LiteralOrType);
            continue;
        }
        if (identifierStart(text.at(i))) {
            const int start = i++;
            while (i < text.size() && identifierPart(text.at(i))) {
                ++i;
            }
            const QString word = text.mid(start, i - start);
            if (keywords.contains(word)) {
                addSpan(spans, start, i, Keyword);
            } else if (types.contains(word)) {
                addSpan(spans, start, i, LiteralOrType);
            }
            continue;
        }
        ++i;
    }
    return state;
}

} // namespace

TextDecorationResult decorate(const TextDecorationRequest &request)
{
    const Language language = detectLanguage(request);
    if (language == Language::None) {
        return {};
    }
    TextDecorationResult result;
    result.supported = true;
    result.defaultWrap = false;
    result.defaultLineNumbers = true;

    const int state = initialLexerState(request);
    int finalState = 0;
    switch (language) {
    case Language::Cpp:
        result.languageId = QStringLiteral("cpp");
        result.languageLabel = QStringLiteral("C/C++");
        finalState = scanCpp(request.content, state, result.spans);
        break;
    case Language::Python:
        result.languageId = QStringLiteral("python");
        result.languageLabel = QStringLiteral("Python");
        finalState = scanPython(request.content, state, result.spans);
        break;
    case Language::Bash:
        result.languageId = QStringLiteral("bash");
        result.languageLabel = QStringLiteral("Bash");
        finalState = scanBash(request.content, state, result.spans);
        break;
    case Language::Java:
        result.languageId = QStringLiteral("java");
        result.languageLabel = QStringLiteral("Java");
        finalState = scanJava(request.content, state, result.spans);
        break;
    case Language::Kotlin:
        result.languageId = QStringLiteral("kotlin");
        result.languageLabel = QStringLiteral("Kotlin");
        finalState = scanKotlin(request.content, state, result.spans);
        break;
    case Language::None:
        return {};
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("CodeHighlighting"));
    result.fontFamily = settings.value(QStringLiteral("fontFamily"),
                                       QStringLiteral("DejaVu Sans Mono")).toString();
    result.role1Color = QColor(settings.value(QStringLiteral("keywordColor"),
                                               QStringLiteral("#7c3aed")).toString());
    result.role2Color = QColor(settings.value(QStringLiteral("stringColor"),
                                               QStringLiteral("#18794e")).toString());
    result.role3Color = QColor(settings.value(QStringLiteral("commentColor"),
                                               QStringLiteral("#667085")).toString());
    result.role4Color = QColor(settings.value(QStringLiteral("literalColor"),
                                               QStringLiteral("#b45309")).toString());
    const bool roleBold[] = {
        settings.value(QStringLiteral("keywordBold"), false).toBool(),
        settings.value(QStringLiteral("stringBold"), false).toBool(),
        settings.value(QStringLiteral("commentBold"), false).toBool(),
        settings.value(QStringLiteral("literalBold"), false).toBool()};
    const bool roleItalic[] = {
        settings.value(QStringLiteral("keywordItalic"), false).toBool(),
        settings.value(QStringLiteral("stringItalic"), false).toBool(),
        settings.value(QStringLiteral("commentItalic"), false).toBool(),
        settings.value(QStringLiteral("literalItalic"), false).toBool()};
    for (TextDecorationSpan &span : result.spans) {
        if (span.role >= 1 && span.role <= 4) {
            span.bold = roleBold[span.role - 1];
            span.italic = roleItalic[span.role - 1];
        }
    }
    result.finalState = stateBytes(language, finalState);
    return result;
}

} // namespace CodeHighlighter
