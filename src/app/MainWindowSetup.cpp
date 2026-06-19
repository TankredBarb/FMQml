#include "MainWindowSetup.h"

#include "../controllers/AppSettingsController.h"
#include "../controllers/ThemeController.h"

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#endif

namespace {
constexpr const char *AppIconPath = ":/qt/qml/FM/qml/assets/icons/app_icon.png";
QString appIconPath()
{
    return QString::fromLatin1(AppIconPath);
}

bool shouldStartMaximized(AppSettingsController *settings)
{
    return settings && settings->workspaceState().value(QStringLiteral("windowMaximized"), false).toBool();
}

QColor blendColors(const QColor &base, const QColor &overlay, qreal amount)
{
    const qreal clamped = qBound<qreal>(0.0, amount, 1.0);
    return QColor::fromRgbF(
        base.redF() * (1.0 - clamped) + overlay.redF() * clamped,
        base.greenF() * (1.0 - clamped) + overlay.greenF() * clamped,
        base.blueF() * (1.0 - clamped) + overlay.blueF() * clamped);
}

QColor compositeThemeColor(const QColor &base, const QColor &overlay)
{
    return blendColors(base, overlay, overlay.alphaF());
}

#ifdef Q_OS_WIN
COLORREF colorRefFromQColor(const QColor &color)
{
    return RGB(color.red(), color.green(), color.blue());
}

QColor titleBarColorForTheme(const ThemeController *theme)
{
    const QColor surface = theme->panelSurfaceStrong().isValid()
        ? theme->panelSurfaceStrong()
        : theme->surface();
    const QColor start = compositeThemeColor(surface, theme->chromeGradientStart());
    const QColor mid = compositeThemeColor(surface, theme->chromeGradientMid());
    const QColor end = compositeThemeColor(surface, theme->chromeGradientEnd());
    return blendColors(blendColors(start, mid, 0.48), end, 0.18);
}

QColor titleTextColorForBackground(const QColor &background, const ThemeController *theme)
{
    const QColor preferred = theme->textPrimary();
    if (background.lightnessF() < 0.48 && preferred.lightnessF() > 0.56) {
        return preferred;
    }
    if (background.lightnessF() >= 0.48 && preferred.lightnessF() < 0.44) {
        return preferred;
    }
    return background.lightnessF() < 0.48 ? QColor(Qt::white) : QColor(Qt::black);
}

void setDwmColorAttribute(HWND hwnd, DWORD attribute, const QColor &color)
{
    const COLORREF colorRef = colorRefFromQColor(color);
    DwmSetWindowAttribute(hwnd, attribute, &colorRef, sizeof(colorRef));
}

void applyNativeTitleBarTheme(QQuickWindow *window, const ThemeController *theme)
{
    if (!window || !theme) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    const BOOL darkMode = theme->isDark() ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    const QColor caption = titleBarColorForTheme(theme);
    setDwmColorAttribute(hwnd, DWMWA_CAPTION_COLOR, caption);
    setDwmColorAttribute(hwnd, DWMWA_TEXT_COLOR, titleTextColorForBackground(caption, theme));
    setDwmColorAttribute(hwnd, DWMWA_BORDER_COLOR,
                         blendColors(theme->panelBorder(), caption, theme->isDark() ? 0.32 : 0.24));
}
#else
void applyNativeTitleBarTheme(QQuickWindow *, const ThemeController *)
{
}
#endif
}

void MainWindowSetup::configureProcessIdentity()
{
#ifdef Q_OS_WIN
    SetCurrentProcessExplicitAppUserModelID(L"FM.FileManager.1.0");
#endif
}

void MainWindowSetup::configureApplication(QApplication &app)
{
    Q_UNUSED(app);
    QApplication::setApplicationName(QStringLiteral("FM"));
    QApplication::setOrganizationName(QStringLiteral("FM"));
#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
    QGuiApplication::setDesktopFileName(QStringLiteral("fm"));
#endif
    QGuiApplication::setWindowIcon(QIcon(appIconPath()));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
}

void MainWindowSetup::configureMainWindow(QQuickWindow *window, ThemeController *theme, AppSettingsController *settings)
{
    if (!window || !theme) {
        return;
    }

    window->setIcon(QIcon(appIconPath()));
    window->setColor(theme->bg());
    window->setOpacity(0.0);
    applyNativeTitleBarTheme(window, theme);

    QObject::connect(theme, &ThemeController::themeChanged, window, [theme, window]() {
        if (window) {
            window->setColor(theme->bg());
            applyNativeTitleBarTheme(window, theme);
        }
    });

    const QVariantMap state = settings ? settings->workspaceState() : QVariantMap();
    const QVariantMap geometry = settings
        ? settings->sanitizedWindowGeometry(state, 1120, 720)
        : QVariantMap();

    if (geometry.value(QStringLiteral("valid")).toBool()) {
        window->setGeometry(QRect(
            geometry.value(QStringLiteral("x")).toInt(),
            geometry.value(QStringLiteral("y")).toInt(),
            geometry.value(QStringLiteral("width")).toInt(),
            geometry.value(QStringLiteral("height")).toInt()));
    } else {
        const QSize targetSize(1120, 720);
        const QRect screenRect = QGuiApplication::primaryScreen()
            ? QGuiApplication::primaryScreen()->availableGeometry()
            : QRect(0, 0, targetSize.width(), targetSize.height());
        const QPoint targetTopLeft = screenRect.center() - QPoint(targetSize.width() / 2, targetSize.height() / 2);
        window->setGeometry(QRect(targetTopLeft, targetSize));
    }

    if (state.value(QStringLiteral("windowMaximized"), false).toBool()) {
        window->setWindowStates(window->windowStates() | Qt::WindowMaximized);
    }
}

void MainWindowSetup::showMainWindow(QQuickWindow *window, AppSettingsController *settings)
{
    if (!window) {
        return;
    }

    if (shouldStartMaximized(settings)) {
        window->showMaximized();
        return;
    }

    window->show();
}
