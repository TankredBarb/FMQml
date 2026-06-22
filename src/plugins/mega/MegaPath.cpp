#include "MegaPath.h"

#include <QUrl>
#include <QRegularExpression>

namespace MegaPath {

bool isSchemePath(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return false;
    }
    return trimmed.left(separatorIndex).compare(QStringLiteral("mega"), Qt::CaseInsensitive) == 0;
}

QString normalizedPath(QString path)
{
    path = path.trimmed();
    if (path.isEmpty() ||
        path.compare(QStringLiteral("mega:"), Qt::CaseInsensitive) == 0 ||
        path.compare(QStringLiteral("mega:/"), Qt::CaseInsensitive) == 0 ||
        path.compare(QStringLiteral("mega://"), Qt::CaseInsensitive) == 0 ||
        path.compare(QStringLiteral("mega:///"), Qt::CaseInsensitive) == 0) {
        return QString(Root);
    }

    if (!isSchemePath(path)) {
        return {};
    }

    // Clean up slashes
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));

    // Extract everything after scheme
    const int separator = path.indexOf(QStringLiteral("://"));
    QString tail = path.mid(separator + 3);

    // Remove duplicate slashes
    while (tail.contains(QStringLiteral("//"))) {
        tail.replace(QStringLiteral("//"), QStringLiteral("/"));
    }

    while (tail.startsWith(QLatin1Char('/'))) {
        tail.remove(0, 1);
    }
    while (tail.endsWith(QLatin1Char('/'))) {
        tail.chop(1);
    }

    if (tail.isEmpty()) {
        return QString(Root);
    }

    if (tail.startsWith(QStringLiteral("link/"))) {
        return QString(LinkPrefix) + tail.mid(5);
    }

    return QString(Root) + tail;
}

QString parentPath(const QString &path)
{
    const QString normalized = normalizedPath(path);
    if (normalized == Root) {
        return {};
    }

    if (isLinkPath(normalized)) {
        const QString tail = normalized.mid(QString(LinkPrefix).size());
        const int lastSlash = tail.lastIndexOf(QLatin1Char('/'));
        if (lastSlash < 0) {
            // It is a link root (mega://link/<linkId>) -> its parent is mega:///
            return QString(Root);
        }
        return QString(LinkPrefix) + tail.left(lastSlash);
    } else {
        const QString tail = normalized.mid(QString(Root).size());
        const int lastSlash = tail.lastIndexOf(QLatin1Char('/'));
        if (lastSlash < 0) {
            // e.g. mega:///Cloud Drive -> parent is mega:///
            return QString(Root);
        }
        return QString(Root) + tail.left(lastSlash);
    }
}

QString childPath(const QString &parentPath, const QString &name)
{
    const QString normalizedParent = normalizedPath(parentPath);
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) {
        return normalizedParent;
    }

    if (normalizedParent.endsWith(QLatin1Char('/'))) {
        return normalizedParent + cleanName;
    }
    return normalizedParent + QLatin1Char('/') + cleanName;
}

QString fallbackFileNameForPath(const QString &path)
{
    const QString normalized = normalizedPath(path);
    if (normalized == Root) {
        return QStringLiteral("MEGA");
    }

    if (isLinkPath(normalized)) {
        const QString tail = normalized.mid(QString(LinkPrefix).size());
        const int lastSlash = tail.lastIndexOf(QLatin1Char('/'));
        if (lastSlash < 0) {
            return tail; // returns the linkId
        }
        return tail.mid(lastSlash + 1);
    } else {
        const QString tail = normalized.mid(QString(Root).size());
        const int lastSlash = tail.lastIndexOf(QLatin1Char('/'));
        if (lastSlash < 0) {
            return tail;
        }
        return tail.mid(lastSlash + 1);
    }
}

bool isLinkPath(const QString &path)
{
    return path.startsWith(LinkPrefix);
}

QString linkIdForPath(const QString &path)
{
    if (!isLinkPath(path)) {
        return {};
    }
    const QString tail = path.mid(QString(LinkPrefix).size());
    const int slash = tail.indexOf(QLatin1Char('/'));
    if (slash < 0) {
        return tail;
    }
    return tail.left(slash);
}

QString relativePathForPath(const QString &path)
{
    const QString normalized = normalizedPath(path);
    if (normalized == Root) {
        return {};
    }

    if (isLinkPath(normalized)) {
        const QString tail = normalized.mid(QString(LinkPrefix).size());
        const int slash = tail.indexOf(QLatin1Char('/'));
        if (slash < 0) {
            return {};
        }
        return tail.mid(slash + 1);
    } else {
        return normalized.mid(QString(Root).size());
    }
}

QString fromUserInput(const QString &input, QString &linkId, QString &linkKey, bool &isFolder)
{
    linkId.clear();
    linkKey.clear();
    isFolder = false;

    QString clean = input.trimmed();
    if (clean.isEmpty()) {
        return {};
    }

    // Regular expressions for MEGA public URLs
    // Format 1: https://mega.nz/file/ID#KEY
    // Format 2: https://mega.nz/folder/ID#KEY
    // Format 3: https://mega.nz/#!ID!KEY  (legacy file)
    // Format 4: https://mega.nz/#F!ID!KEY  (legacy folder)

    // Normalize host first
    static const QRegularExpression hostPattern(QStringLiteral("^(https?://)?(www\\.)?mega\\.(nz|co\\.nz)/"));
    auto match = hostPattern.match(clean);
    if (!match.hasMatch()) {
        // If it starts with mega://link/, it is already an internal path
        if (clean.startsWith(LinkPrefix)) {
            return normalizedPath(clean);
        }
        return {};
    }

    QString pathPart = clean.mid(match.capturedLength());

    // Check modern folder link: folder/ID#KEY
    if (pathPart.startsWith(QStringLiteral("folder/"))) {
        isFolder = true;
        const QString tail = pathPart.mid(7);
        const int hashIndex = tail.indexOf(QLatin1Char('#'));
        if (hashIndex > 0) {
            linkId = tail.left(hashIndex);
            linkKey = tail.mid(hashIndex + 1);
        }
    }
    // Check modern file link: file/ID#KEY
    else if (pathPart.startsWith(QStringLiteral("file/"))) {
        isFolder = false;
        const QString tail = pathPart.mid(5);
        const int hashIndex = tail.indexOf(QLatin1Char('#'));
        if (hashIndex > 0) {
            linkId = tail.left(hashIndex);
            linkKey = tail.mid(hashIndex + 1);
        }
    }
    // Check legacy links starting with #
    else if (pathPart.startsWith(QLatin1Char('#'))) {
        QString hashTail = pathPart.mid(1);
        if (hashTail.startsWith(QStringLiteral("F!"))) {
            isFolder = true;
            const QString tail = hashTail.mid(2);
            const int exclamationIndex = tail.indexOf(QLatin1Char('!'));
            if (exclamationIndex > 0) {
                linkId = tail.left(exclamationIndex);
                linkKey = tail.mid(exclamationIndex + 1);
            }
        } else if (hashTail.startsWith(QLatin1Char('!'))) {
            isFolder = false;
            const QString tail = hashTail.mid(1);
            const int exclamationIndex = tail.indexOf(QLatin1Char('!'));
            if (exclamationIndex > 0) {
                linkId = tail.left(exclamationIndex);
                linkKey = tail.mid(exclamationIndex + 1);
            }
        }
    }

    if (linkId.isEmpty() || linkKey.isEmpty()) {
        return {};
    }

    // Clean up any remaining URL parameter garbage (e.g. if key ends with some queries)
    const int question = linkKey.indexOf(QLatin1Char('?'));
    if (question >= 0) {
        linkKey = linkKey.left(question);
    }

    return QString(LinkPrefix) + linkId;
}

} // namespace MegaPath
