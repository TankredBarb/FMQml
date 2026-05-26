#include "MainWindowSetup.h"

#include "../controllers/ThemeController.h"

#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif

namespace {
constexpr const char *AppIconPath = ":/qt/qml/FM/qml/assets/icons/app_icon.png";
QString appIconPath()
{
    return QString::fromLatin1(AppIconPath);
}
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
    QGuiApplication::setWindowIcon(QIcon(appIconPath()));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
}

void MainWindowSetup::configureMainWindow(QQuickWindow *window, ThemeController *theme)
{
    if (!window || !theme) {
        return;
    }

    window->setIcon(QIcon(appIconPath()));
    window->setColor(theme->bg());
    window->setOpacity(0.0);

    QObject::connect(theme, &ThemeController::themeChanged, window, [theme, window]() {
        if (window) {
            window->setColor(theme->bg());
        }
    });

    const QSize targetSize(1120, 720);
    const QRect screenRect = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, targetSize.width(), targetSize.height());
    const QPoint targetTopLeft = screenRect.center() - QPoint(targetSize.width() / 2, targetSize.height() / 2);
    window->setGeometry(QRect(targetTopLeft, targetSize));
}
