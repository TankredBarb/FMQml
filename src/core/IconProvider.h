#pragma once

#include <QQuickImageProvider>
#include <QCache>
#include <QImage>

class IconProvider : public QQuickImageProvider {
public:
    IconProvider();
    ~IconProvider() override;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QImage getIcon(const QString &path, const QSize &requestedSize);
    QImage getGenericIcon(const QString &path, const QSize &requestedSize);
    
#ifdef Q_OS_WIN
    QImage getWindowsIcon(const QString &path, const QSize &requestedSize);
#endif

    QCache<QString, QImage> m_cache;
};
