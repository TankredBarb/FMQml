#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QFuture>
#include <QFutureWatcher>

#include <atomic>

struct FileEntry;

class DirectoryScanner final : public QObject {
    Q_OBJECT

public:
    explicit DirectoryScanner(QObject *parent = nullptr);
    ~DirectoryScanner();

    void scan(const QString &path);
    void cancel();
    void setShowHidden(bool show);

    bool isRunning() const;
    QString currentPath() const;

signals:
    void started();
    void batchReady(const QList<FileEntry> &entries);
    void finished(const QString &path, bool success, const QString &error = {});

private:
    QFutureWatcher<void> m_watcher;
    QString m_currentPath;
    std::atomic<int> m_scanGeneration{0};
    bool m_showHidden = false;
};
