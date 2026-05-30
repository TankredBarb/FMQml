#include "QtDirectoryChangeWatcher.h"

#include <QFileInfo>

QtDirectoryChangeWatcher::QtDirectoryChangeWatcher(QObject *parent)
    : DirectoryChangeWatcher(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        DirectoryChangeEvent event;
        event.type = DirectoryChangeEvent::Type::Overflow;
        event.path = path;
        event.sourcePath = m_watchedPath;
        emit eventsReady(QList<DirectoryChangeEvent>{event});
    });
}

bool QtDirectoryChangeWatcher::watch(const QString &path)
{
    stop();

    const QFileInfo info(path);
    if (!info.exists() || !info.isDir()) {
        emit watchFailed(path, QStringLiteral("Folder does not exist"));
        return false;
    }

    m_watchedPath = info.absoluteFilePath();
    if (!m_watcher.addPath(m_watchedPath)) {
        const QString failedPath = m_watchedPath;
        m_watchedPath.clear();
        emit watchFailed(failedPath, QStringLiteral("Cannot watch folder"));
        return false;
    }

    return true;
}

void QtDirectoryChangeWatcher::stop()
{
    if (!m_watchedPath.isEmpty()) {
        m_watcher.removePath(m_watchedPath);
        m_watchedPath.clear();
    }
}

QString QtDirectoryChangeWatcher::watchedPath() const
{
    return m_watchedPath;
}
