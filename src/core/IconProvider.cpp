#include "IconProvider.h"
#include "ArchiveSupport.h"
#include "FileTypeIconResolver.h"
#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QDir>
#include <QElapsedTimer>
#include <QSet>
#include <QDebug>
#include <QSettings>
#include <QSvgRenderer>
#include <QUrl>
#include <QStringList>
#include <QRect>

#ifndef Q_OS_WIN
#include <QMimeDatabase>
#include <QMimeType>
#endif

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

bool canExtractEmbeddedIcon(const QFileInfo &fi)
{
    return !fi.isDir() && fi.suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0;
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

bool imageHasVisibleCorners(const QImage &image)
{
    if (image.isNull() || !image.hasAlphaChannel()) {
        return false;
    }

    const QImage argb = image.format() == QImage::Format_ARGB32
        ? image
        : image.convertToFormat(QImage::Format_ARGB32);
    const int right = argb.width() - 1;
    const int bottom = argb.height() - 1;

    return qAlpha(argb.pixel(0, 0)) > 0
        || qAlpha(argb.pixel(right, 0)) > 0
        || qAlpha(argb.pixel(0, bottom)) > 0
        || qAlpha(argb.pixel(right, bottom)) > 0;
}

QRect imageAlphaBounds(const QImage &image)
{
    if (image.isNull() || !image.hasAlphaChannel()) {
        return {};
    }

    const QImage argb = image.format() == QImage::Format_ARGB32
        ? image
        : image.convertToFormat(QImage::Format_ARGB32);

    int minX = argb.width();
    int minY = argb.height();
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < argb.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x) {
            if (qAlpha(line[x]) > 8) {
                minX = qMin(minX, x);
                minY = qMin(minY, y);
                maxX = qMax(maxX, x);
                maxY = qMax(maxY, y);
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return {};
    }
    return QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}

bool imageLooksTinyInCanvas(const QImage &image)
{
    const QRect bounds = imageAlphaBounds(image);
    if (bounds.isNull()) {
        return false;
    }

    return bounds.width() < image.width() / 2
        || bounds.height() < image.height() / 2;
}

bool imageLooksLikeOpaquePlaceholder(const QImage &image)
{
    if (image.isNull() || image.width() < 8 || image.height() < 8 || !image.hasAlphaChannel()) {
        return false;
    }

    const QImage argb = image.format() == QImage::Format_ARGB32
        ? image
        : image.convertToFormat(QImage::Format_ARGB32);
    const int width = argb.width();
    const int height = argb.height();
    const int total = width * height;
    int visible = 0;
    int opaque = 0;
    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;
    bool colorBuckets[4096] = {};
    int uniqueBuckets = 0;

    for (int y = 0; y < height; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            const int alpha = qAlpha(line[x]);
            if (alpha <= 8) {
                continue;
            }

            ++visible;
            if (alpha >= 240) {
                ++opaque;
            }
            minX = qMin(minX, x);
            minY = qMin(minY, y);
            maxX = qMax(maxX, x);
            maxY = qMax(maxY, y);

            const int bucket = ((qRed(line[x]) >> 4) << 8)
                | ((qGreen(line[x]) >> 4) << 4)
                | (qBlue(line[x]) >> 4);
            if (!colorBuckets[bucket]) {
                colorBuckets[bucket] = true;
                ++uniqueBuckets;
            }
        }
    }

    if (visible == 0 || maxX < minX || maxY < minY) {
        return false;
    }

    const QRect bounds(QPoint(minX, minY), QPoint(maxX, maxY));
    const bool cornersOpaque = qAlpha(argb.pixel(0, 0)) >= 240
        && qAlpha(argb.pixel(width - 1, 0)) >= 240
        && qAlpha(argb.pixel(0, height - 1)) >= 240
        && qAlpha(argb.pixel(width - 1, height - 1)) >= 240;
    const bool fillsCanvas = visible >= total * 92 / 100
        && opaque >= visible * 95 / 100
        && bounds.width() >= width * 90 / 100
        && bounds.height() >= height * 90 / 100;

    return cornersOpaque && fillsCanvas && uniqueBuckets <= 8;
}

QString qrcPathFromUrl(QString source)
{
    if (source.startsWith(QStringLiteral("qrc:/"))) {
        source = source.mid(3);
    }
    return source;
}

QImage renderFallbackIcon(const QString &path, const QSize &requestedSize, bool forceDirectory)
{
    static const FileTypeIconResolver resolver;
    const QString iconSource = resolver.iconForPathHint(path, forceDirectory);
    QSvgRenderer renderer(qrcPathFromUrl(iconSource));
    if (!renderer.isValid()) {
        return {};
    }

    QImage image(requestedSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(requestedSize)));
    return image;
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
    if (icon.isNull()) {
        icon = renderFallbackIcon(path, targetSize, forceDirectory);
    }
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
    if (path.contains(QStringLiteral("://")) && !path.startsWith(QStringLiteral("archive://"), Qt::CaseInsensitive)) {
        return {};
    }
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

QImage getWindowsImageListIcon(const QString &path,
                               const QSize &requestedSize,
                               bool forceDirectory,
                               bool useFileAttributes)
{
    if (path.isEmpty() || !requestedSize.isValid()) {
        return {};
    }

    SHFILEINFO sfi;
    ZeroMemory(&sfi, sizeof(sfi));

    UINT flags = SHGFI_SYSICONINDEX | SHGFI_LARGEICON;
    if (useFileAttributes) {
        flags |= SHGFI_USEFILEATTRIBUTES;
    }

    const DWORD attr = forceDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    if (!SHGetFileInfoW(wpath.c_str(), attr, &sfi, sizeof(sfi), flags)) {
        return {};
    }

    const int imageListSizes[] = {
        SHIL_JUMBO,
        SHIL_EXTRALARGE,
        SHIL_LARGE
    };

    for (const int imageListSize : imageListSizes) {
        IImageList *imageList = nullptr;
        HRESULT hr = SHGetImageList(imageListSize, IID_PPV_ARGS(&imageList));
        if (FAILED(hr) || !imageList) {
            continue;
        }

        HICON hIcon = nullptr;
        hr = imageList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
        imageList->Release();
        if (FAILED(hr) || !hIcon) {
            continue;
        }

        QImage image = QImage::fromHICON(hIcon);
        DestroyIcon(hIcon);
        if (image.isNull() || imageLooksTinyInCanvas(image)) {
            continue;
        }

        return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return {};
}

int windowsSystemIconIndexForPath(const QString &path, bool forceDirectory, bool useFileAttributes)
{
    if (path.isEmpty()) {
        return -1;
    }

    SHFILEINFO sfi;
    ZeroMemory(&sfi, sizeof(sfi));

    UINT flags = SHGFI_SYSICONINDEX;
    if (useFileAttributes) {
        flags |= SHGFI_USEFILEATTRIBUTES;
    }

    const DWORD attr = forceDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    if (!SHGetFileInfoW(wpath.c_str(), attr, &sfi, sizeof(sfi), flags)) {
        return -1;
    }

    return sfi.iIcon;
}

int windowsUnknownFileIconIndex()
{
    static const int index = windowsSystemIconIndexForPath(
        QDir::temp().filePath(QStringLiteral("fm_unknown_file_type.__fm_unknown_assoc__")),
        false,
        true);
    return index;
}

bool shellUsesUnknownFileIcon(const QFileInfo &fileInfo)
{
    if (fileInfo.isDir()) {
        return false;
    }

    const QString suffix = fileInfo.suffix().toLower();
    if (suffix.isEmpty()) {
        return true;
    }

    const int unknownIndex = windowsUnknownFileIconIndex();
    if (unknownIndex < 0) {
        return false;
    }

    const QString fakePath = QDir::temp().filePath(QStringLiteral("fm_file_type.") + suffix);
    const int suffixIndex = windowsSystemIconIndexForPath(fakePath, false, true);
    return suffixIndex >= 0 && suffixIndex == unknownIndex;
}

QImage getWindowsEmbeddedIcon(const QString &path, const QSize &requestedSize)
{
    if (path.isEmpty() || !requestedSize.isValid()) {
        return {};
    }

    HICON hIcon = nullptr;
    UINT iconId = 0;
    const int edge = qMax(requestedSize.width(), requestedSize.height());
    const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    const UINT extracted = PrivateExtractIconsW(
        wpath.c_str(),
        0,
        edge,
        edge,
        &hIcon,
        &iconId,
        1,
        LR_DEFAULTCOLOR);

    if (extracted == UINT_MAX || !hIcon) {
        HICON largeIcon = nullptr;
        HICON smallIcon = nullptr;
        const UINT count = ExtractIconExW(wpath.c_str(), 0, &largeIcon, &smallIcon, 1);
        hIcon = largeIcon ? largeIcon : smallIcon;
        if (count == 0 || !hIcon) {
            return {};
        }

        QImage extractedImage = QImage::fromHICON(hIcon);
        if (largeIcon) {
            DestroyIcon(largeIcon);
        }
        if (smallIcon) {
            DestroyIcon(smallIcon);
        }
        return extractedImage;
    }

    QImage image = QImage::fromHICON(hIcon);
    DestroyIcon(hIcon);
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

    const QFileInfo fileInfo(path);
    const bool pathSpecificIcon = !forceDirectory && !genericOnly && isPathSpecificIcon(fileInfo);
    const bool embeddedIconCandidate = !forceDirectory && !genericOnly && canExtractEmbeddedIcon(fileInfo);
    const bool imageListIconCandidate = pathSpecificIcon && highQualitySystemIconsEnabled();
    const bool unknownShellFileIcon = !forceDirectory && !pathSpecificIcon && shellUsesUnknownFileIcon(fileInfo);

    if (unknownShellFileIcon) {
        if (iconTimingEnabled()) {
            qInfo().noquote()
                << "[IconProvider] shell-file-no-association-fallback"
                << "ms=" << timer.elapsed()
                << "suffix=" << fileInfo.suffix().toLower()
                << "generic=" << genericOnly
                << "path=" << path;
        }
        return {};
    }

    if (imageListIconCandidate) {
        const QImage imageListImage = getWindowsImageListIcon(path, requestedSize, false, false);
        if (!imageListImage.isNull()) {
            if (iconTimingEnabled()) {
                qInfo().noquote()
                    << "[IconProvider] shell-file-imagelist"
                    << "ms=" << timer.elapsed()
                    << "size=" << QStringLiteral("%1x%2").arg(requestedSize.width()).arg(requestedSize.height())
                    << "path=" << path;
            }
            return imageListImage;
        }
    }

    if (highQualitySystemIcons && !genericOnly) {
        const QString effectivePath = forceDirectory ? QDir::toNativeSeparators(QDir::tempPath()) : path;
        const QImage highQualityImage = getWindowsHighQualityIcon(effectivePath, requestedSize);
        if (!highQualityImage.isNull()) {
            if (!forceDirectory && !pathSpecificIcon && imageLooksLikeOpaquePlaceholder(highQualityImage)) {
                if (iconTimingEnabled()) {
                    qInfo().noquote()
                        << "[IconProvider] shell-file-hq-placeholder-fallback"
                        << "ms=" << timer.elapsed()
                        << "size=" << QStringLiteral("%1x%2").arg(requestedSize.width()).arg(requestedSize.height())
                        << "generic=" << genericOnly
                        << "dir=" << forceDirectory
                        << "path=" << effectivePath;
                }
            } else if (!forceDirectory && imageHasVisibleCorners(highQualityImage)) {
                if (iconTimingEnabled()) {
                    qInfo().noquote()
                        << "[IconProvider] shell-file-hq-corner-fallback"
                        << "ms=" << timer.elapsed()
                        << "size=" << QStringLiteral("%1x%2").arg(requestedSize.width()).arg(requestedSize.height())
                        << "generic=" << genericOnly
                        << "dir=" << forceDirectory
                        << "path=" << effectivePath;
                }
            } else {
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
    }

    if (embeddedIconCandidate) {
        const QImage embeddedImage = getWindowsEmbeddedIcon(path, requestedSize);
        if (!embeddedImage.isNull()) {
            if (iconTimingEnabled()) {
                qInfo().noquote()
                    << "[IconProvider] shell-file-embedded"
                    << "ms=" << timer.elapsed()
                    << "size=" << QStringLiteral("%1x%2").arg(requestedSize.width()).arg(requestedSize.height())
                    << "path=" << path;
            }
            return embeddedImage.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    SHFILEINFO sfi;
    std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    
    UINT flags = SHGFI_ICON | SHGFI_SMALLICON;
    if (!pathSpecificIcon) {
        flags |= SHGFI_USEFILEATTRIBUTES;
    }
    if (qMax(requestedSize.width(), requestedSize.height()) > 32) {
        flags &= ~SHGFI_SMALLICON;
        flags |= SHGFI_LARGEICON;
    }

    const DWORD attr = forceDirectory || (!genericOnly && fileInfo.isDir())
        ? FILE_ATTRIBUTE_DIRECTORY
        : FILE_ATTRIBUTE_NORMAL;

    if (SHGetFileInfo(wpath.c_str(), attr, &sfi, sizeof(sfi), flags) && sfi.hIcon) {
        QImage image = QImage::fromHICON(sfi.hIcon);
        DestroyIcon(sfi.hIcon);
        if (!image.isNull()) {
            if (!forceDirectory && !pathSpecificIcon && imageLooksLikeOpaquePlaceholder(image)) {
                if (iconTimingEnabled()) {
                    qInfo().noquote()
                        << "[IconProvider] shell-file-placeholder-fallback"
                        << "ms=" << timer.elapsed()
                        << "size=" << QStringLiteral("%1x%2").arg(requestedSize.width()).arg(requestedSize.height())
                        << "generic=" << genericOnly
                        << "dir=" << forceDirectory
                        << "path=" << path;
                }
            } else {
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

    if (forceDirectory) {
        return getGenericIcon(path, requestedSize, true);
    }
    return {};
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
        icon = QIcon::fromTheme(QStringLiteral("folder"));
    } else if (archiveFile && !suffix.isEmpty() && ArchiveSupport::isArchiveExtension(suffix)) {
        icon = QIcon::fromTheme(QStringLiteral("package-x-generic"));
    } else {
#ifdef Q_OS_WIN
        icon = QIcon::fromTheme(QStringLiteral("text-x-generic"));
#else
        QMimeDatabase db;
        QMimeType mime;
        if (info.exists()) {
            mime = db.mimeTypeForFile(info);
        } else {
            mime = db.mimeTypeForFile(info.fileName(), QMimeDatabase::MatchExtension);
        }
        
        const QString iconName = mime.iconName();
        icon = QIcon::fromTheme(iconName);
        
        if (icon.isNull()) {
            const QString genericName = mime.genericIconName();
            if (!genericName.isEmpty()) {
                icon = QIcon::fromTheme(genericName);
            }
        }
        
        if (icon.isNull()) {
            icon = QIcon::fromTheme(QStringLiteral("text-x-generic"));
        }
#endif
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
