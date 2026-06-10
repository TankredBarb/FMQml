#pragma once

#include "DirectoryChangeWatcher.h"

#include <QHash>
#include <QSocketNotifier>

class LinuxDirectoryChangeWatcher final : public DirectoryChangeWatcher {
    Q_OBJECT

public:
    explicit LinuxDirectoryChangeWatcher(QObject *parent = nullptr);
    ~LinuxDirectoryChangeWatcher() override;

    bool watch(const QString &path) override;
    void stop() override;
    QString watchedPath() const override;

private:
    void readAvailableEvents();
    QList<DirectoryChangeEvent> parseEvents(const QByteArray &buffer);
    DirectoryChangeEvent eventForPath(DirectoryChangeEvent::Type type, const QString &path) const;
    DirectoryChangeEvent overflowEvent() const;
    void closeWatch();

    int m_fd = -1;
    int m_watchDescriptor = -1;
    QSocketNotifier *m_notifier = nullptr;
    QString m_watchedPath;
    QHash<quint32, QString> m_pendingMoves;
    bool m_structuralFailurePending = false;
};
