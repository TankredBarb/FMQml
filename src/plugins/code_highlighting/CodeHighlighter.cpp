#include "CodeHighlighter.h"

#include <QFileInfo>
#include <QSet>
#include <QSettings>

namespace CodeHighlighter {
namespace {

enum class Language : unsigned char { None, Cpp, Python, Bash };
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

Language detectLanguage(const TextDecorationRequest &request)
{
    if (!request.initialState.isEmpty()) {
        const int value = static_cast<unsigned char>(request.initialState.at(0));
        if (value >= static_cast<int>(Language::Cpp)
            && value <= static_cast<int>(Language::Bash)) {
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
    if (request.content.startsWith(QLatin1String("#!"))) {
        const QString firstLine = request.content.left(request.content.indexOf(QLatin1Char('\n'))).toLower();
        if (firstLine.contains(QLatin1String("python"))) {
            return Language::Python;
        }
        if (firstLine.contains(QLatin1String("bash"))
            || firstLine.contains(QLatin1String("/sh"))) {
            return Language::Bash;
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
    result.languageId = language == Language::Cpp ? QStringLiteral("cpp")
        : language == Language::Python ? QStringLiteral("python") : QStringLiteral("bash");
    result.languageLabel = language == Language::Cpp ? QStringLiteral("C/C++")
        : language == Language::Python ? QStringLiteral("Python") : QStringLiteral("Bash");

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

    const int state = initialLexerState(request);
    const int finalState = language == Language::Cpp
        ? scanCpp(request.content, state, result.spans)
        : language == Language::Python
            ? scanPython(request.content, state, result.spans)
            : scanBash(request.content, state, result.spans);
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
