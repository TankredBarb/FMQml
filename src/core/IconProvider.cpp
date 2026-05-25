#include "IconProvider.h"
#include "ArchiveSupport.h"
#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QDir>
#include <QSet>
#include <QDebug>
#include <QUrl>

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
    QString path = QUrl::fromPercentEncoding(id.toUtf8());
    // QML image providers hand us a URL path; archive separators can be percent-encoded.
    
    bool forceDirectory = false;
    if (path.endsWith(QStringLiteral("?directory=true"), Qt::CaseInsensitive)) {
        forceDirectory = true;
        path.chop(15); // Strip "?directory=true"
    }

    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(32, 32);
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

    QImage icon = getIcon(path, targetSize, forceDirectory);
    
    {
        QMutexLocker locker(&m_mutex);
        if (!m_cache.contains(cacheKey)) {
            m_cache.insert(cacheKey, new QImage(icon));
        }
    }
    
    return icon;
}

QImage IconProvider::getIcon(const QString &path, const QSize &requestedSize, bool forceDirectory)
{
#ifdef Q_OS_WIN
    if (forceDirectory) {
        return getWindowsStockFolderIcon(requestedSize);
    }

    if (ArchiveSupport::isArchivePath(path)) {
        const QString archiveName = ArchiveSupport::archiveFileName(path);
        const bool archiveDir = forceDirectory || path.endsWith(QStringLiteral("|/")) || archiveName.isEmpty();
        if (archiveDir) {
            return getWindowsStockFolderIcon(requestedSize);
        }

        const QString suffix = QFileInfo(archiveName).suffix().toLower();
        if (!suffix.isEmpty()) {
            const QString fakeName = QDir::toNativeSeparators(
                QDir::temp().filePath(QStringLiteral("file.") + suffix));
            return getWindowsIcon(fakeName, requestedSize, false);
        }

        return getGenericIcon(path, requestedSize, forceDirectory);
    }
    return getWindowsIcon(path, requestedSize, forceDirectory);
#else
    return getGenericIcon(path, requestedSize, forceDirectory);
#endif
}

#ifdef Q_OS_WIN
QImage IconProvider::getWindowsStockFolderIcon(const QSize &requestedSize)
{
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
            return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    const QString fakeFolder = QDir::toNativeSeparators(QDir::tempPath());
    return getWindowsIcon(fakeFolder, requestedSize, true);
}

QImage IconProvider::getWindowsIcon(const QString &path, const QSize &requestedSize, bool forceDirectory)
{
    SHFILEINFO sfi;
    std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    
    UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON;
    if (qMax(requestedSize.width(), requestedSize.height()) > 32) {
        flags &= ~SHGFI_SMALLICON;
        flags |= SHGFI_LARGEICON;
    }

    const DWORD attr = forceDirectory || QFileInfo(path).isDir()
        ? FILE_ATTRIBUTE_DIRECTORY
        : FILE_ATTRIBUTE_NORMAL;

    if (SHGetFileInfo(wpath.c_str(), attr, &sfi, sizeof(sfi), flags) && sfi.hIcon) {
        QImage image = QImage::fromHICON(sfi.hIcon);
        DestroyIcon(sfi.hIcon);
        if (!image.isNull()) {
            return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
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
