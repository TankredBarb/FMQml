#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "controllers/WorkspaceController.h"
#include "controllers/ThemeController.h"
#include "controllers/QuickLookController.h"
#include "controllers/PropertiesController.h"
#include "core/IconProvider.h"
#include "core/ThumbnailProvider.h"

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

    return app.exec();
}
