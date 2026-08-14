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

    TextDecorationRequest plainRequest;
    plainRequest.fileName = QStringLiteral("notes.txt");
    plainRequest.content = QStringLiteral("ordinary text");
    if (CodeHighlighter::decorate(plainRequest).supported) {
        return fail("code plugin claimed ordinary text");
    }

    return 0;
}
