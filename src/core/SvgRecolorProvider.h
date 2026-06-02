#pragma once

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

class SvgRecolorProvider : public QQuickImageProvider {
public:
    SvgRecolorProvider();
    ~SvgRecolorProvider() override;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QImage renderPayload(const QString &payload, const QSize &requestedSize);

    QCache<QString, QImage> m_cache;
    mutable QMutex m_mutex;
};
