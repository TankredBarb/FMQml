#pragma once

#include <QQuickImageProvider>
#include <QCache>
#include <QImage>

class ThumbnailProvider : public QQuickImageProvider {
public:
    ThumbnailProvider();
    ~ThumbnailProvider() override;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QCache<QString, QImage> m_cache;
};
