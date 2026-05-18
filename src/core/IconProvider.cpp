#include "IconProvider.h"
#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QDir>
#include <QSet>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <commoncontrols.h>
#endif

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_cache(2000) // Cache 2000 icons
{
}

IconProvider::~IconProvider() = default;

namespace {
bool isPathSpecificIcon(const QFileInfo &fi)
{
    // Only .exe and .lnk truly need per-file icons (they can have custom icons).
    // .dll, .sys, .msi, .bat, .cmd, .ps1 all share a common icon per suffix,
    // so caching by suffix avoids thousands of unique cache misses in dirs like WinSxS.
    static const QSet<QString> perPathExtensions = {
        QStringLiteral("exe"),
        QStringLiteral("lnk"),
    };

    return !fi.isDir() && perPathExtensions.contains(fi.suffix().toLower());
}
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QString path = id;
    // id might contain size info if needed, e.g. "path?size=32"
    // For now, just the path.
    
    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(32, 32);
    if (size) {
        *size = targetSize;
    }

    QFileInfo fi(path);
    QString suffix = fi.suffix().toLower();
    QString cacheKey;
    if (fi.isDir()) {
        cacheKey = QStringLiteral("_dir_");
    } else if (isPathSpecificIcon(fi)) {
        cacheKey = path;
    } else if (suffix.isEmpty()) {
        cacheKey = QStringLiteral("_noext_");
    } else {
        cacheKey = QStringLiteral(".").append(suffix);
    }
    cacheKey += QString::number(targetSize.width()) + QStringLiteral("x") + QString::number(targetSize.height());

    {
        QMutexLocker locker(&m_mutex);
        if (m_cache.contains(cacheKey)) {
            return *m_cache.object(cacheKey);
        }
    }

    QImage icon = getIcon(path, targetSize);
    
    {
        QMutexLocker locker(&m_mutex);
        if (!m_cache.contains(cacheKey)) {
            m_cache.insert(cacheKey, new QImage(icon));
        }
    }
    
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
namespace {
int imageListTypeForSize(const QSize &requestedSize)
{
    const int edge = qMax(requestedSize.width(), requestedSize.height());
    if (edge <= 16) {
        return SHIL_SMALL;
    }
    if (edge <= 32) {
        return SHIL_LARGE;
    }
    if (edge <= 64) {
        return SHIL_EXTRALARGE;
    }
    return SHIL_JUMBO;
}
}

QImage IconProvider::getWindowsIcon(const QString &path, const QSize &requestedSize)
{
    SHFILEINFO sfi;
    std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    
    UINT flags = SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES;
    DWORD attr = QFileInfo(path).isDir() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

    if (SHGetFileInfo(wpath.c_str(), attr, &sfi, sizeof(sfi), flags)) {
        IImageList *imageList = nullptr;
        const int sizeType = imageListTypeForSize(requestedSize);
        if (SUCCEEDED(SHGetImageList(sizeType, IID_IImageList, reinterpret_cast<void **>(&imageList))) && imageList) {
            HICON hIcon = nullptr;
            QImage image;
            if (SUCCEEDED(imageList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon)) && hIcon) {
                image = QImage::fromHICON(hIcon);
                DestroyIcon(hIcon);
            }
            imageList->Release();
            if (!image.isNull()) {
                return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
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
