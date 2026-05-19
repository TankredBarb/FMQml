#pragma once

#include <QObject>
#include <QString>
#include <QRunnable>
#include <atomic>

class FolderSizeCalculator : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit FolderSizeCalculator(const QString &path, int generation)
        : m_path(path), m_generation(generation), m_cancelled(false)
    {
        setAutoDelete(false);
    }
    void run() override;
    void cancel() { m_cancelled = true; }

signals:
    void resultReady(qint64 size, int files, int folders, int generation);
    void progressUpdate(qint64 currentSize, int files, int folders, int generation);

private:
    QString m_path;
    int m_generation;
    std::atomic<bool> m_cancelled;
};
