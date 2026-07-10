#pragma once

#include <QImage>
#include <QMutex>
#include <QString>

class ThumbnailDiskCache final {
public:
    ThumbnailDiskCache();

    QImage load(const QString &key);
    void store(const QString &key, const QImage &image);

private:
    QString filePathForKey(const QString &key) const;
    void evictIfNeeded();

    QString m_root;
    QMutex m_mutex;
};
