#include "GDrivePath.h"

#include <QUrl>

namespace GDrivePath {

bool isSchemePath(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return false;
    }
    return trimmed.left(separatorIndex).compare(QStringLiteral("gdrive"), Qt::CaseInsensitive) == 0;
}

QString itemPathForId(const QString &id)
{
    const QString encodedId = QString::fromLatin1(QUrl::toPercentEncoding(id));
    return QString(ItemPrefix) + encodedId;
}

QString idForItemPath(const QString &path)
{
    if (!path.startsWith(ItemPrefix)) {
        return {};
    }
    return QUrl::fromPercentEncoding(path.mid(QString(ItemPrefix).size()).toUtf8());
}

QString shortcutPathForId(const QString &id)
{
    const QString encodedId = QString::fromLatin1(QUrl::toPercentEncoding(id));
    return QString(ShortcutPrefix) + encodedId;
}

QString idForShortcutPath(const QString &path)
{
    if (!path.startsWith(ShortcutPrefix)) {
        return {};
    }
    return QUrl::fromPercentEncoding(path.mid(QString(ShortcutPrefix).size()).toUtf8());
}

bool isShortcutsViewPath(const QString &path)
{
    return path == ShortcutsRoot || path.startsWith(ShortcutPrefix);
}

QString pendingPathForParentIdAndName(const QString &parentId, const QString &name)
{
    const QString cleanParentId = parentId.trimmed();
    const QString cleanName = name.trimmed();
    if (cleanParentId.isEmpty() || cleanName.isEmpty()) {
        return {};
    }
    return QString(NewPrefix)
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanParentId))
        + QLatin1Char('/')
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanName));
}

PendingPath pendingPathInfo(const QString &path)
{
    if (!path.startsWith(NewPrefix)) {
        return {};
    }

    const QString tail = path.mid(QString(NewPrefix).size());
    const int slash = tail.indexOf(QLatin1Char('/'));
    if (slash <= 0 || slash == tail.size() - 1) {
        return {};
    }

    PendingPath result;
    result.parentId = QUrl::fromPercentEncoding(tail.left(slash).toUtf8());
    result.name = QUrl::fromPercentEncoding(tail.mid(slash + 1).toUtf8());
    return result;
}

QString parentPathForDriveParentId(const QString &parentId)
{
    if (parentId == QLatin1String("root")) {
        return QString(MyDrive);
    }
    return itemPathForId(parentId);
}

QString driveParentIdForPath(const QString &path)
{
    if (path == MyDrive) {
        return QStringLiteral("root");
    }
    return idForItemPath(path);
}

QString normalizedPath(QString path)
{
    path = path.trimmed();
    if (path.compare(QStringLiteral("gdrive:"), Qt::CaseInsensitive) == 0
        || path.compare(QStringLiteral("gdrive:/"), Qt::CaseInsensitive) == 0
        || path.compare(QStringLiteral("gdrive://"), Qt::CaseInsensitive) == 0) {
        return QString(Root);
    }

    if (!isSchemePath(path)) {
        return {};
    }

    QString tail = path.mid(path.indexOf(QStringLiteral("://")) + 3);
    tail.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (tail.startsWith(QLatin1Char('/'))) {
        tail.remove(0, 1);
    }
    while (tail.endsWith(QLatin1Char('/'))) {
        tail.chop(1);
    }

    if (tail.isEmpty()) {
        return QString(Root);
    }

    const QString lowerTail = tail.toLower();
    if (lowerTail == QStringLiteral("my-drive")) {
        return QString(MyDrive);
    }
    if (lowerTail == QStringLiteral("shared-with-me")) {
        return QString(SharedWithMe);
    }
    if (lowerTail == QStringLiteral("shortcuts")) {
        return QString(ShortcutsRoot);
    }
    if (lowerTail == QStringLiteral("trash")) {
        return QString(Trash);
    }
    if (lowerTail.startsWith(QStringLiteral("item/")) && lowerTail.size() > 5) {
        return QString(ItemPrefix) + tail.mid(5);
    }
    if (lowerTail.startsWith(QStringLiteral("shortcuts/")) && lowerTail.size() > 10) {
        return QString(ShortcutPrefix) + tail.mid(10);
    }
    if (lowerTail.startsWith(QStringLiteral("new/")) && lowerTail.size() > 4) {
        const PendingPath pending = pendingPathInfo(QString(Root) + tail);
        if (pending.valid()) {
            return pendingPathForParentIdAndName(pending.parentId, pending.name);
        }
    }

    return {};
}

QString parentPath(const QString &path)
{
    const QString normalized = normalizedPath(path);
    if (normalized == MyDrive || normalized == SharedWithMe
        || normalized == ShortcutsRoot || normalized == Trash) {
        return QString(Root);
    }
    const PendingPath pending = pendingPathInfo(normalized);
    if (pending.valid()) {
        return parentPathForDriveParentId(pending.parentId);
    }
    return {};
}

QString fallbackFileNameForPath(const QString &path)
{
    const QString normalized = normalizedPath(path);
    if (normalized == Root) {
        return QStringLiteral("Google Drive");
    }
    if (normalized == MyDrive) {
        return QStringLiteral("My Drive");
    }
    if (normalized == SharedWithMe) {
        return QStringLiteral("Shared with me");
    }
    if (normalized == ShortcutsRoot) {
        return QStringLiteral("Shortcuts");
    }
    if (normalized == Trash) {
        return QStringLiteral("Trash");
    }
    const QString id = idForItemPath(normalized);
    const QString shortcutId = idForShortcutPath(normalized);
    const PendingPath pending = pendingPathInfo(normalized);
    if (pending.valid()) {
        return pending.name;
    }
    if (!shortcutId.isEmpty()) {
        return shortcutId;
    }
    return id.isEmpty() ? QStringLiteral("Google Drive") : id;
}

QString virtualIconNameForPath(const QString &path)
{
    const QString normalized = normalizedPath(path);
    if (normalized == Root) {
        return QStringLiteral("gdrive");
    }
    if (normalized == MyDrive) {
        return QStringLiteral("gdrive-mydrive");
    }
    if (normalized == SharedWithMe) {
        return QStringLiteral("gdrive-shared");
    }
    if (normalized == ShortcutsRoot) {
        return QStringLiteral("gdrive-shortcut");
    }
    if (normalized == Trash) {
        return QStringLiteral("gdrive-trash");
    }
    return {};
}

QString childPath(const QString &parentPath, const QString &name)
{
    const QString normalizedParent = normalizedPath(parentPath);
    const QString cleanName = name.trimmed();
    if (normalizedParent == Root) {
        if (cleanName.compare(QStringLiteral("My Drive"), Qt::CaseInsensitive) == 0) {
            return QString(MyDrive);
        }
        if (cleanName.compare(QStringLiteral("Shared with me"), Qt::CaseInsensitive) == 0) {
            return QString(SharedWithMe);
        }
        if (cleanName.compare(QStringLiteral("Shortcuts"), Qt::CaseInsensitive) == 0) {
            return QString(ShortcutsRoot);
        }
        if (cleanName.compare(QStringLiteral("Trash"), Qt::CaseInsensitive) == 0) {
            return QString(Trash);
        }
    }
    return {};
}

} // namespace GDrivePath
