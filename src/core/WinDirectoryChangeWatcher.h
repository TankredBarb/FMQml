#pragma once

#include "DirectoryChangeWatcher.h"

#include <QFutureWatcher>
#include <atomic>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class WinDirectoryChangeWatcher final : public DirectoryChangeWatcher {
    Q_OBJECT

public:
    explicit WinDirectoryChangeWatcher(QObject *parent = nullptr);
    ~WinDirectoryChangeWatcher() override;

    bool watch(const QString &path) override;
    void stop() override;
    QString watchedPath() const override;

private:
#ifdef Q_OS_WIN
    bool startWatch(QString path);
    void requestStop();
    void closeCancelEvent();
    void stopAndWait();
    void handleWorkerFinished();
    void runWatchLoop(QString path, int generation, HANDLE cancelEvent);
    QList<DirectoryChangeEvent> parseNotifications(const QByteArray &buffer, const QString &basePath) const;
#endif

    QFutureWatcher<void> m_worker;
    QString m_watchedPath;
    QString m_pendingWatchPath;
    std::atomic<int> m_generation{0};

#ifdef Q_OS_WIN
    HANDLE m_cancelEvent = nullptr;
#endif
};
