#include "plugins/code_highlighting/CodeHighlighter.h"

#include <QCoreApplication>

#include <cstdio>

namespace {
int fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

bool hasRoleText(const TextDecorationResult &result, const QString &content,
                 int role, const QString &expected)
{
    for (const TextDecorationSpan &span : result.spans) {
        if (span.role == role && content.mid(span.start, span.length) == expected) {
            return true;
        }
    }
    return false;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    TextDecorationRequest cppRequest;
    cppRequest.fileName = QStringLiteral("sample.cpp");
    cppRequest.content = QStringLiteral("const int value = 42; // note\n/* open");
    const TextDecorationResult cpp = CodeHighlighter::decorate(cppRequest);
    if (!cpp.supported || cpp.languageId != QLatin1String("cpp")
        || !hasRoleText(cpp, cppRequest.content, 1, QStringLiteral("const"))
        || !hasRoleText(cpp, cppRequest.content, 4, QStringLiteral("int"))
        || cpp.finalState.size() < 2 || cpp.finalState.at(1) != 1) {
        return fail("C++ highlighting or continuation state is inconsistent");
    }

    TextDecorationRequest cppContinuation;
    cppContinuation.fileName = cppRequest.fileName;
    cppContinuation.content = QStringLiteral(" comment */ return value;\n");
    cppContinuation.initialState = cpp.finalState;
    const TextDecorationResult cppSecond = CodeHighlighter::decorate(cppContinuation);
    if (!hasRoleText(cppSecond, cppContinuation.content, 3,
                     QStringLiteral(" comment */"))
        || !hasRoleText(cppSecond, cppContinuation.content, 1,
                        QStringLiteral("return"))) {
        return fail("C++ multiline comment state was not carried to the next page");
    }

    TextDecorationRequest pythonRequest;
    pythonRequest.fileName = QStringLiteral("sample.py");
    pythonRequest.content = QStringLiteral("def run():\n    text = \"\"\"open\n");
    const TextDecorationResult python = CodeHighlighter::decorate(pythonRequest);
    if (!python.supported || python.languageId != QLatin1String("python")
        || !hasRoleText(python, pythonRequest.content, 1, QStringLiteral("def"))
        || python.finalState.size() < 2 || python.finalState.at(1) != 2) {
        return fail("Python highlighting or triple-string state is inconsistent");
    }

    TextDecorationRequest bashRequest;
    bashRequest.fileName = QStringLiteral("script.sh");
    bashRequest.content = QStringLiteral("if [ \"$value\" = 1 ]; then # note\n  echo ok\nfi\n");
    const TextDecorationResult bash = CodeHighlighter::decorate(bashRequest);
    if (!bash.supported || bash.languageId != QLatin1String("bash")
        || !hasRoleText(bash, bashRequest.content, 1, QStringLiteral("if"))
        || !hasRoleText(bash, bashRequest.content, 3, QStringLiteral("# note"))) {
        return fail("Bash highlighting is inconsistent");
    }

    TextDecorationRequest javaRequest;
    javaRequest.fileName = QStringLiteral("Sample.java");
    javaRequest.content = QStringLiteral(
        "@Override\n"
        "public @interface Marker {}\n"
        "public record Sample(int count, String name) {\n"
        "    /*/*/ boolean ok = true;\n"
        "    double pi = 3.14e-0;\n"
        "    double hexFloat = 0x1.fp-3;\n"
        "    int diff = 0xFE-1;\n"
        "    String block = \"\"\"open \\\"\\\"\\\"\n");
    const TextDecorationResult java = CodeHighlighter::decorate(javaRequest);
    if (!java.supported || java.languageId != QLatin1String("java")
        || java.languageLabel != QLatin1String("Java")
        || !hasRoleText(java, javaRequest.content, 4, QStringLiteral("@Override"))
        || !hasRoleText(java, javaRequest.content, 1, QStringLiteral("interface"))
        || hasRoleText(java, javaRequest.content, 4, QStringLiteral("@interface"))
        || !hasRoleText(java, javaRequest.content, 1, QStringLiteral("record"))
        || !hasRoleText(java, javaRequest.content, 4, QStringLiteral("String"))
        || !hasRoleText(java, javaRequest.content, 4, QStringLiteral("3.14e-0"))
        || !hasRoleText(java, javaRequest.content, 4, QStringLiteral("0x1.fp-3"))
        || !hasRoleText(java, javaRequest.content, 4, QStringLiteral("0xFE"))
        || !hasRoleText(java, javaRequest.content, 4, QStringLiteral("1"))
        || hasRoleText(java, javaRequest.content, 4, QStringLiteral("0xFE-1"))
        || !hasRoleText(java, javaRequest.content, 3, QStringLiteral("/*/*/"))
        || java.finalState.size() < 2 || java.finalState.at(1) != 2) {
        return fail("Java highlighting or text block state is inconsistent");
    }

    TextDecorationRequest javaContinuation;
    javaContinuation.fileName = javaRequest.fileName;
    javaContinuation.content = QStringLiteral("still block\"\"\"; /* open");
    javaContinuation.initialState = java.finalState;
    const TextDecorationResult javaSecond = CodeHighlighter::decorate(javaContinuation);
    if (!javaSecond.supported || javaSecond.languageId != QLatin1String("java")
        || !hasRoleText(javaSecond, javaContinuation.content, 2, QStringLiteral("still block\"\"\""))
        || !hasRoleText(javaSecond, javaContinuation.content, 3, QStringLiteral("/* open"))
        || javaSecond.finalState.size() < 2 || javaSecond.finalState.at(1) != 1) {
        return fail("Java text block or block comment continuation is inconsistent");
    }

    TextDecorationRequest javaCommentEnd;
    javaCommentEnd.fileName = javaRequest.fileName;
    javaCommentEnd.content = QStringLiteral(" done */ return null;");
    javaCommentEnd.initialState = javaSecond.finalState;
    const TextDecorationResult javaThird = CodeHighlighter::decorate(javaCommentEnd);
    if (!hasRoleText(javaThird, javaCommentEnd.content, 3, QStringLiteral(" done */"))
        || !hasRoleText(javaThird, javaCommentEnd.content, 1, QStringLiteral("return"))
        || !hasRoleText(javaThird, javaCommentEnd.content, 4, QStringLiteral("null"))) {
        return fail("Java multiline comment continuation failed");
    }

    TextDecorationRequest javaMimeRequest;
    javaMimeRequest.fileName = QStringLiteral("SourceWithoutSuffix");
    javaMimeRequest.mimeName = QStringLiteral("text/x-java");
    javaMimeRequest.content = QStringLiteral("class App {}");
    const TextDecorationResult javaMime = CodeHighlighter::decorate(javaMimeRequest);
    if (!javaMime.supported || javaMime.languageId != QLatin1String("java")
        || !hasRoleText(javaMime, javaMimeRequest.content, 1, QStringLiteral("class"))) {
        return fail("Java MIME fallback failed");
    }

    TextDecorationRequest kotlinRequest;
    kotlinRequest.fileName = QStringLiteral("Sample.kt");
    kotlinRequest.content = QStringLiteral(
        "@file:JvmName(\"Demo\")\n"
        "@Composable\n"
        "sealed class Result {\n"
        "    data class Ok(val items: List<Int> = listOf(0..10)) : Result()\n"
        "}\n"
        "suspend fun `when in`(): Unit {\n"
        "    val msg = \"value = ${map[\"key\"]}\"\n"
        "    /* outer /* inner */ still outer */\n"
        "    val raw = \"\"\"open\n");
    const TextDecorationResult kotlin = CodeHighlighter::decorate(kotlinRequest);
    if (!kotlin.supported || kotlin.languageId != QLatin1String("kotlin")
        || kotlin.languageLabel != QLatin1String("Kotlin")
        || !hasRoleText(kotlin, kotlinRequest.content, 4, QStringLiteral("@file:JvmName"))
        || !hasRoleText(kotlin, kotlinRequest.content, 4, QStringLiteral("@Composable"))
        || !hasRoleText(kotlin, kotlinRequest.content, 1, QStringLiteral("sealed"))
        || !hasRoleText(kotlin, kotlinRequest.content, 1, QStringLiteral("data"))
        || !hasRoleText(kotlin, kotlinRequest.content, 1, QStringLiteral("suspend"))
        || !hasRoleText(kotlin, kotlinRequest.content, 1, QStringLiteral("fun"))
        || !hasRoleText(kotlin, kotlinRequest.content, 4, QStringLiteral("Unit"))
        || !hasRoleText(kotlin, kotlinRequest.content, 4, QStringLiteral("0"))
        || !hasRoleText(kotlin, kotlinRequest.content, 4, QStringLiteral("10"))
        || hasRoleText(kotlin, kotlinRequest.content, 4, QStringLiteral("0..10"))
        || hasRoleText(kotlin, kotlinRequest.content, 1, QStringLiteral("when"))
        || hasRoleText(kotlin, kotlinRequest.content, 1, QStringLiteral("in"))
        || !hasRoleText(kotlin, kotlinRequest.content, 2, QStringLiteral("\"value = ${map[\"key\"]}\""))
        || !hasRoleText(kotlin, kotlinRequest.content, 3, QStringLiteral("/* outer /* inner */ still outer */"))
        || kotlin.finalState.size() < 2 || kotlin.finalState.at(1) != 1) {
        return fail("Kotlin highlighting is inconsistent");
    }

    TextDecorationRequest kotlinContinuation;
    kotlinContinuation.fileName = kotlinRequest.fileName;
    kotlinContinuation.content = QStringLiteral("raw end\"\"\"\" /* outer /* inner");
    kotlinContinuation.initialState = kotlin.finalState;
    const TextDecorationResult kotlinSecond = CodeHighlighter::decorate(kotlinContinuation);
    if (!kotlinSecond.supported || kotlinSecond.languageId != QLatin1String("kotlin")
        || !hasRoleText(kotlinSecond, kotlinContinuation.content, 2, QStringLiteral("raw end\"\"\"\""))
        || !hasRoleText(kotlinSecond, kotlinContinuation.content, 3, QStringLiteral("/* outer /* inner"))
        || kotlinSecond.finalState.size() < 2 || kotlinSecond.finalState.at(1) != 3) {
        return fail("Kotlin raw string or nested comment state is inconsistent");
    }

    TextDecorationRequest kotlinNestedEnd;
    kotlinNestedEnd.fileName = kotlinRequest.fileName;
    kotlinNestedEnd.content = QStringLiteral(" */ still outer */ val done = true");
    kotlinNestedEnd.initialState = kotlinSecond.finalState;
    const TextDecorationResult kotlinThird = CodeHighlighter::decorate(kotlinNestedEnd);
    if (!hasRoleText(kotlinThird, kotlinNestedEnd.content, 3, QStringLiteral(" */ still outer */"))
        || !hasRoleText(kotlinThird, kotlinNestedEnd.content, 1, QStringLiteral("val"))
        || !hasRoleText(kotlinThird, kotlinNestedEnd.content, 4, QStringLiteral("true"))
        || kotlinThird.finalState.size() < 2 || kotlinThird.finalState.at(1) != 0) {
        return fail("Kotlin nested block comment continuation failed");
    }

    TextDecorationRequest ktsRequest;
    ktsRequest.fileName = QStringLiteral("build.gradle.kts");
    ktsRequest.content = QStringLiteral("val name = \"app\"");
    const TextDecorationResult kts = CodeHighlighter::decorate(ktsRequest);
    if (!kts.supported || kts.languageId != QLatin1String("kotlin")) {
        return fail("Kotlin script (.kts) highlighting failed");
    }

    TextDecorationRequest kotlinMimeRequest;
    kotlinMimeRequest.fileName = QStringLiteral("ScriptWithoutSuffix");
    kotlinMimeRequest.mimeName = QStringLiteral("text/x-kotlin");
    kotlinMimeRequest.content = QStringLiteral("when (x) { else -> Unit }");
    const TextDecorationResult kotlinMime = CodeHighlighter::decorate(kotlinMimeRequest);
    if (!kotlinMime.supported || kotlinMime.languageId != QLatin1String("kotlin")
        || !hasRoleText(kotlinMime, kotlinMimeRequest.content, 1, QStringLiteral("when"))) {
        return fail("Kotlin MIME fallback failed");
    }

    TextDecorationRequest kotlinShebangRequest;
    kotlinShebangRequest.fileName = QStringLiteral("runscript");
    kotlinShebangRequest.content = QStringLiteral("#!/usr/bin/env kotlin\nfun main() = Unit\n");
    const TextDecorationResult kotlinShebang = CodeHighlighter::decorate(kotlinShebangRequest);
    if (!kotlinShebang.supported || kotlinShebang.languageId != QLatin1String("kotlin")
        || !hasRoleText(kotlinShebang, kotlinShebangRequest.content, 1, QStringLiteral("fun"))) {
        return fail("Kotlin shebang detection failed");
    }

    TextDecorationRequest javaShebangRequest;
    javaShebangRequest.fileName = QStringLiteral("runjava");
    javaShebangRequest.content = QStringLiteral("#!/usr/bin/env\tjava \r\nclass Main {}\r\n");
    const TextDecorationResult javaShebang = CodeHighlighter::decorate(javaShebangRequest);
    if (!javaShebang.supported || javaShebang.languageId != QLatin1String("java")
        || !hasRoleText(javaShebang, javaShebangRequest.content, 1, QStringLiteral("class"))) {
        return fail("Java CRLF shebang detection failed");
    }

    TextDecorationRequest jsShebangRequest;
    jsShebangRequest.fileName = QStringLiteral("runjs");
    jsShebangRequest.content = QStringLiteral("#!/usr/bin/env javascript\r\nclass Demo {}\r\n");
    if (CodeHighlighter::decorate(jsShebangRequest).supported) {
        return fail("JavaScript shebang was falsely detected as Java");
    }

    TextDecorationRequest plainRequest;
    plainRequest.fileName = QStringLiteral("notes.txt");
    plainRequest.content = QStringLiteral("ordinary text");
    if (CodeHighlighter::decorate(plainRequest).supported) {
        return fail("code plugin claimed ordinary text");
    }

    return 0;
}
