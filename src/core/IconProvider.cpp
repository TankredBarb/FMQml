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
#include <QStandardPaths>

#ifndef Q_OS_WIN
#include <QFile>
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

#ifndef Q_OS_WIN
QString linuxIconThemeFromSettingsFile(const QString &path, const QString &group, const QString &key)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return {};
    }

    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(group);
    const QString value = settings.value(key).toString().trimmed();
    settings.endGroup();
    return value;
}

QString linuxConfiguredIconThemeName()
{
    const QString envTheme = QString::fromLocal8Bit(qgetenv("QT_ICON_THEME")).trimmed();
    if (!envTheme.isEmpty()) {
        return envTheme;
    }

    const QString configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString kdeTheme = linuxIconThemeFromSettingsFile(
        QDir(configHome).filePath(QStringLiteral("kdeglobals")),
        QStringLiteral("Icons"),
        QStringLiteral("Theme"));
    if (!kdeTheme.isEmpty()) {
        return kdeTheme;
    }

    const QString kdeDefaultTheme = linuxIconThemeFromSettingsFile(
        QDir(configHome).filePath(QStringLiteral("kdedefaults/kdeglobals")),
        QStringLiteral("Icons"),
        QStringLiteral("Theme"));
    if (!kdeDefaultTheme.isEmpty()) {
        return kdeDefaultTheme;
    }

    const QString gtk4Theme = linuxIconThemeFromSettingsFile(
        QDir(configHome).filePath(QStringLiteral("gtk-4.0/settings.ini")),
        QStringLiteral("Settings"),
        QStringLiteral("gtk-icon-theme-name"));
    if (!gtk4Theme.isEmpty()) {
        return gtk4Theme;
    }

    return linuxIconThemeFromSettingsFile(
        QDir(configHome).filePath(QStringLiteral("gtk-3.0/settings.ini")),
        QStringLiteral("Settings"),
        QStringLiteral("gtk-icon-theme-name"));
}

void configureLinuxIconTheme()
{
    QStringList searchPaths = QIcon::themeSearchPaths();
    const QString home = QDir::homePath();
    const QStringList extraPaths = {
        QDir(home).filePath(QStringLiteral(".local/share/icons")),
        QDir(home).filePath(QStringLiteral(".icons")),
        QStringLiteral("/usr/local/share/icons"),
        QStringLiteral("/usr/share/icons"),
    };
    for (const QString &path : extraPaths) {
        if (QFileInfo(path).isDir() && !searchPaths.contains(path)) {
            searchPaths.append(path);
        }
    }
    QIcon::setThemeSearchPaths(searchPaths);
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    const QString themeName = linuxConfiguredIconThemeName();
    if (!themeName.isEmpty() && QIcon::themeName() != themeName) {
        QIcon::setThemeName(themeName);
    }

    if (qEnvironmentVariableIsSet("FM_ICON_TIMING")) {
        qInfo().noquote()
            << "[IconProvider] linux-icon-theme"
            << "theme=" << QIcon::themeName()
            << "fallback=" << QIcon::fallbackThemeName()
            << "paths=" << QIcon::themeSearchPaths().join(QLatin1Char(';'));
    }
}
#endif

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_cache(2000) // Cache 2000 icons
{
#ifndef Q_OS_WIN
    configureLinuxIconTheme();
#endif
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
#ifndef Q_OS_WIN
    if (fi.isDir()) {
        return false;
    }
    const QString suffix = fi.suffix().toLower();
    return suffix == QLatin1String("desktop") || suffix.isEmpty();
#endif

    // Only .exe and .lnk truly need per-file icons (they can have custom icons).
    // .dll, .sys, .msi, .bat, .cmd, .ps1 all share a common icon per suffix,
    // so caching by suffix avoids thousands of unique cache misses in dirs like WinSxS.
    static const QSet<QString> perPathExtensions = {
        QStringLiteral("exe"),
        QStringLiteral("lnk"),
    };

    return !fi.isDir() && perPathExtensions.contains(fi.suffix().toLower());
}

#ifndef Q_OS_WIN
QString cleanIconPath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QString standardLocationPath(QStandardPaths::StandardLocation location)
{
    const QString path = location == QStandardPaths::HomeLocation
        ? QDir::homePath()
        : QStandardPaths::writableLocation(location);
    return path.isEmpty() ? QString() : cleanIconPath(path);
}

bool sameIconPath(const QString &left, const QString &right)
{
    return !left.isEmpty() && !right.isEmpty() && cleanIconPath(left) == cleanIconPath(right);
}

QStringList linuxThemeIconCandidatesForSpecialFolder(const QString &path)
{
    const QString cleanPath = cleanIconPath(path);
    if (cleanPath.isEmpty()) {
        return {};
    }

    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::HomeLocation))) {
        return {QStringLiteral("user-home"), QStringLiteral("folder-home"), QStringLiteral("folder")};
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::DesktopLocation))) {
        return {QStringLiteral("user-desktop"), QStringLiteral("folder-desktop"), QStringLiteral("folder")};
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::DownloadLocation))) {
        return {QStringLiteral("folder-download"), QStringLiteral("folder-downloads"), QStringLiteral("folder")};
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::DocumentsLocation))) {
        return {QStringLiteral("folder-documents"), QStringLiteral("folder")};
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::PicturesLocation))) {
        return {QStringLiteral("folder-pictures"), QStringLiteral("folder")};
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::MusicLocation))) {
        return {QStringLiteral("folder-music"), QStringLiteral("folder")};
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::MoviesLocation))) {
        return {QStringLiteral("folder-videos"), QStringLiteral("folder-video"), QStringLiteral("folder")};
    }
    return {};
}

QString linuxSpecialFolderDebugName(const QString &path)
{
    const QString cleanPath = cleanIconPath(path);
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::HomeLocation))) {
        return QStringLiteral("home");
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::DesktopLocation))) {
        return QStringLiteral("desktop");
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::DownloadLocation))) {
        return QStringLiteral("downloads");
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::DocumentsLocation))) {
        return QStringLiteral("documents");
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::PicturesLocation))) {
        return QStringLiteral("pictures");
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::MusicLocation))) {
        return QStringLiteral("music");
    }
    if (sameIconPath(cleanPath, standardLocationPath(QStandardPaths::MoviesLocation))) {
        return QStringLiteral("videos");
    }
    return {};
}

bool isLinuxSpecialFolder(const QFileInfo &fi)
{
    return fi.isDir() && !linuxThemeIconCandidatesForSpecialFolder(fi.absoluteFilePath()).isEmpty();
}

QIcon firstThemeIcon(const QStringList &names, const QString &debugContext = {})
{
    for (const QString &name : names) {
        if (name.isEmpty()) {
            continue;
        }
        QIcon icon = QIcon::fromTheme(name);
        if (iconTimingEnabled()) {
            qInfo().noquote()
                << "[IconProvider] linux-theme-candidate"
                << "context=" << debugContext
                << "name=" << name
                << "ok=" << !icon.isNull()
                << "theme=" << QIcon::themeName();
        }
        if (!icon.isNull()) {
            return icon;
        }
    }
    return {};
}

QString desktopEntryIconName(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    bool inDesktopEntry = false;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            inDesktopEntry = line.compare(QStringLiteral("[Desktop Entry]"), Qt::CaseInsensitive) == 0;
            continue;
        }
        if (!inDesktopEntry || !line.startsWith(QStringLiteral("Icon="), Qt::CaseInsensitive)) {
            continue;
        }
        line.remove(0, 5);
        return line.trimmed();
    }
    return {};
}

QIcon linuxDesktopEntryIcon(const QString &path)
{
    const QString iconName = desktopEntryIconName(path);
    if (iconName.isEmpty()) {
        return {};
    }
    if (iconName.startsWith(QLatin1Char('/'))) {
        return QIcon(iconName);
    }
    return QIcon::fromTheme(iconName);
}

QStringList linuxWindowsExecutableIconCandidates(const QString &suffix)
{
    static const QSet<QString> suffixes = {
        QStringLiteral("exe"), QStringLiteral("dll"), QStringLiteral("msi"),
        QStringLiteral("bat"), QStringLiteral("cmd"), QStringLiteral("ps1"),
        QStringLiteral("com"), QStringLiteral("sys"), QStringLiteral("scr"),
        QStringLiteral("cpl"), QStringLiteral("appx"), QStringLiteral("msix"),
    };
    if (!suffixes.contains(suffix.toLower())) {
        return {};
    }

    return {
        QStringLiteral("application-x-executable"),
        QStringLiteral("application-vnd.microsoft.portable-executable"),
        QStringLiteral("application-x-msdownload"),
        QStringLiteral("application-x-ms-dos-executable"),
    };
}
#endif

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

QString decodedQueryValue(const QString &value)
{
    return QUrl::fromPercentEncoding(value.toUtf8()).trimmed();
}

QString sanitizedSuffix(QString suffix)
{
    suffix = suffix.trimmed().toLower();
    if (suffix.startsWith(QLatin1Char('.'))) {
        suffix.remove(0, 1);
    }
    if (suffix.contains(QLatin1Char('/')) || suffix.contains(QLatin1Char('\\'))) {
        return {};
    }
    return suffix;
}

QString fileNameFromIconHints(const QString &path, const QString &displayName, const QString &suffix)
{
    QString name = displayName.trimmed();
    if (name.isEmpty()) {
        const int slash = path.lastIndexOf(QLatin1Char('/'));
        name = slash >= 0 ? path.mid(slash + 1) : path;
    }

    if (name.isEmpty()) {
        name = QStringLiteral("file");
    }

    const QString cleanSuffix = sanitizedSuffix(suffix);
    if (!cleanSuffix.isEmpty() && QFileInfo(name).suffix().isEmpty()) {
        name += QLatin1Char('.') + cleanSuffix;
    }
    return name;
}

QString providerIconCacheTypeKey(bool forceDirectory, const QString &mimeType, const QString &suffix, const QString &displayName)
{
    if (forceDirectory) {
        return QStringLiteral("_provider_dir_");
    }

    const QString mime = mimeType.trimmed().toLower();
    if (!mime.isEmpty()) {
        return QStringLiteral("_provider_mime_") + mime;
    }

    const QString cleanSuffix = sanitizedSuffix(suffix);
    if (!cleanSuffix.isEmpty()) {
        return QStringLiteral("_provider_ext_.") + cleanSuffix;
    }

    const QString nameSuffix = QFileInfo(displayName.trimmed()).suffix().toLower();
    return nameSuffix.isEmpty()
        ? QStringLiteral("_provider_noext_")
        : QStringLiteral("_provider_name_ext_.") + nameSuffix;
}

#ifndef Q_OS_WIN
QIcon linuxProviderIconForHints(bool forceDirectory,
                                const QString &mimeType,
                                const QString &suffix,
                                const QString &displayName,
                                const QString &path)
{
    if (forceDirectory) {
        return QIcon::fromTheme(QStringLiteral("folder"));
    }

    QMimeDatabase db;
    QMimeType mime;
    const QString cleanMime = mimeType.trimmed();
    if (!cleanMime.isEmpty()) {
        mime = db.mimeTypeForName(cleanMime);
    }

    if (!mime.isValid() || mime.name() == QLatin1String("application/octet-stream")) {
        const QString name = fileNameFromIconHints(path, displayName, suffix);
        mime = db.mimeTypeForFile(name, QMimeDatabase::MatchExtension);
    }

    QIcon icon;
    if (mime.isValid()) {
        icon = QIcon::fromTheme(mime.iconName());
        if (icon.isNull()) {
            const QString genericName = mime.genericIconName();
            if (!genericName.isEmpty()) {
                icon = QIcon::fromTheme(genericName);
            }
        }
    }

    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("text-x-generic"));
    }
    return icon;
}
#endif

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

QImage windowsProviderFileTypeIcon(const QString &fileName, const QSize &requestedSize)
{
    const auto systemIconIndexForPath = [](const QString &path) {
        SHFILEINFO sfi;
        ZeroMemory(&sfi, sizeof(sfi));

        const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
        if (!SHGetFileInfoW(wpath.c_str(),
                            FILE_ATTRIBUTE_NORMAL,
                            &sfi,
                            sizeof(sfi),
                            SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES)) {
            return -1;
        }
        return sfi.iIcon;
    };

    const QFileInfo fileInfo(fileName);
    const QString suffix = fileInfo.suffix().toLower();
    if (suffix.isEmpty()) {
        return {};
    }

    static const int unknownIndex = systemIconIndexForPath(
        QDir::temp().filePath(QStringLiteral("fm_unknown_file_type.__fm_unknown_assoc__")));
    if (unknownIndex >= 0) {
        const QString fakePath = QDir::temp().filePath(fileName);
        const int suffixIndex = systemIconIndexForPath(fakePath);
        if (suffixIndex >= 0 && suffixIndex == unknownIndex) {
            return {};
        }
    }

    SHFILEINFO sfi;
    ZeroMemory(&sfi, sizeof(sfi));

    UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON;
    if (qMax(requestedSize.width(), requestedSize.height()) > 32) {
        flags &= ~SHGFI_SMALLICON;
        flags |= SHGFI_LARGEICON;
    }

    const QString fakePath = QDir::temp().filePath(fileName);
    const std::wstring wpath = QDir::toNativeSeparators(fakePath).toStdWString();
    if (!SHGetFileInfoW(wpath.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags) || !sfi.hIcon) {
        return {};
    }

    QImage image = QImage::fromHICON(sfi.hIcon);
    DestroyIcon(sfi.hIcon);
    if (imageLooksLikeOpaquePlaceholder(image) || imageHasVisibleCorners(image)) {
        return {};
    }
    return image.isNull()
        ? QImage{}
        : image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
    bool providerPath = false;
    QString displayNameHint;
    QString suffixHint;
    QString mimeTypeHint;
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
            } else if (key == QLatin1String("provider") && value == QLatin1String("true")) {
                providerPath = true;
            } else if (key == QLatin1String("name")) {
                displayNameHint = decodedQueryValue(part.section(QLatin1Char('='), 1));
            } else if (key == QLatin1String("suffix")) {
                suffixHint = decodedQueryValue(part.section(QLatin1Char('='), 1));
            } else if (key == QLatin1String("mime")) {
                mimeTypeHint = decodedQueryValue(part.section(QLatin1Char('='), 1));
            }
        }
    }

    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(32, 32);
    const bool highQualitySystemIcons = shouldUseHighQualitySystemIcons(targetSize);
    if (size) {
        *size = targetSize;
    }

    QFileInfo fi(path);
    QString suffix = suffixHint.isEmpty() ? fi.suffix().toLower() : sanitizedSuffix(suffixHint);
    const bool archivePath = ArchiveSupport::isArchivePath(path);
    if (archivePath) {
        const QString archiveName = ArchiveSupport::archiveFileName(path);
        suffix = QFileInfo(archiveName).suffix().toLower();
    }
    QString cacheKey;
    if (providerPath) {
        cacheKey = providerIconCacheTypeKey(forceDirectory, mimeTypeHint, suffix, displayNameHint);
    } else if (!archivePath && !genericOnly && isPathSpecificIcon(fi)) {
        cacheKey = path;
#ifndef Q_OS_WIN
    } else if (!archivePath && isLinuxSpecialFolder(fi)) {
        cacheKey = path;
#endif
    } else if (forceDirectory || fi.isDir() || (archivePath && path.endsWith(QStringLiteral("|/")))) {
        cacheKey = QStringLiteral("_dir_");
    } else if (archivePath) {
        cacheKey = QStringLiteral("_archive_.") + suffix;
    } else if (suffix.isEmpty()) {
        cacheKey = QStringLiteral("_noext_");
    } else {
        cacheKey = QStringLiteral(".").append(suffix);
    }
    cacheKey += QString::number(targetSize.width()) + QStringLiteral("x") + QString::number(targetSize.height());
    cacheKey += highQualitySystemIcons ? QStringLiteral("|hq") : QStringLiteral("|std");

#ifndef Q_OS_WIN
    if (iconTimingEnabled()) {
        const QString specialFolder = linuxSpecialFolderDebugName(fi.absoluteFilePath());
        if (!specialFolder.isEmpty() || fi.isDir()) {
            qInfo().noquote()
                << "[IconProvider] linux-request"
                << "path=" << path
                << "absolute=" << fi.absoluteFilePath()
                << "exists=" << fi.exists()
                << "dir=" << fi.isDir()
                << "forceDir=" << forceDirectory
                << "special=" << specialFolder
                << "candidates=" << linuxThemeIconCandidatesForSpecialFolder(fi.absoluteFilePath()).join(QLatin1Char(','))
                << "cacheKey=" << cacheKey
                << "theme=" << QIcon::themeName()
                << "fallback=" << QIcon::fallbackThemeName();
        }
    }
#endif

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
    QImage icon;
    if (providerPath) {
#ifdef Q_OS_WIN
        icon = forceDirectory
            ? getWindowsStockFolderIcon(targetSize, highQualitySystemIcons)
            : windowsProviderFileTypeIcon(fileNameFromIconHints(path, displayNameHint, suffix), targetSize);
#else
        const QIcon providerIcon = linuxProviderIconForHints(forceDirectory, mimeTypeHint, suffix, displayNameHint, path);
        if (!providerIcon.isNull()) {
            icon = providerIcon.pixmap(targetSize).toImage();
        }
#endif
    } else {
        icon = getIcon(path, targetSize, forceDirectory, genericOnly, highQualitySystemIcons);
    }
    if (icon.isNull()) {
        icon = renderFallbackIcon(providerPath ? fileNameFromIconHints(path, displayNameHint, suffix) : path,
                                  targetSize,
                                  forceDirectory);
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
    const bool archiveDir = archivePath && (path.endsWith(QStringLiteral("|/")) || archiveName.isEmpty());
    const bool archiveFile = archivePath && !archiveDir;

    if (forceDirectory || info.isDir() || archiveDir) {
#ifndef Q_OS_WIN
        if (!archiveDir) {
            icon = firstThemeIcon(linuxThemeIconCandidatesForSpecialFolder(info.absoluteFilePath()),
                                  linuxSpecialFolderDebugName(info.absoluteFilePath()));
        }
        if (icon.isNull()) {
            icon = QIcon::fromTheme(QStringLiteral("folder"));
            if (iconTimingEnabled()) {
                qInfo().noquote()
                    << "[IconProvider] linux-folder-fallback"
                    << "path=" << path
                    << "ok=" << !icon.isNull()
                    << "theme=" << QIcon::themeName();
            }
        }
#else
        icon = QIcon::fromTheme(QStringLiteral("folder"));
#endif
    } else if (archiveFile && !suffix.isEmpty() && ArchiveSupport::isArchiveExtension(suffix)) {
        icon = QIcon::fromTheme(QStringLiteral("package-x-generic"));
    } else {
#ifdef Q_OS_WIN
        icon = QIcon::fromTheme(QStringLiteral("text-x-generic"));
#else
        if (suffix == QLatin1String("desktop") && info.exists()) {
            icon = linuxDesktopEntryIcon(info.absoluteFilePath());
        }

        if (icon.isNull()) {
            icon = firstThemeIcon(linuxWindowsExecutableIconCandidates(suffix),
                                  QStringLiteral("windows-executable"));
        }

        QMimeDatabase db;
        QMimeType mime;
        if (info.exists()) {
            mime = db.mimeTypeForFile(info);
        } else {
            mime = db.mimeTypeForFile(info.fileName(), QMimeDatabase::MatchExtension);
        }

        if (icon.isNull()) {
            const QString iconName = mime.iconName();
            icon = QIcon::fromTheme(iconName);
        }
        
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
        return {};
    }
    
    return icon.pixmap(requestedSize).toImage();
}
