#pragma once

#include <QQuickImageProvider>
#include <QCache>
#include <QImage>
#include <QMutex>

class IconProvider : public QQuickImageProvider {
public:
    IconProvider();
    ~IconProvider() override;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    
    // Support async loading from multiple threads
    ImageType imageType() const { return QQuickImageProvider::Image; }
    Flags flags() const override { return QQuickImageProvider::ForceAsynchronousImageLoading; }

private:
    QImage getIcon(const QString &path, const QSize &requestedSize, bool forceDirectory = false);
    QImage getGenericIcon(const QString &path, const QSize &requestedSize, bool forceDirectory = false);
    
#ifdef Q_OS_WIN
    QImage getWindowsIcon(const QString &path, const QSize &requestedSize, bool forceDirectory = false);
#endif

    QCache<QString, QImage> m_cache;
    mutable QMutex m_mutex;
};
