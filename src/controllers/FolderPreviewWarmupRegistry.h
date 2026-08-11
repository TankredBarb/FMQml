#pragma once

#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QString>

namespace FolderPreviewWarmupRegistry {

inline QMutex &mutex()
{
    static QMutex value;
    return value;
}

inline QSet<QString> &paths()
{
    static QSet<QString> value;
    return value;
}

inline bool tryAcquire(const QString &path)
{
    QMutexLocker locker(&mutex());
    if (paths().contains(path)) return false;
    paths().insert(path);
    return true;
}

inline bool isRunning(const QString &path)
{
    QMutexLocker locker(&mutex());
    return paths().contains(path);
}

inline void release(const QString &path)
{
    QMutexLocker locker(&mutex());
    paths().remove(path);
}

} // namespace FolderPreviewWarmupRegistry
