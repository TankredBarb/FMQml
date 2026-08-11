#include "DebugReportSanitizer.h"

#include <QRegularExpression>

QString DebugReportSanitizer::sanitize(QString text)
{
    static const QRegularExpression credentials(
        QStringLiteral("(?i)\\b([a-z][a-z0-9+.-]*://)[^/@\\s:]+:[^/@\\s]+@"));
    text.replace(credentials, QStringLiteral("\\1[REDACTED]@"));

    static const QRegularExpression bearer(QStringLiteral("(?i)\\bBearer\\s+[A-Za-z0-9._~+/=-]+"));
    text.replace(bearer, QStringLiteral("Bearer [REDACTED]"));

    static const QRegularExpression assignment(
        QStringLiteral("(?i)\\b(authorization|cookie|token|access[_-]?token|refresh[_-]?token|password|passwd|secret|session)\\b\\s*[:=]\\s*([^\\s;,&]+)"));
    text.replace(assignment, QStringLiteral("\\1=[REDACTED]"));

    static const QRegularExpression query(
        QStringLiteral("(?i)([?&](?:token|access_token|refresh_token|password|secret|session|auth)=)[^&#\\s]+"));
    text.replace(query, QStringLiteral("\\1[REDACTED]"));

    static const QRegularExpression fragment(
        QStringLiteral("(?i)(\\b[a-z][a-z0-9+.-]*://[^\\s#]+)#[^\\s]+"));
    text.replace(fragment, QStringLiteral("\\1#[REDACTED]"));

    return text;
}
