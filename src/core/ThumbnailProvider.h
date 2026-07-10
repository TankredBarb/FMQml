#pragma once

#include <QQuickImageProvider>
#include <QCache>
#include <QImage>
#include <QMutex>
#include <QSet>

#include "ThumbnailDiskCache.h"

class ThumbnailController;

class ThumbnailProvider : public QQuickImageProvider {
public:
    explicit ThumbnailProvider(ThumbnailController *controller = nullptr);
    ~ThumbnailProvider() override;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QCache<QString, QImage> m_cache;
    ThumbnailDiskCache m_diskCache;
    QSet<QString> m_negativeCache;
    QMutex m_cacheMutex;
    ThumbnailController *m_controller = nullptr;
};
