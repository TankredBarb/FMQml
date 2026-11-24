#include "WallpaperSetter.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QLocale>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantList>

#ifdef Q_OS_WIN
#  include <QSettings>
#  include <windows.h>
#endif

namespace {

bool hasSupportedWallpaperExtension(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("png")
        || suffix == QLatin1String("jpg")
        || suffix == QLatin1String("jpeg")
        || suffix == QLatin1String("bmp");
}

QString normalizedExistingFilePath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return {};
    }
    if (!hasSupportedWallpaperExtension(info.absoluteFilePath())) {
        return {};
    }
    return info.absoluteFilePath();
}

QString normalizedFillMode(const QString &mode)
{
    const QString lower = mode.trimmed().toLower();
    if (lower == QLatin1String("fill")
        || lower == QLatin1String("fit")
        || lower == QLatin1String("stretch")
        || lower == QLatin1String("tile")
        || lower == QLatin1String("center")) {
        return lower;
    }
    return QStringLiteral("fill");
}

#ifdef Q_OS_LINUX
QString jsStringLiteral(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    escaped.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    return QStringLiteral("\"%1\"").arg(escaped);
}

bool isKdeSession()
{
    const QString xdg = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    const QString session = qEnvironmentVariable("DESKTOP_SESSION").toLower();
    return xdg.contains(QStringLiteral("kde"))
        || xdg.contains(QStringLiteral("plasma"))
        || session.contains(QStringLiteral("kde"))
        || session.contains(QStringLiteral("plasma"))
        || qEnvironmentVariableIsSet("KDE_FULL_SESSION");
}

bool runProcess(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.start(program, arguments);
    return process.waitForFinished(10000)
        && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
}

int kdeFillModeValue(const QString &mode)
{
    const QString normalized = normalizedFillMode(mode);
    if (normalized == QLatin1String("stretch")) {
        return 0;
    }
    if (normalized == QLatin1String("fit")) {
        return 1;
    }
    if (normalized == QLatin1String("tile")) {
        return 3;
    }
    if (normalized == QLatin1String("center")) {
        return 6;
    }
    return 2; // fill (Scaled and Cropped)
}

bool setKdeWallpaper(const QString &path, const WallpaperSetter::Options &options)
{
    static const QString dbus = []() {
        for (const QString &candidate : {QStringLiteral("qdbus6"), QStringLiteral("qdbus"), QStringLiteral("qdbus-qt5")}) {
            if (!QStandardPaths::findExecutable(candidate).isEmpty()) {
                return candidate;
            }
        }
        return QString();
    }();
    if (dbus.isEmpty()) {
        return false;
    }

    const QString fileUrl = QUrl::fromLocalFile(path).toString();
    const int fillMode = kdeFillModeValue(options.fillMode);
    const QColor bg = options.backgroundColor.isValid() ? options.backgroundColor : QColor(12, 18, 28);
    const QString colorValue = QStringLiteral("%1,%2,%3").arg(bg.red()).arg(bg.green()).arg(bg.blue());
    const QString blurLiteral = options.blurBackground ? QStringLiteral("true") : QStringLiteral("false");

    const QString script = QStringLiteral(
        "var allDesktops = desktops();"
        "var targetScreen = %1;"
        "for (var i = 0; i < allDesktops.length; ++i) {"
        "  var desktop = allDesktops[i];"
        "  if (targetScreen >= 0 && desktop.screen !== targetScreen) continue;"
        "  desktop.wallpaperPlugin = \"org.kde.image\";"
        "  desktop.currentConfigGroup = [\"Wallpaper\", \"org.kde.image\", \"General\"];"
        "  desktop.writeConfig(\"Image\", %2);"
        "  desktop.writeConfig(\"FillMode\", %3);"
        "  desktop.writeConfig(\"Color\", %4);"
        "  desktop.writeConfig(\"Blur\", %5);"
        "}"
    ).arg(options.screenIndex)
     .arg(jsStringLiteral(fileUrl))
     .arg(fillMode)
     .arg(jsStringLiteral(colorValue))
     .arg(blurLiteral);

    return runProcess(dbus, {
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell.evaluateScript"),
        script
    });
}
#endif

} // namespace

namespace WallpaperSetter {

bool canSetWallpaperForPath(const QString &path)
{
    if (normalizedExistingFilePath(path).isEmpty()) {
        return false;
    }
#ifdef Q_OS_WIN
    return true;
#elif defined(Q_OS_LINUX)
    return isKdeSession();
#else
    return false;
#endif
}

QVariantMap screenCapabilitiesInfo()
{
    QVariantMap info;
    QVariantList screens;
    const QList<QScreen *> appScreens = QGuiApplication::screens();
    const QScreen *primaryScreen = QGuiApplication::primaryScreen();
    for (int i = 0; i < appScreens.size(); ++i) {
        const QScreen *screen = appScreens.at(i);
        if (!screen) {
            continue;
        }
        const QSize geometry = screen->size();
        const qreal dpr = screen->devicePixelRatio();
        const int pixelW = qMax(1, qRound(geometry.width() * dpr));
        const int pixelH = qMax(1, qRound(geometry.height() * dpr));
        QVariantMap entry;
        entry.insert(QStringLiteral("index"), i);
        entry.insert(QStringLiteral("name"), screen->name().isEmpty()
                                                 ? QStringLiteral("Display %1").arg(i + 1)
                                                 : screen->name());
        entry.insert(QStringLiteral("width"), pixelW);
        entry.insert(QStringLiteral("height"), pixelH);
        entry.insert(QStringLiteral("aspectRatio"), static_cast<qreal>(pixelW) / static_cast<qreal>(pixelH));
        entry.insert(QStringLiteral("primary"), screen == primaryScreen);
        screens.append(entry);
    }
    info.insert(QStringLiteral("screens"), screens);

#ifdef Q_OS_LINUX
    const bool kde = isKdeSession();
    info.insert(QStringLiteral("supportsBlur"), kde);
    info.insert(QStringLiteral("supportsPerScreen"), kde && screens.size() > 1);
#else
    info.insert(QStringLiteral("supportsBlur"), false);
    info.insert(QStringLiteral("supportsPerScreen"), false);
#endif

    return info;
}

PreviewLoadResult probeFileMetadata(const QString &path)
{
    PreviewLoadResult result;
    const QString normalizedPath = normalizedExistingFilePath(path);
    const QFileInfo fileInfo(normalizedPath.isEmpty() ? path : normalizedPath);

    result.valid = !normalizedPath.isEmpty();
    result.filePath = normalizedPath.isEmpty() ? path : normalizedPath;
    result.fileUrl = normalizedPath.isEmpty()
        ? QString()
        : QUrl::fromLocalFile(normalizedPath).toString();
    result.fileName = fileInfo.fileName();
    result.fileSizeText = fileInfo.exists() ? QLocale().formattedDataSize(fileInfo.size()) : QString();

    if (result.valid) {
        QImageReader reader(normalizedPath);
        reader.setAutoTransform(true);
        QSize imageSize = reader.size();
        if (imageSize.isValid() && (reader.transformation() & QImageIOHandler::TransformationRotate90)) {
            imageSize.transpose();
        }
        result.imageWidth = imageSize.isValid() ? imageSize.width() : 0;
        result.imageHeight = imageSize.isValid() ? imageSize.height() : 0;
    } else {
        result.errorText = QStringLiteral("Wallpaper is only supported for local PNG, JPG, JPEG, or BMP files.");
    }

    return result;
}

void decodePreviewImages(PreviewLoadResult &result,
                         int maxPreviewDimension,
                         int maxBlurDimension,
                         const std::function<bool()> &isCancelled)
{
    if (!result.valid || result.filePath.isEmpty() || (isCancelled && isCancelled())) {
        return;
    }

    QImageReader reader(result.filePath);
    reader.setAutoTransform(true);

    const QSize rawSize = reader.size();
    const int boundedPreviewDim = qMax(64, maxPreviewDimension);
    const int boundedBlurDim = qMax(32, maxBlurDimension);
    const QSize targetBounds(boundedPreviewDim, boundedPreviewDim);

    if (reader.supportsOption(QImageIOHandler::ScaledSize)
        && rawSize.isValid()
        && (rawSize.width() > boundedPreviewDim || rawSize.height() > boundedPreviewDim)) {
        QSize scaled = rawSize.scaled(targetBounds, Qt::KeepAspectRatio);
        scaled.setWidth(qMax(1, scaled.width()));
        scaled.setHeight(qMax(1, scaled.height()));
        reader.setScaledSize(scaled);
    }

    QImage image = reader.read();
    if (isCancelled && isCancelled()) {
        return;
    }

    if (image.isNull()) {
        const QString readerError = reader.errorString().trimmed();
        result.previewErrorText = readerError.isEmpty()
            ? QStringLiteral("Preview could not be decoded for this image.")
            : readerError;
        return;
    }

    if (result.imageWidth <= 0 || result.imageHeight <= 0) {
        if (rawSize.isValid()) {
            QSize effective = rawSize;
            if (reader.transformation() & QImageIOHandler::TransformationRotate90) {
                effective.transpose();
            }
            result.imageWidth = effective.width();
            result.imageHeight = effective.height();
        } else {
            result.imageWidth = image.width();
            result.imageHeight = image.height();
        }
    }

    if (image.width() > boundedPreviewDim || image.height() > boundedPreviewDim) {
        if (image.width() > boundedPreviewDim * 2 || image.height() > boundedPreviewDim * 2) {
            image = image.scaled(QSize(boundedPreviewDim * 2, boundedPreviewDim * 2),
                                 Qt::KeepAspectRatio,
                                 Qt::FastTransformation);
            if (isCancelled && isCancelled()) {
                return;
            }
        }
        image = image.scaled(targetBounds, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (isCancelled && isCancelled()) {
            return;
        }
    }

    if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(image.hasAlphaChannel()
                                      ? QImage::Format_ARGB32_Premultiplied
                                      : QImage::Format_RGB32);
    }
    image.setDevicePixelRatio(1.0);

    if (image.width() > boundedBlurDim || image.height() > boundedBlurDim) {
        result.blurImage = image.scaled(QSize(boundedBlurDim, boundedBlurDim),
                                        Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
    } else {
        result.blurImage = image;
    }
    result.blurImage.setDevicePixelRatio(1.0);
    result.previewImage = std::move(image);
}

QVariantMap previewInfo(const QString &path)
{
    const PreviewLoadResult meta = probeFileMetadata(path);
    QVariantMap info = screenCapabilitiesInfo();
    info.insert(QStringLiteral("valid"), meta.valid);
    info.insert(QStringLiteral("filePath"), meta.filePath);
    info.insert(QStringLiteral("fileUrl"), meta.fileUrl);
    info.insert(QStringLiteral("fileName"), meta.fileName);
    info.insert(QStringLiteral("fileSizeText"), meta.fileSizeText);
    info.insert(QStringLiteral("imageWidth"), meta.imageWidth);
    info.insert(QStringLiteral("imageHeight"), meta.imageHeight);
    return info;
}

bool setWallpaper(const QString &path, QString *errorMessage)
{
    return setWallpaper(path, Options{}, errorMessage);
}

bool setWallpaper(const QString &path, const Options &options, QString *errorMessage)
{
    const QString normalizedPath = normalizedExistingFilePath(path);
    if (normalizedPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Wallpaper is only supported for local PNG, JPG, JPEG, or BMP files.");
        }
        return false;
    }

#ifdef Q_OS_WIN
    const QString mode = normalizedFillMode(options.fillMode);
    QString wallpaperStyle = QStringLiteral("10");
    QString tileWallpaper = QStringLiteral("0");
    if (mode == QLatin1String("fit")) {
        wallpaperStyle = QStringLiteral("6");
    } else if (mode == QLatin1String("stretch")) {
        wallpaperStyle = QStringLiteral("2");
    } else if (mode == QLatin1String("tile")) {
        wallpaperStyle = QStringLiteral("0");
        tileWallpaper = QStringLiteral("1");
    } else if (mode == QLatin1String("center")) {
        wallpaperStyle = QStringLiteral("0");
    }

    QSettings desktopSettings(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Desktop"), QSettings::NativeFormat);
    desktopSettings.setValue(QStringLiteral("WallpaperStyle"), wallpaperStyle);
    desktopSettings.setValue(QStringLiteral("TileWallpaper"), tileWallpaper);
    desktopSettings.sync();

    if (options.backgroundColor.isValid()) {
        const QColor bg = options.backgroundColor;
        QSettings colorSettings(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Colors"), QSettings::NativeFormat);
        colorSettings.setValue(QStringLiteral("Background"),
                               QStringLiteral("%1 %2 %3").arg(bg.red()).arg(bg.green()).arg(bg.blue()));
        colorSettings.sync();
        int element = COLOR_DESKTOP;
        COLORREF colorRef = RGB(bg.red(), bg.green(), bg.blue());
        SetSysColors(1, &element, &colorRef);
    }

    const QString nativePath = QDir::toNativeSeparators(normalizedPath);
    wchar_t *buffer = const_cast<wchar_t *>(reinterpret_cast<const wchar_t *>(nativePath.utf16()));
    const BOOL ok = SystemParametersInfoW(SPI_SETDESKWALLPAPER,
                                          0,
                                          buffer,
                                          SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    if (!ok && errorMessage) {
        *errorMessage = QStringLiteral("Windows rejected the wallpaper change.");
    }
    return ok == TRUE;
#elif defined(Q_OS_LINUX)
    if (!isKdeSession()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Wallpaper setting is currently supported on Linux only for KDE Plasma.");
        }
        return false;
    }
    const bool ok = setKdeWallpaper(normalizedPath, options);
    if (!ok && errorMessage) {
        *errorMessage = QStringLiteral("Failed to set wallpaper through KDE Plasma.");
    }
    return ok;
#else
    if (errorMessage) {
        *errorMessage = QStringLiteral("Wallpaper setting is not supported on this platform.");
    }
    return false;
#endif
}

} // namespace WallpaperSetter
