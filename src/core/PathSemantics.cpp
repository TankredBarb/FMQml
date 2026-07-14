#include "PathSemantics.h"

#include <QUrl>

QString PathSemantics::explicitScheme(const QString &path)
{
    const QString trimmed = path.trimmed();
    const qsizetype separator = trimmed.indexOf(QStringLiteral("://"));
    if (separator <= 0) return {};

    const QString scheme = trimmed.left(separator);
    if (!scheme.at(0).isLetter()) return {};
    for (const QChar ch : scheme) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('+') && ch != QLatin1Char('.') && ch != QLatin1Char('-')) {
            return {};
        }
    }
    return scheme.toLower();
}

PathDescriptor PathSemantics::describe(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) return {};

    const QString scheme = explicitScheme(trimmed);
    if (scheme == QLatin1String("file")) return {PathKind::FileUrl, scheme};
    if (scheme == QLatin1String("archive")) return {PathKind::Archive, scheme};
    if (scheme == QLatin1String("devices")) return {PathKind::DevicesRoot, scheme};
    if (scheme == QLatin1String("favorites")) return {PathKind::FavoritesRoot, scheme};
    if (!scheme.isEmpty()) return {PathKind::Provider, scheme};
    if (trimmed.contains(QStringLiteral("://"))) return {PathKind::Unknown, {}};
    return {PathKind::Local, {}};
}

bool PathSemantics::hasExplicitNonLocalScheme(const QString &path)
{
    const QString scheme = explicitScheme(path);
    return !scheme.isEmpty() && scheme != QLatin1String("file");
}

bool PathSemantics::isProviderPath(const QString &path)
{
    return describe(path).kind == PathKind::Provider;
}

QString PathSemantics::localPathFromFileUrl(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (describe(trimmed).kind != PathKind::FileUrl) return trimmed;
    return QUrl(trimmed).toLocalFile();
}

QString PathSemantics::fileUrlFromLocalPath(const QString &path)
{
    return path.isEmpty() ? QString{} : QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
}

QString PathSemantics::compatibilityKindName(const PathDescriptor &descriptor)
{
    switch (descriptor.kind) {
    case PathKind::Archive: return QStringLiteral("archive");
    case PathKind::DevicesRoot: return QStringLiteral("devices");
    case PathKind::FavoritesRoot: return QStringLiteral("favorites");
    case PathKind::Provider:
        if (descriptor.scheme == QLatin1String("ftp")
            || descriptor.scheme == QLatin1String("gdrive")
            || descriptor.scheme == QLatin1String("mega")
            || descriptor.scheme == QLatin1String("instagram")
            || descriptor.scheme == QLatin1String("telegram")) {
            return descriptor.scheme;
        }
        return QStringLiteral("remote");
    case PathKind::Unknown: return QStringLiteral("remote");
    case PathKind::Empty:
    case PathKind::Local:
    case PathKind::FileUrl:
        return QStringLiteral("local");
    }
    return QStringLiteral("local");
}
