#include "../../src/core/DebugReportSanitizer.h"
#include "../../src/core/DebugReportFormatter.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {
int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QString assignments = DebugReportSanitizer::sanitize(
        QStringLiteral("token=abc123 Cookie: session-value password = hunter2"));
    if (assignments.contains(QStringLiteral("abc123"))
        || assignments.contains(QStringLiteral("session-value"))
        || assignments.contains(QStringLiteral("hunter2"))
        || assignments.count(QStringLiteral("[REDACTED]")) != 3) {
        return fail(QStringLiteral("sensitive assignments were not fully redacted"));
    }

    const QString query = DebugReportSanitizer::sanitize(
        QStringLiteral("https://example.test/file?id=42&access_token=private-value&view=1"));
    if (!query.contains(QStringLiteral("id=42")) || !query.contains(QStringLiteral("view=1"))
        || query.contains(QStringLiteral("private-value"))) {
        return fail(QStringLiteral("sensitive URL query value was not redacted safely"));
    }

    const QString bearer = DebugReportSanitizer::sanitize(
        QStringLiteral("Authorization: Bearer abc.def-123"));
    if (bearer.contains(QStringLiteral("abc.def-123"))) {
        return fail(QStringLiteral("bearer credential was not redacted"));
    }

    const QString credentialUrl = DebugReportSanitizer::sanitize(
        QStringLiteral("https://person:private-password@example.test/path#session-data"));
    if (credentialUrl.contains(QStringLiteral("person"))
        || credentialUrl.contains(QStringLiteral("private-password"))
        || credentialUrl.contains(QStringLiteral("session-data"))) {
        return fail(QStringLiteral("URL credentials or fragment were not redacted"));
    }

    const QString ordinary = QStringLiteral("Theme: Graphite | Operation: Idle | Qt: 6.9.1");
    if (DebugReportSanitizer::sanitize(ordinary) != ordinary) {
        return fail(QStringLiteral("ordinary diagnostic text was changed"));
    }

    const QString ordinaryPaths = QStringLiteral("C:\\Users\\person\\file.txt | /home/person/file.txt");
    if (DebugReportSanitizer::sanitize(ordinaryPaths) != ordinaryPaths) {
        return fail(QStringLiteral("ordinary Windows or Linux paths were changed"));
    }

    const QVariantMap snapshot{{QStringLiteral("sections"), QVariantList{
        QVariantMap{{QStringLiteral("title"), QStringLiteral("TEST")},
                    {QStringLiteral("rows"), QVariantList{
                        QVariantMap{{QStringLiteral("label"), QStringLiteral("State")},
                                    {QStringLiteral("value"), QStringLiteral("token=private")},
                                    {QStringLiteral("path"), false}},
                        QVariantMap{{QStringLiteral("label"), QStringLiteral("Path")},
                                    {QStringLiteral("value"), QStringLiteral("/home/person/private")},
                                    {QStringLiteral("path"), true}},
                        QVariantMap{{QStringLiteral("label"), QStringLiteral("Provider path")},
                                    {QStringLiteral("value"), QStringLiteral("telegram://account/private-item")},
                                    {QStringLiteral("path"), true}}
                    }}}
    }}};
    const QString safeReport = DebugReportFormatter::format(snapshot, QStringLiteral("now"), false);
    if (safeReport.contains(QStringLiteral("/home/person/private"))
        || safeReport.contains(QStringLiteral("telegram://account/private-item"))
        || safeReport.contains(QStringLiteral("token=private"))) {
        return fail(QStringLiteral("default report exposed a path or secret"));
    }
    const QString pathReport = DebugReportFormatter::format(snapshot, QStringLiteral("now"), true);
    if (!pathReport.contains(QStringLiteral("/home/person/private"))
        || !pathReport.contains(QStringLiteral("telegram://account/private-item"))
        || pathReport.contains(QStringLiteral("token=private"))) {
        return fail(QStringLiteral("path-enabled report did not preserve the redaction policy"));
    }
    return 0;
}
