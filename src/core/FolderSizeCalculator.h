#pragma once

#include <QObject>
#include <QString>
#include <QRunnable>

class FolderSizeCalculator : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit FolderSizeCalculator(const QString &path) : m_path(path) {}
    void run() override;

signals:
    void resultReady(qint64 size);
    void progressUpdate(qint64 currentSize);

private:
    QString m_path;
};
