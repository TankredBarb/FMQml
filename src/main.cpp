#include <QGuiApplication>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QScreen>

#include "controllers/WorkspaceController.h"
#include "controllers/ThemeController.h"
#include "controllers/QuickLookController.h"
#include "controllers/PropertiesController.h"
#include "core/IconProvider.h"
#include "core/ThumbnailProvider.h"
#include "SplashWindow.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("FM"));
    QGuiApplication::setOrganizationName(QStringLiteral("FM"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    WorkspaceController workspace;
    ThemeController theme;
    QuickLookController quickLook;
    PropertiesController properties;

    QQmlApplicationEngine engine;

    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    engine.addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("workspaceController"), &workspace);
    engine.rootContext()->setContextProperty(QStringLiteral("themeController"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("quickLookController"), &quickLook);
    engine.rootContext()->setContextProperty(QStringLiteral("propertiesController"), &properties);

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

    mainWin->create();
    mainWin->setColor(QColor(QStringLiteral("#0D0D0D")));

    mainWin->show();
    mainWin->hide();

    QRect fg = mainWin->frameGeometry();
    QRect cg = mainWin->geometry();
    int frameLeft = cg.x() - fg.x();
    int frameTop = cg.y() - fg.y();
    int frameRight = (fg.x() + fg.width()) - (cg.x() + cg.width());
    int frameBottom = (fg.y() + fg.height()) - (cg.y() + cg.height());

    SplashWindow splash;
    splash.resizeContent(cg.width() + frameLeft + frameRight,
                         cg.height() + frameTop + frameBottom,
                         frameLeft, frameTop, frameTop);
    splash.showSplash();

    mainWin->setPosition(splash.x() + frameLeft,
                         splash.y() + frameTop);
    mainWin->show();

    QTimer::singleShot(800, &splash, [&splash, mainWin]() {
        splash.setFrameConnection(
            QObject::connect(mainWin, &QQuickWindow::frameSwapped,
                &splash, [&splash, mainWin]() {
                    splash.onFrameSwapped(mainWin);
                }, Qt::QueuedConnection));
    });

    return app.exec();
}
