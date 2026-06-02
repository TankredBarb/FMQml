#include "ArchiveSupport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef HAS_UNOFFICIAL_BIT7Z
#include <bit7z/bit7z.hpp>
#endif

namespace {
QString normalizedLocalPath(const QString &path)
{
    if (path.isEmpty()) {
        return {};
    }

    const QString value = QDir::fromNativeSeparators(path);
    const bool hadTrailingSeparator = value.endsWith(QLatin1Char('/'));
    QString absolutePath = QDir::isAbsolutePath(value)
        ? value
        : QDir::current().absoluteFilePath(value);
    absolutePath = QDir::cleanPath(absolutePath);
    if (hadTrailingSeparator && !absolutePath.endsWith(QLatin1Char('/'))) {
        absolutePath.append(QLatin1Char('/'));
    }
    return absolutePath;
}
}

namespace ArchiveSupport {

QStringList splitArchiveTokens(const QString &path)
{
    QString raw = path;
    if (raw.startsWith(QStringLiteral("archive://"), Qt::CaseInsensitive)) {
        raw = raw.mid(10);
    }
    return raw.split(QLatin1Char('|'), Qt::KeepEmptyParts);
}

bool isArchiveExtension(const QString &suffix)
{
    static const QStringList archiveSuffixes = {
        QStringLiteral("zip"),
        QStringLiteral("7z"),
        QStringLiteral("rar"),
        QStringLiteral("tar"),
        QStringLiteral("gz"),
        QStringLiteral("bz2"),
        QStringLiteral("xz"),
        QStringLiteral("cab"),
        QStringLiteral("tgz"),
        QStringLiteral("txz"),
        QStringLiteral("tbz2"),
        QStringLiteral("zst"),
        QStringLiteral("apk"),
    };
    return archiveSuffixes.contains(suffix.toLower());
}

bool isArchiveFilePath(const QString &path)
{
    const QFileInfo info(path);
    return isArchiveExtension(info.suffix()) && info.isFile();
}

bool isArchivePath(const QString &path)
{
    return path.startsWith(QStringLiteral("archive://"), Qt::CaseInsensitive);
}

bool archiveBackendAvailable()
{
#ifdef HAS_UNOFFICIAL_BIT7Z
    static const bool available = []() {
        try {
            bit7z::Bit7zLibrary lib;
            return true;
        } catch (...) {
            try {
                const QString libraryPath = archiveLibraryPath();
                if (libraryPath.isEmpty()) {
                    return false;
                }
                bit7z::Bit7zLibrary lib(libraryPath.toStdString());
                return true;
            } catch (...) {
                return false;
            }
        }
    }();
    return available;
#else
    return false;
#endif
}

QString archiveLibraryPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("7z.dll")),
        QDir::current().filePath(QStringLiteral("7z.dll")),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QDir::toNativeSeparators(candidate);
        }
    }
    return {};
}

QString sevenZipExecutablePath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("7z.exe")),
        QStandardPaths::findExecutable(QStringLiteral("7z")),
        QStringLiteral("C:/Program Files/7-Zip/7z.exe"),
        QStringLiteral("C:/Program Files (x86)/7-Zip/7z.exe"),
    };

    for (const QString &candidate : candidates) {
        if (candidate.isEmpty()) {
            continue;
        }
        const QFileInfo info(QDir::fromNativeSeparators(candidate));
        if (info.isFile() && info.isExecutable()) {
            return QDir::toNativeSeparators(info.absoluteFilePath());
        }
    }
    return {};
}

QString physicalArchivePath(const QString &path)
{
    const QStringList tokens = splitArchiveTokens(path);
    return tokens.isEmpty() ? QString{} : normalizedLocalPath(tokens.first());
}

QStringList archiveSegments(const QString &path)
{
    const QStringList tokens = splitArchiveTokens(path);
    if (tokens.size() <= 1) {
        return {};
    }
    QStringList segments = tokens.mid(1);
    for (QString &segment : segments) {
        segment = QDir::fromNativeSeparators(segment.trimmed());
    }
    return segments;
}

QString archiveBrowsePath(const QString &path)
{
    const QStringList segments = archiveSegments(path);
    if (segments.isEmpty()) {
        return QStringLiteral("/");
    }
    return segments.last().isEmpty() ? QStringLiteral("/") : QDir::fromNativeSeparators(segments.last());
}

QString archiveRootPath(const QString &physicalArchivePath)
{
    return QStringLiteral("archive://") + normalizedLocalPath(physicalArchivePath) + QStringLiteral("|/");
}

QString archiveRootPathForPath(const QString &path)
{
    if (!isArchivePath(path)) {
        return archiveRootPath(path);
    }

    QString normalized = normalizeArchivePath(path);
    if (normalized.endsWith(QStringLiteral("|/"))) {
        return normalized;
    }

    if (normalized.endsWith(QLatin1Char('/'))) {
        normalized.append(QLatin1Char('/'));
        return normalized;
    }

    return normalized + QStringLiteral("|/");
}

QString archiveChildPath(const QString &parentPath, const QString &childName)
{
    if (parentPath.isEmpty()) {
        return {};
    }

    if (!isArchivePath(parentPath)) {
        return QDir::cleanPath(QDir(parentPath).filePath(childName));
    }

    QString base = parentPath;
    if (!base.endsWith(QLatin1Char('|'))) {
        if (!base.endsWith(QLatin1Char('/'))) {
            base.append(QLatin1Char('|'));
        }
    }
    QString child = QDir::fromNativeSeparators(childName);
    if (child.startsWith(QLatin1Char('/'))) {
        child.remove(0, 1);
    }
    return base + child;
}

QString archiveParentPath(const QString &path)
{
    if (!isArchivePath(path)) {
        return QFileInfo(path).absoluteDir().absolutePath();
    }

    const QStringList tokens = splitArchiveTokens(path);
    if (tokens.size() <= 1) {
        return QFileInfo(normalizedLocalPath(tokens.value(0))).absoluteDir().absolutePath();
    }

    auto buildArchivePath = [](const QStringList &parts) {
        return QStringLiteral("archive://") + parts.join(QLatin1Char('|'));
    };

    const QString last = QDir::fromNativeSeparators(tokens.last().trimmed());
    if (last == QLatin1String("/") || last.isEmpty()) {
        if (tokens.size() == 2) {
            return QFileInfo(normalizedLocalPath(tokens.first())).absoluteDir().absolutePath();
        }

        QStringList parts;
        parts << normalizedLocalPath(tokens.first());
        for (int i = 1; i < tokens.size() - 2; ++i) {
            parts << QDir::fromNativeSeparators(tokens.at(i).trimmed());
        }
        const QString parentContainer = QDir::fromNativeSeparators(tokens.at(tokens.size() - 2).trimmed());
        QString parentRel = parentContainer;
        if (parentRel.startsWith(QLatin1Char('/'))) {
            parentRel.remove(0, 1);
        }
        if (parentRel.endsWith(QLatin1Char('/'))) {
            parentRel.chop(1);
        }
        const int slash = parentRel.lastIndexOf(QLatin1Char('/'));
        if (slash >= 0) {
            parentRel = parentRel.left(slash);
        } else {
            parentRel.clear();
        }
        if (parentRel.isEmpty()) {
            parts << QStringLiteral("/");
        } else {
            parts << (QStringLiteral("/") + parentRel);
        }
        return buildArchivePath(parts);
    }

    QStringList parts;
    parts << normalizedLocalPath(tokens.first());
    for (int i = 1; i < tokens.size() - 1; ++i) {
        parts << QDir::fromNativeSeparators(tokens.at(i).trimmed());
    }

    QString parentRel = QDir::fromNativeSeparators(last);
    if (parentRel.startsWith(QLatin1Char('/'))) {
        parentRel.remove(0, 1);
    }
    while (parentRel.endsWith(QLatin1Char('/'))) {
        parentRel.chop(1);
    }
    const int slash = parentRel.lastIndexOf(QLatin1Char('/'));
    if (slash >= 0) {
        parentRel = parentRel.left(slash);
    } else {
        parentRel.clear();
    }

    if (parentRel.isEmpty()) {
        parts << QStringLiteral("/");
    } else {
        parts << (QStringLiteral("/") + parentRel);
    }
    return buildArchivePath(parts);
}

QString archiveFileName(const QString &path)
{
    if (!isArchivePath(path)) {
        return QFileInfo(path).fileName();
    }

    const QStringList tokens = splitArchiveTokens(path);
    if (tokens.isEmpty()) {
        return {};
    }

    QString last = tokens.last();
    last = QDir::fromNativeSeparators(last);
    if (last.endsWith(QLatin1Char('/')) && last.size() > 1) {
        last.chop(1);
    }
    if (last == QLatin1String("/")) {
        if (tokens.size() >= 3) {
            QString container = QDir::fromNativeSeparators(tokens.at(tokens.size() - 2).trimmed());
            if (container.startsWith(QLatin1Char('/'))) {
                container.remove(0, 1);
            }
            if (container.endsWith(QLatin1Char('/'))) {
                container.chop(1);
            }
            return QFileInfo(container).fileName();
        }
        const QFileInfo info(normalizedLocalPath(tokens.first()));
        return info.fileName();
    }
    return QFileInfo(last).fileName();
}

QString normalizeArchivePath(const QString &path)
{
    if (!isArchivePath(path)) {
        return normalizedLocalPath(path);
    }

    QStringList tokens = splitArchiveTokens(path);
    if (tokens.isEmpty()) {
        return {};
    }

    tokens[0] = normalizedLocalPath(tokens[0]);
    for (int i = 1; i < tokens.size(); ++i) {
        tokens[i] = QDir::fromNativeSeparators(tokens[i].trimmed());
        if (tokens[i].isEmpty()) {
            tokens[i] = QStringLiteral("/");
        }
    }
    return QStringLiteral("archive://") + tokens.join(QLatin1Char('|'));
}

QString stripArchiveScheme(const QString &path)
{
    if (!isArchivePath(path)) {
        return path;
    }
    return path.mid(10);
}

} // namespace ArchiveSupport
