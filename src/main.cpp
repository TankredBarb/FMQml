#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QScreen>
#include <QPointer>
#include <QWindow>
#include <QIcon>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif

#include "controllers/WorkspaceController.h"
#include "controllers/ThemeController.h"
#include "controllers/QuickLookController.h"
#include "controllers/PropertiesController.h"
#include "core/IconProvider.h"
#include "core/ThumbnailProvider.h"
#include "core/SystemInfoProvider.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Set AppUserModelID to ensure the taskbar icon is correctly associated
    SetCurrentProcessExplicitAppUserModelID(L"FM.FileManager.1.0");
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("FM"));
    QApplication::setOrganizationName(QStringLiteral("FM"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/FM/qml/assets/icons/app_icon.png")));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    ThemeController theme;

    QQmlApplicationEngine splashEngine;
    splashEngine.rootContext()->setContextProperty(QStringLiteral("themeController"), &theme);
    splashEngine.loadFromModule(QStringLiteral("FM"), QStringLiteral("Splash"));

    QPointer<QWindow> splashWindow;
    for (QObject *obj : splashEngine.rootObjects()) {
        splashWindow = qobject_cast<QWindow *>(obj);
        if (splashWindow) {
            break;
        }
    }

    if (splashWindow) {
        const QSize splashSize = splashWindow->size();
        const QRect screenRect = QGuiApplication::primaryScreen()
            ? QGuiApplication::primaryScreen()->availableGeometry()
            : QRect(0, 0, splashSize.width(), splashSize.height());
        const QPoint splashTopLeft = screenRect.center() - QPoint(splashSize.width() / 2, splashSize.height() / 2);
        splashWindow->setGeometry(QRect(splashTopLeft, splashSize));
        splashWindow->show();
        splashWindow->raise();
        splashWindow->requestActivate();
        qApp->processEvents();
    }

    WorkspaceController workspace;
    QuickLookController quickLook;
    quickLook.setIsoMountManager(workspace.isoMountManager());
    PropertiesController properties;
    SystemInfoProvider systemInfo;

    QQmlApplicationEngine engine;

    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    engine.addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("workspaceController"), &workspace);
    engine.rootContext()->setContextProperty(QStringLiteral("themeController"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("quickLookController"), &quickLook);
    engine.rootContext()->setContextProperty(QStringLiteral("propertiesController"), &properties);
    engine.rootContext()->setContextProperty(QStringLiteral("systemInfoProvider"), &systemInfo);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("FM"), QStringLiteral("App"));

    QQuickWindow *mainWin = nullptr;
    for (auto *obj : engine.rootObjects()) {
        mainWin = qobject_cast<QQuickWindow *>(obj);
        if (mainWin) break;
    }

    if (!mainWin) return -1;

    mainWin->setIcon(QIcon(QStringLiteral(":/qt/qml/FM/qml/assets/icons/app_icon.png")));
    mainWin->setColor(theme.bg());
    mainWin->setOpacity(0.0);

    QObject::connect(&theme, &ThemeController::themeChanged, mainWin,
        [&theme, mainWin]() {
            if (mainWin) {
                mainWin->setColor(theme.bg());
            }
        });

    const QSize targetSize(1120, 720);
    const QRect screenRect = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, targetSize.width(), targetSize.height());
    const QPoint targetTopLeft = screenRect.center() - QPoint(targetSize.width() / 2, targetSize.height() / 2);
    mainWin->setGeometry(QRect(targetTopLeft, targetSize));

    QObject::connect(mainWin, &QQuickWindow::frameSwapped, &app,
        [mainWin, splashWindow]() mutable {
            static int frameCount = 0;
            ++frameCount;
            if (frameCount >= 3 && mainWin) {
                mainWin->setOpacity(1.0);
            }
            if (frameCount >= 3 && splashWindow) {
                splashWindow->close();
            }
        }, Qt::QueuedConnection);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &workspace,
        [&workspace]() {
            workspace.isoMountManager()->unmountAll();
        });

    mainWin->show();

    return app.exec();
}
