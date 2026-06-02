#include "WinDirectoryChangeWatcher.h"

#include <QByteArray>
#include <QDir>
#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QStringList>
#include <QtConcurrent>
#include <cstddef>
#include <utility>

#ifdef Q_OS_WIN

namespace {
QString nativeErrorMessage(DWORD error)
{
    wchar_t *messageBuffer = nullptr;
    const DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&messageBuffer),
        0,
        nullptr);

    QString message = size > 0 && messageBuffer
        ? QString::fromWCharArray(messageBuffer, static_cast<int>(size)).trimmed()
        : QStringLiteral("Windows error %1").arg(error);

    if (messageBuffer) {
        LocalFree(messageBuffer);
    }
    return message;
}

QString childPathForNotification(const QString &basePath, const FILE_NOTIFY_INFORMATION *info)
{
    const QString relativeName = QString::fromWCharArray(info->FileName, static_cast<int>(info->FileNameLength / sizeof(wchar_t)));
    return QDir::fromNativeSeparators(QDir(basePath).filePath(relativeName));
}

QString watcherExtendedLengthWindowsPath(const QString &path)
{
    QString nativePath = QDir::toNativeSeparators(path);
    if (nativePath.startsWith(QStringLiteral("\\\\?\\"))) {
        return nativePath;
    }
    if (nativePath.startsWith(QStringLiteral("\\\\"))) {
        return QStringLiteral("\\\\?\\UNC\\") + nativePath.mid(2);
    }
    return QStringLiteral("\\\\?\\") + nativePath;
}

bool watchDebugEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_WATCH_DEBUG")
        || qEnvironmentVariableIsSet("FM_NAV_TRACE");
    return enabled;
}

void traceWinWatch(const char *stage, const QString &path, int generation, const QString &detail = {})
{
    if (!watchDebugEnabled()) {
        return;
    }
    qInfo().noquote() << "[WinWatch]" << stage
                      << "path=" << path
                      << "gen=" << generation
                      << detail;
}

void cancelPendingIo(HANDLE directoryHandle, OVERLAPPED *overlapped)
{
    if (!directoryHandle || directoryHandle == INVALID_HANDLE_VALUE || !overlapped) {
        return;
    }

    const BOOL cancelOk = CancelIoEx(directoryHandle, overlapped);
    const DWORD cancelError = cancelOk ? ERROR_SUCCESS : GetLastError();
    DWORD ignoredBytes = 0;
    const BOOL resultOk = GetOverlappedResult(directoryHandle, overlapped, &ignoredBytes, TRUE);
    if (watchDebugEnabled() && resultOk) {
        qInfo().noquote() << "[WinWatch] cancel-pending-io"
                          << "cancelOk=" << cancelOk
                          << "cancelError=" << cancelError
                          << "resultOk=" << resultOk
                          << "resultError=" << (resultOk ? ERROR_SUCCESS : GetLastError())
                          << "bytes=" << ignoredBytes;
    }
}
}

#endif

WinDirectoryChangeWatcher::WinDirectoryChangeWatcher(QObject *parent)
    : DirectoryChangeWatcher(parent)
{
#ifdef Q_OS_WIN
    connect(&m_worker, &QFutureWatcher<void>::finished,
            this, &WinDirectoryChangeWatcher::handleWorkerFinished);
#endif
}

WinDirectoryChangeWatcher::~WinDirectoryChangeWatcher()
{
#ifdef Q_OS_WIN
    stopAndWait();
#else
    stop();
#endif
}

bool WinDirectoryChangeWatcher::watch(const QString &path)
{
#ifndef Q_OS_WIN
    Q_UNUSED(path)
    return false;
#else
    QElapsedTimer timer;
    timer.start();
    traceWinWatch("watch-begin", path, m_generation.load(),
                  QStringLiteral("workerRunning=%1 watched=%2")
                      .arg(m_worker.isRunning())
                      .arg(QDir::toNativeSeparators(m_watchedPath)));
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(watcherExtendedLengthWindowsPath(path).utf16()));
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        traceWinWatch("watch-missing", path, m_generation.load());
        emit watchFailed(path, QStringLiteral("Folder does not exist"));
        return false;
    }

    const QString nextPath = QDir::fromNativeSeparators(QDir::cleanPath(path));
    if (m_worker.isRunning()) {
        m_pendingWatchPath = nextPath;
        requestStop();
        m_watchedPath.clear();
        traceWinWatch("watch-deferred", nextPath, m_generation.load(),
                      QStringLiteral("elapsedMs=%1").arg(timer.elapsed()));
        return true;
    }

    closeCancelEvent();
    const bool result = startWatch(nextPath);
    traceWinWatch("watch-end", nextPath, m_generation.load(),
                  QStringLiteral("result=%1 elapsedMs=%2").arg(result).arg(timer.elapsed()));
    return result;
#endif
}

void WinDirectoryChangeWatcher::stop()
{
#ifdef Q_OS_WIN
    QElapsedTimer timer;
    timer.start();
    traceWinWatch("stop-begin", m_watchedPath, m_generation.load(),
                  QStringLiteral("workerRunning=%1 pending=%2")
                      .arg(m_worker.isRunning())
                      .arg(QDir::toNativeSeparators(m_pendingWatchPath)));
    m_pendingWatchPath.clear();
    if (m_worker.isRunning()) {
        requestStop();
        m_watchedPath.clear();
        traceWinWatch("stop-end", {}, m_generation.load(),
                      QStringLiteral("mode=async elapsedMs=%1").arg(timer.elapsed()));
        return;
    }

    closeCancelEvent();
    m_watchedPath.clear();
    traceWinWatch("stop-end", {}, m_generation.load(),
                  QStringLiteral("mode=idle elapsedMs=%1").arg(timer.elapsed()));
#endif
}

QString WinDirectoryChangeWatcher::watchedPath() const
{
    return m_watchedPath;
}

#ifdef Q_OS_WIN

bool WinDirectoryChangeWatcher::startWatch(QString path)
{
    QElapsedTimer timer;
    timer.start();
    m_watchedPath = std::move(path);
    m_cancelEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!m_cancelEvent) {
        const QString failedPath = m_watchedPath;
        m_watchedPath.clear();
        traceWinWatch("watch-cancel-event-failed", failedPath, m_generation.load(), nativeErrorMessage(GetLastError()));
        emit watchFailed(failedPath, nativeErrorMessage(GetLastError()));
        return false;
    }

    const int generation = ++m_generation;
    const HANDLE cancelEvent = m_cancelEvent;
    const QString watchedPath = m_watchedPath;

    m_worker.setFuture(QtConcurrent::run([this, watchedPath, generation, cancelEvent]() {
        runWatchLoop(watchedPath, generation, cancelEvent);
    }));

    traceWinWatch("startWatch", watchedPath, generation,
                  QStringLiteral("elapsedMs=%1").arg(timer.elapsed()));
    return true;
}

void WinDirectoryChangeWatcher::requestStop()
{
    ++m_generation;
    if (m_cancelEvent) {
        SetEvent(m_cancelEvent);
    }
}

void WinDirectoryChangeWatcher::closeCancelEvent()
{
    if (m_cancelEvent) {
        CloseHandle(m_cancelEvent);
        m_cancelEvent = nullptr;
    }
}

void WinDirectoryChangeWatcher::stopAndWait()
{
    m_pendingWatchPath.clear();
    requestStop();
    if (m_worker.isRunning()) {
        m_worker.waitForFinished();
    }
    closeCancelEvent();
    m_watchedPath.clear();
}

void WinDirectoryChangeWatcher::handleWorkerFinished()
{
    traceWinWatch("worker-finished", m_watchedPath, m_generation.load(),
                  QStringLiteral("pending=%1").arg(QDir::toNativeSeparators(m_pendingWatchPath)));
    closeCancelEvent();
    m_watchedPath.clear();

    if (m_pendingWatchPath.isEmpty()) {
        return;
    }

    const QString nextPath = m_pendingWatchPath;
    m_pendingWatchPath.clear();
    startWatch(nextPath);
}

void WinDirectoryChangeWatcher::runWatchLoop(QString path, int generation, HANDLE cancelEvent)
{
    QElapsedTimer loopTimer;
    loopTimer.start();
    traceWinWatch("loop-begin", path, generation);

    auto emitFailure = [this, path](const QString &error) {
        traceWinWatch("emit-failure", path, m_generation.load(), error);
        QMetaObject::invokeMethod(this, [this, path, error]() {
            emit watchFailed(path, error);
        }, Qt::QueuedConnection);
    };

    const QString nativePath = watcherExtendedLengthWindowsPath(path);
    const HANDLE directoryHandle = CreateFileW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (directoryHandle == INVALID_HANDLE_VALUE) {
        emitFailure(nativeErrorMessage(GetLastError()));
        traceWinWatch("loop-open-failed", path, generation,
                      QStringLiteral("elapsedMs=%1").arg(loopTimer.elapsed()));
        return;
    }

    QByteArray buffer;
    buffer.resize(64 * 1024);

    while (generation == m_generation.load()) {
        DWORD bytesReturned = 0;
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent) {
            emitFailure(nativeErrorMessage(GetLastError()));
            traceWinWatch("loop-overlapped-event-failed", path, generation);
            break;
        }

        const BOOL started = ReadDirectoryChangesW(
            directoryHandle,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME
                | FILE_NOTIFY_CHANGE_DIR_NAME
                | FILE_NOTIFY_CHANGE_ATTRIBUTES
                | FILE_NOTIFY_CHANGE_SIZE
                | FILE_NOTIFY_CHANGE_LAST_WRITE
                | FILE_NOTIFY_CHANGE_CREATION,
            &bytesReturned,
            &overlapped,
            nullptr);

        if (!started) {
            emitFailure(nativeErrorMessage(GetLastError()));
            traceWinWatch("loop-read-start-failed", path, generation);
            CloseHandle(overlapped.hEvent);
            break;
        }

        const HANDLE waitHandles[] = {overlapped.hEvent, cancelEvent};
        const DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0 + 1) {
            cancelPendingIo(directoryHandle, &overlapped);
            CloseHandle(overlapped.hEvent);
            break;
        }
        if (waitResult != WAIT_OBJECT_0) {
            emitFailure(nativeErrorMessage(GetLastError()));
            cancelPendingIo(directoryHandle, &overlapped);
            CloseHandle(overlapped.hEvent);
            traceWinWatch("loop-wait-failed", path, generation);
            break;
        }

        if (!GetOverlappedResult(directoryHandle, &overlapped, &bytesReturned, FALSE)) {
            emitFailure(nativeErrorMessage(GetLastError()));
            CloseHandle(overlapped.hEvent);
            traceWinWatch("loop-result-failed", path, generation);
            break;
        }
        CloseHandle(overlapped.hEvent);

        QList<DirectoryChangeEvent> events;
        if (bytesReturned == 0) {
            DirectoryChangeEvent event;
            event.type = DirectoryChangeEvent::Type::Overflow;
            event.path = path;
            event.sourcePath = path;
            events.append(event);
        } else {
            events = parseNotifications(QByteArray(buffer.constData(), static_cast<qsizetype>(bytesReturned)), path);
        }

        if (!events.isEmpty() && generation == m_generation.load()) {
            QMetaObject::invokeMethod(this, [this, events]() {
                emit eventsReady(events);
            }, Qt::QueuedConnection);
        } else if (!events.isEmpty()) {
            traceWinWatch("drop-events-generation", path, generation,
                          QStringLiteral("count=%1 currentGen=%2").arg(events.size()).arg(m_generation.load()));
        }
    }

    CloseHandle(directoryHandle);
    traceWinWatch("loop-end", path, generation,
                  QStringLiteral("elapsedMs=%1 currentGen=%2")
                      .arg(loopTimer.elapsed())
                      .arg(m_generation.load()));
}

QList<DirectoryChangeEvent> WinDirectoryChangeWatcher::parseNotifications(const QByteArray &buffer, const QString &basePath) const
{
    QList<DirectoryChangeEvent> events;
    QString pendingOldName;
    qsizetype offset = 0;
    const qsizetype minimumRecordSize = static_cast<qsizetype>(offsetof(FILE_NOTIFY_INFORMATION, FileName));

    while (offset + minimumRecordSize <= buffer.size()) {
        const auto *info = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(buffer.constData() + offset);
        const qsizetype remaining = buffer.size() - offset;
        const qsizetype fileNameLength = static_cast<qsizetype>(info->FileNameLength);
        if (fileNameLength < 0 || minimumRecordSize + fileNameLength > remaining) {
            DirectoryChangeEvent event;
            event.type = DirectoryChangeEvent::Type::Overflow;
            event.path = basePath;
            event.sourcePath = basePath;
            events.append(event);
            break;
        }

        const QString path = childPathForNotification(basePath, info);

        DirectoryChangeEvent event;
        event.sourcePath = basePath;
        switch (info->Action) {
        case FILE_ACTION_ADDED:
            event.type = DirectoryChangeEvent::Type::Added;
            event.path = path;
            events.append(event);
            break;
        case FILE_ACTION_REMOVED:
            event.type = DirectoryChangeEvent::Type::Removed;
            event.path = path;
            events.append(event);
            break;
        case FILE_ACTION_MODIFIED:
            event.type = DirectoryChangeEvent::Type::Modified;
            event.path = path;
            events.append(event);
            break;
        case FILE_ACTION_RENAMED_OLD_NAME:
            pendingOldName = path;
            break;
        case FILE_ACTION_RENAMED_NEW_NAME:
            if (!pendingOldName.isEmpty()) {
                event.type = DirectoryChangeEvent::Type::Renamed;
                event.oldPath = pendingOldName;
                event.newPath = path;
                events.append(event);
                pendingOldName.clear();
            } else {
                event.type = DirectoryChangeEvent::Type::Added;
                event.path = path;
                events.append(event);
            }
            break;
        default:
            event.type = DirectoryChangeEvent::Type::Overflow;
            event.path = basePath;
            events.append(event);
            break;
        }

        if (info->NextEntryOffset == 0) {
            break;
        }
        if (info->NextEntryOffset < minimumRecordSize
            || static_cast<qsizetype>(info->NextEntryOffset) > remaining) {
            DirectoryChangeEvent overflow;
            overflow.type = DirectoryChangeEvent::Type::Overflow;
            overflow.path = basePath;
            overflow.sourcePath = basePath;
            events.append(overflow);
            break;
        }
        offset += static_cast<qsizetype>(info->NextEntryOffset);
    }

    if (!pendingOldName.isEmpty()) {
        DirectoryChangeEvent event;
        event.type = DirectoryChangeEvent::Type::Removed;
        event.path = pendingOldName;
        event.sourcePath = basePath;
        events.append(event);
    }

    return events;
}

#endif
