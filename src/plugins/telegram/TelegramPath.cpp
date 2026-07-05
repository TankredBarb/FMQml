#include "TelegramPath.h"

#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

namespace TelegramProviderInternal {

namespace {

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

bool validId(const QString &id)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_.:-]+$"));
    return !id.isEmpty() && pattern.match(id).hasMatch();
}

bool validChatId(const QString &id)
{
    static const QRegularExpression pattern(QStringLiteral("^-?[0-9]+$"));
    return pattern.match(id).hasMatch();
}

bool validPublicName(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_]{5,32}$"));
    return pattern.match(name).hasMatch();
}

bool hasUnsafeSegments(const QStringList &parts, int startIndex)
{
    for (int i = startIndex; i < parts.size(); ++i) {
        if (parts.at(i) == QLatin1String(".") || parts.at(i) == QLatin1String("..")) {
            return true;
        }
    }
    return false;
}

} // namespace

bool isTelegramSchemePath(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return false;
    }
    return trimmed.left(separatorIndex).compare(QStringLiteral("telegram"), Qt::CaseInsensitive) == 0;
}

ParsedTelegramPath parseTelegramPath(const QString &path)
{
    ParsedTelegramPath result;
    const QString trimmed = path.trimmed();
    if (!isTelegramSchemePath(trimmed)) {
        return result;
    }

    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    const QString tail = cleanTail(trimmed.mid(separatorIndex + 3));
    if (tail.isEmpty()) {
        result.valid = true;
        result.kind = TelegramPathKind::Root;
        result.normalized = QStringLiteral("telegram:///");
        return result;
    }

    const QStringList parts = tail.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return result;
    }

    const QString kind = parts.at(0).toLower();
    if (kind == QLatin1String("saved")) {
        if (hasUnsafeSegments(parts, 1)) {
            return result;
        }
        result.kind = TelegramPathKind::Saved;
        result.itemName = parts.size() > 1 ? parts.mid(1).join(QLatin1Char('/')) : QString{};
        result.loadMore = result.itemName == QLatin1String(TelegramLoadMoreItemName);
    } else if (kind == QLatin1String("chats") && parts.size() == 1) {
        result.kind = TelegramPathKind::Chats;
    } else if (kind == QLatin1String("downloads")) {
        if (hasUnsafeSegments(parts, 1)) {
            return result;
        }
        result.kind = TelegramPathKind::Downloads;
        result.itemName = parts.size() > 1 ? parts.mid(1).join(QLatin1Char('/')) : QString{};
    } else if (kind == QLatin1String("status")) {
        if (hasUnsafeSegments(parts, 1)) {
            return result;
        }
        result.kind = TelegramPathKind::Status;
        result.itemName = parts.size() > 1 ? parts.mid(1).join(QLatin1Char('/')) : QString{};
    } else if ((kind == QLatin1String("chat") || kind == QLatin1String("channel")) && parts.size() >= 2 && validId(parts.at(1))) {
        if (hasUnsafeSegments(parts, 2)) {
            return result;
        }
        result.kind = kind == QLatin1String("chat") ? TelegramPathKind::Chat : TelegramPathKind::Channel;
        result.id = parts.at(1);
        result.itemName = parts.size() > 2 ? parts.mid(2).join(QLatin1Char('/')) : QString{};
        result.loadMore = result.itemName == QLatin1String(TelegramLoadMoreItemName);
    } else {
        return result;
    }

    result.valid = true;
    result.normalized = QStringLiteral("telegram://") + parts.join(QLatin1Char('/'));
    return result;
}

QString normalizedTelegramPath(const QString &path)
{
    return parseTelegramPath(path).normalized;
}

QString parentTelegramPath(const QString &path)
{
    const ParsedTelegramPath parsed = parseTelegramPath(path);
    if (!parsed.valid || parsed.kind == TelegramPathKind::Root) {
        return {};
    }
    if ((parsed.kind == TelegramPathKind::Saved && parsed.itemName.isEmpty())
        || parsed.kind == TelegramPathKind::Chats
        || (parsed.kind == TelegramPathKind::Downloads && parsed.itemName.isEmpty())
        || (parsed.kind == TelegramPathKind::Status && parsed.itemName.isEmpty())) {
        return QStringLiteral("telegram:///");
    }
    if ((parsed.kind == TelegramPathKind::Chat || parsed.kind == TelegramPathKind::Channel) && parsed.itemName.isEmpty()) {
        return QStringLiteral("telegram://chats");
    }

    const QString tail = parsed.normalized.mid(QStringLiteral("telegram://").size());
    const int slashIndex = tail.lastIndexOf(QLatin1Char('/'));
    if (slashIndex < 0) {
        return QStringLiteral("telegram:///");
    }
    return QStringLiteral("telegram://") + tail.left(slashIndex);
}

QString fileNameForTelegramPath(const QString &path)
{
    const ParsedTelegramPath parsed = parseTelegramPath(path);
    if (!parsed.valid) {
        return {};
    }
    if (parsed.kind == TelegramPathKind::Root) {
        return QStringLiteral("Telegram");
    }
    if (parsed.kind == TelegramPathKind::Saved && parsed.itemName.isEmpty()) {
        return QStringLiteral("Saved Messages");
    }
    if (parsed.kind == TelegramPathKind::Chats) {
        return QStringLiteral("Chats and Channels");
    }
    if (parsed.kind == TelegramPathKind::Downloads) {
        return QStringLiteral("Downloads");
    }
    if (parsed.kind == TelegramPathKind::Status && parsed.itemName.isEmpty()) {
        return QStringLiteral("Provider Status");
    }
    if (parsed.loadMore) {
        return QStringLiteral("Load more...");
    }
    if (parsed.kind == TelegramPathKind::Chat && parsed.itemName.isEmpty()) {
        return QStringLiteral("Chat ") + parsed.id;
    }
    if (parsed.kind == TelegramPathKind::Channel && parsed.itemName.isEmpty()) {
        return QStringLiteral("Channel ") + parsed.id;
    }

    const QString tail = parsed.normalized.mid(QStringLiteral("telegram://").size());
    const int slashIndex = tail.lastIndexOf(QLatin1Char('/'));
    return slashIndex < 0 ? tail : tail.mid(slashIndex + 1);
}

QString childTelegramPath(const QString &parentPath, const QString &name)
{
    const QString parent = normalizedTelegramPath(parentPath);
    QString cleanName = cleanTail(name);
    if (parent.isEmpty() || cleanName.isEmpty()) {
        return parent;
    }
    if (parent == QStringLiteral("telegram:///")) {
        return QStringLiteral("telegram://") + cleanName;
    }
    return parent + QLatin1Char('/') + cleanName;
}

QString telegramPathFromUserInput(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const ParsedTelegramPath parsed = parseTelegramPath(trimmed);
    if (parsed.valid) {
        return parsed.normalized;
    }

    if (validChatId(trimmed)) {
        return QStringLiteral("telegram://chat/") + trimmed;
    }

    if (trimmed.startsWith(QLatin1Char('@'))) {
        const QString name = trimmed.mid(1);
        return validPublicName(name) ? QStringLiteral("telegram://channel/") + name : QString{};
    }

    QUrl url(trimmed);
    if (!url.isValid() || url.host().isEmpty()) {
        url = QUrl(QStringLiteral("https://") + trimmed);
    }

    const QString host = url.host().toLower();
    if (host != QLatin1String("t.me") && host != QLatin1String("telegram.me")) {
        return {};
    }

    const QStringList parts = cleanTail(url.path()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return {};
    }

    if (parts.at(0) == QLatin1String("c") && parts.size() >= 2 && validChatId(parts.at(1))) {
        return QStringLiteral("telegram://chat/-100") + parts.at(1);
    }

    const QString name = parts.at(0);
    if (name.startsWith(QLatin1Char('+')) || name == QLatin1String("joinchat")) {
        return {};
    }
    return validPublicName(name) ? QStringLiteral("telegram://channel/") + name : QString{};
}

} // namespace TelegramProviderInternal
