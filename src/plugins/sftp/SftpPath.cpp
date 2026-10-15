#include "SftpPath.h"

#include <QStringList>

namespace SftpPath {
namespace {
QString cleanPath(const QString &path)
{
    QStringList parts;
    for (const QString &part : path.split(QLatin1Char('/'))) {
        if (part.isEmpty() || part == QLatin1String(".")) continue;
        if (part == QLatin1String("..")) {
            if (!parts.isEmpty()) parts.removeLast();
        } else {
            parts.append(part);
        }
    }
    return QStringLiteral("/") + parts.join(QLatin1Char('/'));
}
}

QString Address::identity() const
{
    QUrl origin = url;
    origin.setPath(QStringLiteral("/"));
    return origin.toString(QUrl::FullyEncoded);
}

bool isSchemePath(const QString &path)
{
    return path.trimmed().startsWith(QLatin1String("sftp://"), Qt::CaseInsensitive);
}

Address parse(const QString &path)
{
    Address result;
    result.url = QUrl(path.trimmed(), QUrl::StrictMode);
    const QString authority = result.url.authority(QUrl::FullyEncoded);
    const bool passwordInUrl = authority.contains(QLatin1Char('@'))
        && authority.section(QLatin1Char('@'), 0, 0).contains(QLatin1Char(':'));
    if (!isSchemePath(path) || !result.url.isValid() || result.url.host().isEmpty()) {
        result.error = QStringLiteral("Enter an SFTP address with a host: sftp://user@host/path.");
    } else if (result.url.hasQuery() || result.url.hasFragment()) {
        result.error = QStringLiteral("SFTP addresses cannot contain a query or fragment. Encode special characters in file names.");
    } else if (passwordInUrl) {
        result.error = QStringLiteral("Enter the password in SFTP settings, not in the address.");
    } else if (result.url.port(22) < 1 || result.url.port(22) > 65535
               || result.url.path(QUrl::FullyDecoded).contains(QChar::Null)
               || result.url.userName(QUrl::FullyDecoded).contains(QChar::Null)) {
        result.error = QStringLiteral("The SFTP port or path is invalid.");
    }
    // Never return an address containing a password, even on an invalid input.
    result.url.setPassword({});
    result.url.setScheme(QStringLiteral("sftp"));
    if (result.url.port() == 22) result.url.setPort(-1);
    result.url.setPath(cleanPath(result.url.path(QUrl::FullyDecoded)), QUrl::DecodedMode);
    return result;
}

Address fromFields(const QString &host, int port, const QString &user, const QString &path)
{
    QUrl url;
    url.setScheme(QStringLiteral("sftp"));
    url.setHost(host.trimmed());
    url.setPort(port);
    url.setUserName(user, QUrl::DecodedMode);
    url.setPath(path.isEmpty() ? QStringLiteral("/") : path, QUrl::DecodedMode);
    Address result = parse(url.toString(QUrl::FullyEncoded));
    if (port < 1 || port > 65535) result.error = QStringLiteral("Enter an SSH port between 1 and 65535.");
    if (user.isEmpty()) result.error = QStringLiteral("Enter your SSH login.");
    return result;
}

QString parent(const QString &path)
{
    Address address = parse(path);
    if (!address.valid()) return {};
    const QString remote = address.url.path(QUrl::FullyDecoded);
    address.url.setPath(remote.left(remote.lastIndexOf(QLatin1Char('/'))), QUrl::DecodedMode);
    return parse(address.encoded()).encoded();
}

QString child(const QString &path, const QString &name)
{
    Address address = parse(path);
    if (!address.valid() || name.isEmpty() || name == QLatin1String(".")
        || name == QLatin1String("..") || name.contains(QLatin1Char('/'))
        || name.contains(QChar::Null)) return {};
    QString remote = address.url.path(QUrl::FullyDecoded);
    if (!remote.endsWith(QLatin1Char('/'))) remote += QLatin1Char('/');
    address.url.setPath(remote + name, QUrl::DecodedMode);
    return address.encoded();
}

QString fileName(const QString &path)
{
    const Address address = parse(path);
    if (!address.valid()) return {};
    return address.url.path(QUrl::FullyDecoded).section(QLatin1Char('/'), -1);
}
}
