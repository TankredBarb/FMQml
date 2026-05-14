#pragma once

#include <QObject>
#include <QString>
#include <QRunnable>

class FolderSizeCalculator : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit FolderSizeCalculator(const QString &path, int generation)
        : m_path(path), m_generation(generation) {}
    void run() override;

signals:
    void resultReady(qint64 size, int generation);
    void progressUpdate(qint64 currentSize, int generation);

private:
    QString m_path;
    int m_generation;
};
