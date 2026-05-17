#pragma once

#include <QQuickImageProvider>
#include <QCache>
#include <QImage>
#include <QMutex>

class ThumbnailProvider : public QQuickImageProvider {
public:
    ThumbnailProvider();
    ~ThumbnailProvider() override;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QCache<QString, QImage> m_cache;
    QMutex m_cacheMutex;
};
