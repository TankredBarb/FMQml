#include "IconProvider.h"
#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_cache(500) // Cache 500 icons
{
}

IconProvider::~IconProvider() = default;

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QString path = id;
    // id might contain size info if needed, e.g. "path?size=32"
    // For now, just the path.
    
    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(32, 32);
    if (size) {
        *size = targetSize;
    }

    QString cacheKey = path + QString::number(targetSize.width()) + "x" + QString::number(targetSize.height());
    if (m_cache.contains(cacheKey)) {
        return *m_cache.object(cacheKey);
    }

    QImage icon = getIcon(path, targetSize);
    m_cache.insert(cacheKey, new QImage(icon));
    return icon;
}

QImage IconProvider::getIcon(const QString &path, const QSize &requestedSize)
{
#ifdef Q_OS_WIN
    return getWindowsIcon(path, requestedSize);
#else
    return getGenericIcon(path, requestedSize);
#endif
}

#ifdef Q_OS_WIN
QImage IconProvider::getWindowsIcon(const QString &path, const QSize &requestedSize)
{
    SHFILEINFO sfi;
    std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    
    UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES;
    if (requestedSize.width() <= 16) {
        flags |= SHGFI_SMALLICON;
    } else {
        flags |= SHGFI_LARGEICON;
    }

    // Check if it's a directory
    DWORD attr = 0;
    if (QFileInfo(path).isDir()) {
        attr = FILE_ATTRIBUTE_DIRECTORY;
    } else {
        attr = FILE_ATTRIBUTE_NORMAL;
    }

    if (SHGetFileInfo(wpath.c_str(), attr, &sfi, sizeof(sfi), flags)) {
        ICONINFO iconInfo;
        if (GetIconInfo(sfi.hIcon, &iconInfo)) {
            BITMAP bmp;
            if (GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp)) {
                QImage image(bmp.bmWidth, bmp.bmHeight, QImage::Format_ARGB32);
                HDC hdc = GetDC(NULL);
                BITMAPINFO bmi = {0};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = bmp.bmWidth;
                bmi.bmiHeader.biHeight = -bmp.bmHeight;
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                GetDIBits(hdc, iconInfo.hbmColor, 0, bmp.bmHeight, image.bits(), &bmi, DIB_RGB_COLORS);
                ReleaseDC(NULL, hdc);
                
                DeleteObject(iconInfo.hbmColor);
                DeleteObject(iconInfo.hbmMask);
                DestroyIcon(sfi.hIcon);
                
                return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            DeleteObject(iconInfo.hbmColor);
            DeleteObject(iconInfo.hbmMask);
        }
        DestroyIcon(sfi.hIcon);
    }

    return getGenericIcon(path, requestedSize);
}
#endif

QImage IconProvider::getGenericIcon(const QString &path, const QSize &requestedSize)
{
    QFileInfo info(path);
    QIcon icon;
    
    if (info.isDir()) {
        icon = QIcon::fromTheme("folder");
    } else {
        icon = QIcon::fromTheme("text-x-generic");
    }
    
    if (icon.isNull()) {
        // Fallback to internal simple icon if theme failed
        QImage img(requestedSize, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(info.isDir() ? Qt::blue : Qt::gray);
        p.drawRect(2, 2, requestedSize.width() - 4, requestedSize.height() - 4);
        return img;
    }
    
    return icon.pixmap(requestedSize).toImage();
}
