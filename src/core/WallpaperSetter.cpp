#include "WallpaperSetter.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WIN
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
    return process.waitForFinished(4000)
        && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
}

bool setKdeWallpaper(const QString &path)
{
    QString dbus;
    for (const QString &candidate : {QStringLiteral("qdbus6"), QStringLiteral("qdbus"), QStringLiteral("qdbus-qt5")}) {
        if (!QStandardPaths::findExecutable(candidate).isEmpty()) {
            dbus = candidate;
            break;
        }
    }
    if (dbus.isEmpty()) {
        return false;
    }

    const QString fileUrl = QUrl::fromLocalFile(path).toString();
    const QString script = QStringLiteral(
        "var allDesktops = desktops();"
        "for (var i = 0; i < allDesktops.length; ++i) {"
        "  var desktop = allDesktops[i];"
        "  desktop.wallpaperPlugin = \"org.kde.image\";"
        "  desktop.currentConfigGroup = [\"Wallpaper\", \"org.kde.image\", \"General\"];"
        "  desktop.writeConfig(\"Image\", %1);"
        "}"
    ).arg(jsStringLiteral(fileUrl));

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

bool setWallpaper(const QString &path, QString *errorMessage)
{
    const QString normalizedPath = normalizedExistingFilePath(path);
    if (normalizedPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Wallpaper is only supported for local PNG, JPG, JPEG, or BMP files.");
        }
        return false;
    }

#ifdef Q_OS_WIN
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
    const bool ok = setKdeWallpaper(normalizedPath);
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
