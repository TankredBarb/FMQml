#include "LinuxDirectoryChangeWatcher.h"

#ifdef Q_OS_LINUX

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QString>

#include <cerrno>
#include <cstring>
#include <sys/inotify.h>
#include <unistd.h>

namespace {
QString linuxErrorMessage(int error)
{
    return QString::fromLocal8Bit(std::strerror(error));
}

QString eventNamePath(const QString &basePath, const inotify_event *event)
{
    if (!event || event->len == 0 || event->name[0] == '\0') {
        return basePath;
    }
    return QDir::fromNativeSeparators(QDir(basePath).filePath(QString::fromLocal8Bit(event->name)));
}

bool isStructuralWatchFailure(uint32_t mask)
{
    return (mask & (IN_IGNORED | IN_UNMOUNT | IN_DELETE_SELF | IN_MOVE_SELF)) != 0;
}

bool isPartStagingPath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path)).endsWith(QStringLiteral(".part"));
}
}

LinuxDirectoryChangeWatcher::LinuxDirectoryChangeWatcher(QObject *parent)
    : DirectoryChangeWatcher(parent)
{
}

LinuxDirectoryChangeWatcher::~LinuxDirectoryChangeWatcher()
{
    stop();
}

bool LinuxDirectoryChangeWatcher::watch(const QString &path)
{
    stop();

    const QFileInfo info(path);
    if (!info.exists() || !info.isDir()) {
        emit watchFailed(path, QStringLiteral("Folder does not exist"));
        return false;
    }

    const QString absolutePath = info.absoluteFilePath();
    m_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_fd < 0) {
        emit watchFailed(absolutePath, linuxErrorMessage(errno));
        return false;
    }

    constexpr uint32_t mask = IN_CREATE
        | IN_DELETE
        | IN_MODIFY
        | IN_ATTRIB
        | IN_CLOSE_WRITE
        | IN_MOVED_FROM
        | IN_MOVED_TO
        | IN_DELETE_SELF
        | IN_MOVE_SELF
        | IN_UNMOUNT
        | IN_Q_OVERFLOW;

    const QByteArray nativePath = QFile::encodeName(absolutePath);
    m_watchDescriptor = inotify_add_watch(m_fd, nativePath.constData(), mask);
    if (m_watchDescriptor < 0) {
        const QString error = linuxErrorMessage(errno);
        closeWatch();
        emit watchFailed(absolutePath, error);
        return false;
    }

    m_watchedPath = absolutePath;
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, &LinuxDirectoryChangeWatcher::readAvailableEvents);
    return true;
}

void LinuxDirectoryChangeWatcher::stop()
{
    closeWatch();
    m_watchedPath.clear();
}

QString LinuxDirectoryChangeWatcher::watchedPath() const
{
    return m_watchedPath;
}

void LinuxDirectoryChangeWatcher::readAvailableEvents()
{
    if (m_fd < 0 || !m_notifier) {
        return;
    }

    QByteArray buffer;
    buffer.resize(16 * 1024);
    QList<DirectoryChangeEvent> events;

    while (true) {
        const ssize_t bytesRead = read(m_fd, buffer.data(), static_cast<size_t>(buffer.size()));
        if (bytesRead > 0) {
            events.append(parseEvents(QByteArray(buffer.constData(), static_cast<qsizetype>(bytesRead))));
            continue;
        }
        if (bytesRead == 0) {
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }

        const QString failedPath = m_watchedPath;
        const QString error = linuxErrorMessage(errno);
        stop();
        emit watchFailed(failedPath, error);
        return;
    }

    if (m_structuralFailurePending) {
        m_structuralFailurePending = false;
        const QString failedPath = m_watchedPath;
        stop();
        emit watchFailed(failedPath, QStringLiteral("Folder is no longer available"));
        return;
    }

    for (const QString &path : std::as_const(m_pendingMoves)) {
        events.append(eventForPath(DirectoryChangeEvent::Type::Removed, path));
    }
    m_pendingMoves.clear();

    if (!events.isEmpty()) {
        emit eventsReady(events);
    }
}

QList<DirectoryChangeEvent> LinuxDirectoryChangeWatcher::parseEvents(const QByteArray &buffer)
{
    QList<DirectoryChangeEvent> events;
    qsizetype offset = 0;

    while (offset + static_cast<qsizetype>(sizeof(inotify_event)) <= buffer.size()) {
        const auto *event = reinterpret_cast<const inotify_event *>(buffer.constData() + offset);
        const qsizetype eventSize = static_cast<qsizetype>(sizeof(inotify_event)) + static_cast<qsizetype>(event->len);
        if (eventSize <= 0 || offset + eventSize > buffer.size()) {
            events.append(overflowEvent());
            break;
        }

        if (event->wd != m_watchDescriptor && event->wd != -1) {
            offset += eventSize;
            continue;
        }

        if ((event->mask & IN_Q_OVERFLOW) != 0) {
            events.append(overflowEvent());
            offset += eventSize;
            continue;
        }

        if (isStructuralWatchFailure(event->mask)) {
            m_structuralFailurePending = true;
            offset += eventSize;
            continue;
        }

        const QString path = eventNamePath(m_watchedPath, event);
        if ((event->mask & IN_MOVED_FROM) != 0) {
            if (event->cookie != 0) {
                m_pendingMoves.insert(event->cookie, path);
            } else {
                events.append(eventForPath(DirectoryChangeEvent::Type::Removed, path));
            }
        } else if ((event->mask & IN_MOVED_TO) != 0) {
            if (event->cookie != 0 && m_pendingMoves.contains(event->cookie)) {
                DirectoryChangeEvent renamed;
                renamed.type = DirectoryChangeEvent::Type::Renamed;
                renamed.oldPath = m_pendingMoves.take(event->cookie);
                renamed.newPath = path;
                renamed.sourcePath = m_watchedPath;
                events.append(renamed);
            } else {
                events.append(eventForPath(DirectoryChangeEvent::Type::Added, path));
            }
        } else if ((event->mask & IN_CREATE) != 0) {
            events.append(eventForPath(DirectoryChangeEvent::Type::Added, path));
        } else if ((event->mask & IN_DELETE) != 0) {
            events.append(eventForPath(DirectoryChangeEvent::Type::Removed, path));
        } else if ((event->mask & (IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE)) != 0
                   && !isPartStagingPath(path)) {
            events.append(eventForPath(DirectoryChangeEvent::Type::Modified, path));
        }

        offset += eventSize;
    }

    return events;
}

DirectoryChangeEvent LinuxDirectoryChangeWatcher::eventForPath(DirectoryChangeEvent::Type type, const QString &path) const
{
    DirectoryChangeEvent event;
    event.type = type;
    event.path = path;
    event.sourcePath = m_watchedPath;
    return event;
}

DirectoryChangeEvent LinuxDirectoryChangeWatcher::overflowEvent() const
{
    return eventForPath(DirectoryChangeEvent::Type::Overflow, m_watchedPath);
}

void LinuxDirectoryChangeWatcher::closeWatch()
{
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }

    if (m_fd >= 0 && m_watchDescriptor >= 0) {
        inotify_rm_watch(m_fd, m_watchDescriptor);
    }
    m_watchDescriptor = -1;
    m_pendingMoves.clear();
    m_structuralFailurePending = false;

    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

#endif
