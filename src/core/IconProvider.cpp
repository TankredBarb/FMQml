#include "IconProvider.h"
#include "ArchiveSupport.h"
#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QDir>
#include <QElapsedTimer>
#include <QSet>
#include <QDebug>
#include <QSettings>
#include <QUrl>
#include <QStringList>

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
bool iconTimingEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_ICON_TIMING");
    return enabled;
}

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

bool highQualitySystemIconsEnabled()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("appearance"));
    const bool enabled = settings.value(QStringLiteral("useHighQualitySystemIcons"), true).toBool();
    settings.endGroup();
    return enabled;
}

bool shouldUseHighQualitySystemIcons(const QSize &requestedSize)
{
    if (!highQualitySystemIconsEnabled()) {
        return false;
    }

    return qMax(requestedSize.width(), requestedSize.height()) > 32;
}

#ifdef Q_OS_WIN
QImage imageFromHBitmap(HBITMAP hBmp)
{
    if (!hBmp) {
        return {};
    }

    BITMAP bmp;
    if (!GetObject(hBmp, sizeof(BITMAP), &bmp) || bmp.bmWidth <= 0 || bmp.bmHeight <= 0) {
        return {};
    }

    QImage image(bmp.bmWidth, bmp.bmHeight, QImage::Format_ARGB32_Premultiplied);
    HDC hdc = GetDC(nullptr);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.bmWidth;
    bmi.bmiHeader.biHeight = -bmp.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    if (!GetDIBits(hdc, hBmp, 0, bmp.bmHeight, image.bits(), &bmi, DIB_RGB_COLORS)) {
        ReleaseDC(nullptr, hdc);
        return {};
    }

    ReleaseDC(nullptr, hdc);
    return image;
}
#endif
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QElapsedTimer totalTimer;
    if (iconTimingEnabled()) {
        totalTimer.start();
    }

    QString path = QUrl::fromPercentEncoding(id.toUtf8());
    // QML image providers hand us a URL path; archive separators can be percent-encoded.
    
    bool forceDirectory = false;
    bool genericOnly = false;
    const int queryStart = path.indexOf(QLatin1Char('?'));
    if (queryStart >= 0) {
        const QString query = path.mid(queryStart + 1);
        path.truncate(queryStart);

        const QStringList parts = query.split(QLatin1Char('&'), Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            const QString key = part.section(QLatin1Char('='), 0, 0).toLower();
            const QString value = part.section(QLatin1Char('='), 1).toLower();
            if (key == QLatin1String("directory") && value == QLatin1String("true")) {
                forceDirectory = true;
            } else if (key == QLatin1String("generic") && value == QLatin1String("true")) {
                genericOnly = true;
            }
        }
    }

    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(32, 32);
    const bool highQualitySystemIcons = shouldUseHighQualitySystemIcons(targetSize);
    if (size) {
        *size = targetSize;
    }

    QFileInfo fi(path);
    QString suffix = fi.suffix().toLower();
    const bool archivePath = ArchiveSupport::isArchivePath(path);
    if (archivePath) {
        const QString archiveName = ArchiveSupport::archiveFileName(path);
        suffix = QFileInfo(archiveName).suffix().toLower();
    }
    QString cacheKey;
    if (forceDirectory || fi.isDir() || (archivePath && path.endsWith(QStringLiteral("|/")))) {
        cacheKey = QStringLiteral("_dir_");
    } else if (archivePath) {
        cacheKey = QStringLiteral("_archive_.") + suffix;
    } else if (!genericOnly && isPathSpecificIcon(fi)) {
        cacheKey = path;
    } else if (suffix.isEmpty()) {
        cacheKey = QStringLiteral("_noext_");
    } else {
        cacheKey = QStringLiteral(".").append(suffix);
    }
    cacheKey += QString::number(targetSize.width()) + QStringLiteral("x") + QString::number(targetSize.height());
    cacheKey += highQualitySystemIcons ? QStringLiteral("|hq") : QStringLiteral("|std");

    {
        QMutexLocker locker(&m_mutex);
        if (m_cache.contains(cacheKey)) {
            if (iconTimingEnabled()) {
                qInfo().noquote()
                    << "[IconProvider] hit"
                    << "ms=" << totalTimer.elapsed()
                    << "size=" << QStringLiteral("%1x%2").arg(targetSize.width()).arg(targetSize.height())
                    << "key=" << cacheKey
                    << "path=" << path;
            }
            return *m_cache.object(cacheKey);
        }
    }

    QElapsedTimer loadTimer;
    if (iconTimingEnabled()) {
        loadTimer.start();
    }
    QImage icon = getIcon(path, targetSize, forceDirectory, genericOnly, highQualitySystemIcons);
    const qint64 loadMs = iconTimingEnabled() ? loadTimer.elapsed() : 0;
    
    {
        QMutexLocker locker(&m_mutex);
        if (!m_cache.contains(cacheKey)) {
            m_cache.insert(cacheKey, new QImage(icon));
        }
    }

    if (iconTimingEnabled()) {
        qInfo().noquote()
            << "[IconProvider] miss"
            << "totalMs=" << totalTimer.elapsed()
            << "loadMs=" << loadMs
            << "size=" << QStringLiteral("%1x%2").arg(targetSize.width()).arg(targetSize.height())
            << "generic=" << genericOnly
            << "dir=" << forceDirectory
            << "key=" << cacheKey
            << "null=" << icon.isNull()
            << "path=" << path;
    }
    
    return icon;
}

QImage IconProvider::getIcon(const QString &path,
                             const QSize &requestedSize,
                             bool forceDirectory,
                             bool genericOnly,
                             bool highQualitySystemIcons)
{
#ifdef Q_OS_WIN
    if (forceDirectory) {
        return getWindowsStockFolderIcon(requestedSize, highQualitySystemIcons);
    }

    if (ArchiveSupport::isArchivePath(path)) {
        const QString archiveName = ArchiveSupport::archiveFileName(path);
        const bool archiveDir = forceDirectory || path.endsWith(QStringLiteral("|/")) || archiveName.isEmpty();
        if (archiveDir) {
            return getWindowsStockFolderIcon(requestedSize, highQualitySystemIcons);
        }

        const QString suffix = QFileInfo(archiveName).suffix().toLower();
        if (!suffix.isEmpty()) {
            const QString fakeName = QDir::toNativeSeparators(
                QDir::temp().filePath(QStringLiteral("file.") + suffix));
            return getWindowsIcon(fakeName, requestedSize, false, true, false);
        }

        return getGenericIcon(path, requestedSize, forceDirectory);
    }
    return getWindowsIcon(path, requestedSize, forceDirectory, genericOnly, highQualitySystemIcons);
#else
    return getGenericIcon(path, requestedSize, forceDirectory);
#endif
}

#ifdef Q_OS_WIN
QImage IconProvider::getWindowsStockFolderIcon(const QSize &requestedSize, bool highQualitySystemIcons)
{
    if (highQualitySystemIcons) {
        const QImage highQualityFolder = getWindowsHighQualityIcon(QDir::toNativeSeparators(QDir::tempPath()), requestedSize);
        if (!highQualityFolder.isNull()) {
            return highQualityFolder.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    QElapsedTimer timer;
    if (iconTimingEnabled()) {
        timer.start();
    }

    SHSTOCKICONINFO sii;
    ZeroMemory(&sii, sizeof(sii));
    sii.cbSize = sizeof(sii);

    UINT flags = SHGSI_ICON | SHGSI_SMALLICON;
    if (qMax(requestedSize.width(), requestedSize.height()) > 32) {
        flags &= ~SHGSI_SMALLICON;
        flags |= SHGSI_LARGEICON;
    }

    if (SUCCEEDED(SHGetStockIconInfo(SIID_FOLDER, flags, &sii)) && sii.hIcon) {
        QImage image = QImage::fromHICON(sii.hIcon);
        DestroyIcon(sii.hIcon);
        if (!image.isNull()) {
            if (iconTimingEnabled()) {
                qInfo().noquote()
                    << "[IconProvider] shell-stock-folder"
                    << "ms=" << timer.elapsed()
                    << "size=" << QStringLiteral("%1x%2").arg(requestedSize.width()).arg(requestedSize.height());
            }
            return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    const QString fakeFolder = QDir::toNativeSeparators(QDir::tempPath());
    return getWindowsIcon(fakeFolder, requestedSize, true, true, false);
}

QImage IconProvider::getWindowsHighQualityIcon(const QString &path, const QSize &requestedSize)
{
    if (path.isEmpty()) {
        return {};
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool needUninit = (hr == S_OK || hr == S_FALSE);

    IShellItem *item = nullptr;
    const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    hr = SHCreateItemFromParsingName(wpath.c_str(), nullptr, IID_PPV_ARGS(&item));
    if (FAILED(hr) || !item) {
        if (needUninit) {
            CoUninitialize();
        }
        return {};
    }

    IShellItemImageFactory *factory = nullptr;
    hr = item->QueryInterface(IID_PPV_ARGS(&factory));
    item->Release();
    if (FAILED(hr) || !factory) {
        if (needUninit) {
            CoUninitialize();
        }
        return {};
    }

    HBITMAP hBmp = nullptr;
    const int edge = qMax(requestedSize.width(), requestedSize.height());
    const SIZE size = { static_cast<LONG>(edge), static_cast<LONG>(edge) };
    hr = factory->GetImage(size, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK | SIIGBF_RESIZETOFIT, &hBmp);
    factory->Release();

    QImage image;
    if (SUCCEEDED(hr) && hBmp) {
        image = imageFromHBitmap(hBmp);
        DeleteObject(hBmp);
    }

    if (needUninit) {
        CoUninitialize();
    }

    return image;
}

QImage IconProvider::getWindowsIcon(const QString &path,
                                    const QSize &requestedSize,
                                    bool forceDirectory,
                                    bool genericOnly,
                                    bool highQualitySystemIcons)
{
    QElapsedTimer timer;
    if (iconTimingEnabled()) {
        timer.start();
    }

    if (highQualitySystemIcons && !genericOnly) {
        const QString effectivePath = forceDirectory ? QDir::toNativeSeparators(QDir::tempPath()) : path;
        const QImage highQualityImage = getWindowsHighQualityIcon(effectivePath, requestedSize);
        if (!highQualityImage.isNull()) {
            if (iconTimingEnabled()) {
                qInfo().noquote()
                    << "[IconProvider] shell-file-hq"
                    << "ms=" << timer.elapsed()
                    << "size=" << QStringLiteral("%1x%2").arg(requestedSize.width()).arg(requestedSize.height())
                    << "generic=" << genericOnly
                    << "dir=" << forceDirectory
                    << "path=" << effectivePath;
            }
            return highQualityImage.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    SHFILEINFO sfi;
    std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    
    UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON;
    if (qMax(requestedSize.width(), requestedSize.height()) > 32) {
        flags &= ~SHGFI_SMALLICON;
        flags |= SHGFI_LARGEICON;
    }

    const DWORD attr = forceDirectory || (!genericOnly && QFileInfo(path).isDir())
        ? FILE_ATTRIBUTE_DIRECTORY
        : FILE_ATTRIBUTE_NORMAL;

    if (SHGetFileInfo(wpath.c_str(), attr, &sfi, sizeof(sfi), flags) && sfi.hIcon) {
        QImage image = QImage::fromHICON(sfi.hIcon);
        DestroyIcon(sfi.hIcon);
        if (!image.isNull()) {
            if (iconTimingEnabled()) {
                qInfo().noquote()
                    << "[IconProvider] shell-file"
                    << "ms=" << timer.elapsed()
                    << "size=" << QStringLiteral("%1x%2").arg(requestedSize.width()).arg(requestedSize.height())
                    << "generic=" << genericOnly
                    << "dir=" << forceDirectory
                    << "path=" << path;
            }
            return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    if (iconTimingEnabled()) {
        qInfo().noquote()
            << "[IconProvider] shell-file-fallback"
            << "ms=" << timer.elapsed()
            << "size=" << QStringLiteral("%1x%2").arg(requestedSize.width()).arg(requestedSize.height())
            << "generic=" << genericOnly
            << "dir=" << forceDirectory
            << "path=" << path;
    }

    return getGenericIcon(path, requestedSize, forceDirectory);
}
#endif

QImage IconProvider::getGenericIcon(const QString &path, const QSize &requestedSize, bool forceDirectory)
{
    QFileInfo info(path);
    QIcon icon;

    const bool archivePath = ArchiveSupport::isArchivePath(path);
    const QString archiveName = archivePath ? ArchiveSupport::archiveFileName(path) : QString();
    const QString suffix = archivePath ? QFileInfo(archiveName).suffix().toLower() : info.suffix().toLower();
    const bool archiveDir = forceDirectory || (archivePath && (path.endsWith(QStringLiteral("|/")) || archiveName.isEmpty()));
    const bool archiveFile = archivePath && !archiveDir;

    if (info.isDir() || archiveDir) {
        icon = QIcon::fromTheme("folder");
    } else if (archiveFile && !suffix.isEmpty() && ArchiveSupport::isArchiveExtension(suffix)) {
        icon = QIcon::fromTheme("package-x-generic");
    } else {
        icon = QIcon::fromTheme("text-x-generic");
    }
    
    if (icon.isNull()) {
        // Fallback to internal simple icon if theme failed
        QImage img(requestedSize, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush((info.isDir() || archiveDir) ? Qt::blue : Qt::gray);
        p.drawRect(2, 2, requestedSize.width() - 4, requestedSize.height() - 4);
        return img;
    }
    
    return icon.pixmap(requestedSize).toImage();
}
