#pragma once

#include <QQuickImageProvider>
#include <QCache>
#include <QImage>
#include <QMutex>
#include <QSet>
#include <functional>

#include "ThumbnailDiskCache.h"

class ThumbnailController;

struct VisualThumbnailRequest {
    QString path;
    QSize size;
    QString mimeName;
    QString suffix;
    std::function<bool()> cancelled;
};

class ThumbnailProvider : public QQuickImageProvider {
public:
    explicit ThumbnailProvider(ThumbnailController *controller = nullptr);
    ~ThumbnailProvider() override;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    QImage requestVisualImage(const VisualThumbnailRequest &request);

private:
    QImage requestImageImpl(const QString &id, QSize *size, const QSize &requestedSize,
                           const std::function<bool()> &cancelled,
                           const QString &mimeName = {}, const QString &suffix = {});
    QCache<QString, QImage> m_cache;
    ThumbnailDiskCache m_diskCache;
    QSet<QString> m_negativeCache;
    QMutex m_cacheMutex;
    ThumbnailController *m_controller = nullptr;
};
