#include "MegaDiagnostics.h"

#include <QRegularExpression>

namespace MegaDiagnostics {

QString redactSensitiveText(const QString &text)
{
    QString redacted = text;

    static const QRegularExpression modernMegaLink(
        QStringLiteral(R"((https?://(?:www\.)?mega\.nz/(?:file|folder)/[A-Za-z0-9_-]+)#([^\s/?&]+))"),
        QRegularExpression::CaseInsensitiveOption);
    redacted.replace(modernMegaLink, QStringLiteral("\\1#<redacted>"));

    static const QRegularExpression legacyMegaLink(
        QStringLiteral(R"((https?://(?:www\.)?mega\.nz/#!?[FA]?!?[A-Za-z0-9_-]+)!([A-Za-z0-9_-]+))"),
        QRegularExpression::CaseInsensitiveOption);
    redacted.replace(legacyMegaLink, QStringLiteral("\\1!<redacted>"));

    static const QRegularExpression namedSecret(
        QStringLiteral(R"(((?:password|session|token|key)\s*[=:]\s*)[^\s,;]+)"),
        QRegularExpression::CaseInsensitiveOption);
    redacted.replace(namedSecret, QStringLiteral("\\1<redacted>"));

    static const QRegularExpression sdkStateRoot(
        QStringLiteral(R"(([^\s]*mega-sdk)[^\s]*)"),
        QRegularExpression::CaseInsensitiveOption);
    redacted.replace(sdkStateRoot, QStringLiteral("\\1/<redacted>"));

    return redacted;
}

} // namespace MegaDiagnostics
