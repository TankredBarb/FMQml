#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QIcon>
#include <QLockFile>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <memory>

#include "app/AppServices.h"
#include "app/MainWindowSetup.h"
#include "app/QmlEngineBootstrap.h"
#include "app/SplashController.h"
#include "controllers/AppSettingsController.h"
#include "controllers/ThemeController.h"
#include "core/ArchiveFileProvider.h"
#include "core/CleanupSubsystem.h"
#include "platform/PlatformIntegration.h"
#include "tools/NavigationBenchmark.h"

namespace {
constexpr auto AppearanceGroup = "appearance";
constexpr auto SingleInstanceLockFileName = "fm-single-instance.lock";

QString singleInstanceLockFilePath()
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    }
    if (basePath.isEmpty()) {
        basePath = QDir::tempPath();
    }

    QDir().mkpath(basePath);
    return QDir(basePath).absoluteFilePath(QString::fromLatin1(SingleInstanceLockFileName));
}

bool allowOnlyOneInstanceSetting()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    const bool enabled = settings.value(QStringLiteral("allowOnlyOneInstance"), false).toBool();
    settings.endGroup();
    return enabled;
}
}

int main(int argc, char *argv[])
{
    MainWindowSetup::configureProcessIdentity();

    QApplication app(argc, argv);
    MainWindowSetup::configureApplication(app);
    QIcon::fromTheme(QStringLiteral("folder")).pixmap(16, 16);

    if (app.arguments().contains(QStringLiteral("--navigation-benchmark-suite"))) {
        return NavigationBenchmark::runSuite(app);
    }
    if (app.arguments().contains(QStringLiteral("--navigation-benchmark"))) {
        return NavigationBenchmark::run(app);
    }
    const bool navigationGuiBenchmark = app.arguments().contains(
        QStringLiteral("--navigation-gui-benchmark"));

    auto singleInstanceLock = std::make_unique<QLockFile>(singleInstanceLockFilePath());
    if (allowOnlyOneInstanceSetting() && !singleInstanceLock->tryLock(100)) {
        ThemeController theme;
        SplashController splash(&theme);
        splash.showSecondaryInstanceMessage();
        QTimer::singleShot(1400, &app, &QCoreApplication::quit);
        return app.exec();
    }

    if (!navigationGuiBenchmark) {
        CleanupSubsystem::instance().scheduleStartupCleanup();
    }

    const auto archiveCacheCleanup = qScopeGuard([]() {
        ArchiveFileProvider::clearCache();
    });
    AppServices services;
    auto syncSingleInstanceLock = [&services, &singleInstanceLock]() {
        if (services.settings()->allowOnlyOneInstance()) {
            if (!singleInstanceLock->isLocked()) {
                singleInstanceLock->tryLock(100);
            }
        } else if (singleInstanceLock->isLocked()) {
            singleInstanceLock->unlock();
        }
    };
    QObject::connect(services.settings(), &AppSettingsController::allowOnlyOneInstanceChanged,
                     &app, syncSingleInstanceLock);

    std::unique_ptr<SplashController> splash;
    if (!navigationGuiBenchmark) {
        splash = std::make_unique<SplashController>(services.theme());
        splash->show();
    }

    auto qml = std::make_unique<QmlEngineBootstrap>(&services);
    QQuickWindow *mainWindow = qml->loadMainWindow();
    if (!mainWindow) {
        return -1;
    }

    MainWindowSetup::configureMainWindow(mainWindow, services.theme(), services.settings());
    if (navigationGuiBenchmark) {
        mainWindow->setGeometry(0, 0, 1120, 720);
        mainWindow->show();
        const int result = NavigationBenchmark::runGui(app, services, *mainWindow);
        mainWindow->close();
        services.shutdown();
        return result;
    }
    splash->closeWhenReady(mainWindow);

    PlatformIntegration platform;
    platform.attach(mainWindow, &services);
    services.systemTray()->attachWindow(mainWindow);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &services, &AppServices::shutdown);

    MainWindowSetup::showMainWindow(mainWindow, services.settings());
    return app.exec();
}
