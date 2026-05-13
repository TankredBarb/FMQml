#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QFuture>
#include <QFutureWatcher>

struct FileEntry;

class DirectoryScanner final : public QObject {
    Q_OBJECT

public:
    explicit DirectoryScanner(QObject *parent = nullptr);
    ~DirectoryScanner();

    void scan(const QString &path);
    void cancel();

    bool isRunning() const;
    QString currentPath() const;

signals:
    void started();
    void batchReady(const QList<FileEntry> &entries);
    void finished(const QString &path, bool success, const QString &error = {});

private:
    void onWatcherFinished();

    QFutureWatcher<void> m_watcher;
    QString m_currentPath;
    bool m_canceled = false;
};
