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
    bool isArchiveInternal = path.startsWith(QLatin1String("archive://"));
    QString diskPath;
    QString internalPath;
    
    if (isArchiveInternal) {
        QString stripped = path.mid(10);
        int pipeIdx = stripped.indexOf(QLatin1Char('|'));
        if (pipeIdx != -1) {
            diskPath = stripped.left(pipeIdx);
            internalPath = stripped.mid(pipeIdx + 1);
        } else {
            diskPath = stripped;
            internalPath = QLatin1String("/");
        }
    }

    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(32, 32);
    if (size) {
        *size = targetSize;
    }

    QFileInfo fi(isArchiveInternal ? internalPath : path);
    QString suffix = fi.suffix().toLower();
    QString cacheKey;
    
    // For archive internals, we don't have full QFileInfo features, 
    // so we handle it slightly differently.
    if (isArchiveInternal) {
        if (internalPath == QLatin1String("/") || internalPath.isEmpty()) {
             // Root of archive: show the icon of the archive file itself
             return requestImage(diskPath, size, requestedSize);
        }
        // Inside archive: distinguish dir/file by path (HACK: assume no suffix means dir or use a better way)
        // Actually, ArchiveFileProvider doesn't give us isDir here easily.
        // We'll use a simple rule: if it's "archive://" and not root, we'll try to guess.
        // Better: DirectoryModel passes path. We can check if it ends with / or has no extension.
        bool likelyDir = internalPath.endsWith(QLatin1Char('/')) || !internalPath.contains(QLatin1Char('.'));
        if (likelyDir) {
            cacheKey = QStringLiteral("_dir_");
        } else {
            cacheKey = QStringLiteral(".").append(suffix);
        }
    } else if (fi.isDir()) {
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

    QImage icon;
    if (isArchiveInternal) {
        if (internalPath == QLatin1String("/") || internalPath.isEmpty()) {
             return requestImage(diskPath, size, requestedSize);
        }
        
        // Use the cacheKey logic to determine if it's a directory
        bool likelyDir = internalPath.endsWith(QLatin1Char('/')) || !internalPath.contains(QLatin1Char('.'));
        
#ifdef Q_OS_WIN
        SHFILEINFO sfi;
        std::wstring wpath = QDir::toNativeSeparators(internalPath).toStdWString();
        UINT flags = SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES;
        DWORD attr = likelyDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

        if (SHGetFileInfo(wpath.c_str(), attr, &sfi, sizeof(sfi), flags)) {
            IImageList *imageList = nullptr;
            const int sizeType = imageListTypeForSize(requestedSize);
            if (SUCCEEDED(SHGetImageList(sizeType, IID_IImageList, reinterpret_cast<void **>(&imageList))) && imageList) {
                HICON hIcon = nullptr;
                if (SUCCEEDED(imageList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon)) && hIcon) {
                    icon = QImage::fromHICON(hIcon);
                    DestroyIcon(hIcon);
                }
                imageList->Release();
            }
        }
        if (!icon.isNull()) {
            icon = icon.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } else {
            icon = getGenericIcon(internalPath, requestedSize);
        }
#else
        icon = getGenericIcon(internalPath, requestedSize);
#endif
    } else {
        icon = getIcon(path, requestedSize);
    }
    
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
