#include "LocalMountPointIndex.h"

#include <QDir>
#include <QReadWriteLock>
#include <QSet>

namespace {
QReadWriteLock &mountRootsLock()
{
    static QReadWriteLock lock;
    return lock;
}

QSet<QString> &mountRoots()
{
    static QSet<QString> roots;
    return roots;
}

QString normalizedMountRootPath(const QString &path)
{
    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    if (normalized.isEmpty() || !QDir::isAbsolutePath(normalized)) {
        return {};
    }
#ifdef Q_OS_WIN
    if (normalized.size() >= 2 && normalized.at(1) == QLatin1Char(':')) {
        return normalized.left(1).toUpper() + normalized.mid(1);
    }
#endif
    return normalized;
}
}

void LocalMountPointIndex::setMountRoots(const QStringList &roots)
{
    QSet<QString> normalizedRoots;
    for (const QString &root : roots) {
        const QString normalized = normalizedMountRootPath(root);
        if (!normalized.isEmpty()) {
            normalizedRoots.insert(normalized);
        }
    }

    QWriteLocker locker(&mountRootsLock());
    mountRoots() = std::move(normalizedRoots);
}

bool LocalMountPointIndex::isMountPoint(const QString &path)
{
    const QString normalized = normalizedMountRootPath(path);
    if (normalized.isEmpty()) {
        return false;
    }

    QReadLocker locker(&mountRootsLock());
    return mountRoots().contains(normalized);
}
