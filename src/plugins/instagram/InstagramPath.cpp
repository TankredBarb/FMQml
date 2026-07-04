#include "InstagramInternal.h"

#include "InstagramAuth.h"

#include <QRegularExpression>
#include <QUrl>

namespace InstagramProviderInternal {

bool isInstagramSchemePath(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return false;
    }
    return trimmed.left(separatorIndex).compare(QStringLiteral("instagram"), Qt::CaseInsensitive) == 0;
}

QString cleanTail(QString tail)
{
    tail.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (tail.contains(QStringLiteral("//"))) {
        tail.replace(QStringLiteral("//"), QStringLiteral("/"));
    }
    while (tail.startsWith(QLatin1Char('/'))) {
        tail.remove(0, 1);
    }
    while (tail.endsWith(QLatin1Char('/'))) {
        tail.chop(1);
    }
    return tail;
}

ParsedPath parseInstagramPath(const QString &path)
{
    ParsedPath result;
    const QString trimmed = path.trimmed();
    if (!isInstagramSchemePath(trimmed)) {
        return result;
    }

    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    const QStringList parts = cleanTail(trimmed.mid(separatorIndex + 3)).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return result;
    }

    const QString kind = parts.at(0).toLower();
    if (kind != QLatin1String("post") && kind != QLatin1String("reel") && kind != QLatin1String("user")) {
        return result;
    }

    static const QRegularExpression shortcodePattern(QStringLiteral("^[A-Za-z0-9_.-]+$"));
    const QString shortcode = parts.at(1).trimmed();
    if (!shortcodePattern.match(shortcode).hasMatch()) {
        return result;
    }

    result.valid = true;
    result.kind = kind;
    result.shortcode = shortcode;
    result.rootPath = QStringLiteral("instagram://%1/%2").arg(kind, shortcode);
    result.storiesRootPath = result.rootPath + QLatin1Char('/') + QString::fromLatin1(StoriesItemName);
    result.itemName = parts.size() > 2 ? parts.mid(2).join(QLatin1Char('/')) : QString{};
    result.loadMore = result.kind == QLatin1String("user") && result.itemName == QLatin1String(LoadMoreItemName);
    result.stories = result.kind == QLatin1String("user")
        && (result.itemName == QLatin1String(StoriesItemName)
            || result.itemName.startsWith(QString::fromLatin1(StoriesItemName) + QLatin1Char('/')));
    if (result.stories && result.itemName.size() > StoriesItemNameLength) {
        result.storyItemName = result.itemName.mid(StoriesItemNameLength + 1);
    }
    result.normalized = result.itemName.isEmpty()
        ? result.rootPath
        : result.rootPath + QLatin1Char('/') + result.itemName;
    return result;
}

QString instagramUrlToPath(const QString &input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl url(trimmed, QUrl::TolerantMode);
    const QString host = url.host().toLower();
    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0) {
        return {};
    }
    if (host != QLatin1String("instagram.com")
        && host != QLatin1String("www.instagram.com")
        && host != QLatin1String("m.instagram.com")
        && host != QLatin1String("instagr.am")
        && host != QLatin1String("www.instagr.am")) {
        return {};
    }

    const QStringList parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return {};
    }

    QString kind;
    int shortcodePart = 1;
    if (parts.at(0).compare(QStringLiteral("p"), Qt::CaseInsensitive) == 0) {
        if (parts.size() < 2) {
            return {};
        }
        kind = QStringLiteral("post");
    } else if (parts.at(0).compare(QStringLiteral("reel"), Qt::CaseInsensitive) == 0
               || parts.at(0).compare(QStringLiteral("reels"), Qt::CaseInsensitive) == 0) {
        if (parts.size() < 2) {
            return {};
        }
        kind = QStringLiteral("reel");
    } else if (parts.size() >= 3
               && parts.at(1).compare(QStringLiteral("p"), Qt::CaseInsensitive) == 0) {
        kind = QStringLiteral("post");
        shortcodePart = 2;
    } else if (parts.size() >= 3
               && (parts.at(1).compare(QStringLiteral("reel"), Qt::CaseInsensitive) == 0
                   || parts.at(1).compare(QStringLiteral("reels"), Qt::CaseInsensitive) == 0)) {
        kind = QStringLiteral("reel");
        shortcodePart = 2;
    } else {
        static const QStringList reserved = {
            QStringLiteral("accounts"),
            QStringLiteral("api"),
            QStringLiteral("direct"),
            QStringLiteral("explore"),
            QStringLiteral("oauth"),
            QStringLiteral("p"),
            QStringLiteral("reel"),
            QStringLiteral("reels"),
            QStringLiteral("stories")
        };
        const QString username = parts.at(0);
        static const QRegularExpression usernamePattern(QStringLiteral("^[A-Za-z0-9_.]+$"));
        if (reserved.contains(username.toLower()) || !usernamePattern.match(username).hasMatch()) {
            return {};
        }
        return QStringLiteral("instagram://user/%1").arg(username);
    }

    static const QRegularExpression shortcodePattern(QStringLiteral("^[A-Za-z0-9_-]+$"));
    const QString shortcode = parts.at(shortcodePart);
    if (!shortcodePattern.match(shortcode).hasMatch()) {
        return {};
    }
    return QStringLiteral("instagram://%1/%2").arg(kind, shortcode);
}

QString sourceUrlFor(const ParsedPath &path)
{
    if (path.kind == QLatin1String("user")) {
        return QStringLiteral("https://www.instagram.com/api/v1/users/web_profile_info/?username=%1").arg(path.shortcode);
    }
    const QString type = path.kind == QLatin1String("reel") ? QStringLiteral("reel") : QStringLiteral("p");
    return QStringLiteral("https://www.instagram.com/%1/%2/").arg(type, path.shortcode);
}

QString mediaIdFromShortcode(const QString &shortcode)
{
    static const QString alphabet = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
    quint64 mediaId = 0;
    for (const QChar ch : shortcode) {
        const qsizetype value = alphabet.indexOf(ch);
        if (value < 0) {
            return {};
        }
        mediaId = (mediaId * 64) + static_cast<quint64>(value);
    }
    return QString::number(mediaId);
}

QString loadMorePathForRoot(const QString &rootPath)
{
    return rootPath + QLatin1Char('/') + QString::fromLatin1(LoadMoreItemName);
}

QString storiesPathForRoot(const QString &rootPath)
{
    return rootPath + QLatin1Char('/') + QString::fromLatin1(StoriesItemName);
}

} // namespace InstagramProviderInternal
