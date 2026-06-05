#include <QApplication>
#include <QCoreApplication>
#include <QQuickWindow>

#include "app/AppServices.h"
#include "app/MainWindowSetup.h"
#include "app/QmlEngineBootstrap.h"
#include "app/SplashController.h"
#include "platform/PlatformIntegration.h"

int main(int argc, char *argv[])
{
    MainWindowSetup::configureProcessIdentity();

    QApplication app(argc, argv);
    MainWindowSetup::configureApplication(app);

    AppServices services;
    SplashController splash(services.theme());
    splash.show();

    QmlEngineBootstrap qml(&services);
    QQuickWindow *mainWindow = qml.loadMainWindow();
    if (!mainWindow) {
        return -1;
    }

    MainWindowSetup::configureMainWindow(mainWindow, services.theme(), services.settings());
    splash.closeWhenReady(mainWindow);

    PlatformIntegration platform;
    platform.attach(mainWindow, &services);
    services.systemTray()->attachWindow(mainWindow);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &services, &AppServices::shutdown);

    MainWindowSetup::showMainWindow(mainWindow, services.settings());
    return app.exec();
}
