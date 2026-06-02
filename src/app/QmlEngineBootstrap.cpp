#include "QmlEngineBootstrap.h"

#include "AppServices.h"
#include "../core/IconProvider.h"
#include "../core/SvgRecolorProvider.h"
#include "../core/ThumbnailProvider.h"

#include <QCoreApplication>
#include <QQmlContext>
#include <QQuickWindow>

QmlEngineBootstrap::QmlEngineBootstrap(AppServices *services)
    : m_services(services)
{
#ifdef HAS_QT_MULTIMEDIA
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"));
#ifdef FM_QT_PLUGIN_PATH
    QCoreApplication::addLibraryPath(QString::fromUtf8(FM_QT_PLUGIN_PATH));
#endif
    m_engine.addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
#ifdef FM_QT_QML_IMPORT_PATH
    m_engine.addImportPath(QString::fromUtf8(FM_QT_QML_IMPORT_PATH));
#endif
#endif

    m_engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    m_engine.addImageProvider(QStringLiteral("svgrecolor"), new SvgRecolorProvider);
    m_engine.addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
    m_engine.rootContext()->setContextProperty(QStringLiteral("workspaceController"), services->workspace());
    m_engine.rootContext()->setContextProperty(QStringLiteral("themeController"), services->theme());
    m_engine.rootContext()->setContextProperty(QStringLiteral("quickLookController"), services->quickLook());
    m_engine.rootContext()->setContextProperty(QStringLiteral("propertiesController"), services->properties());
    m_engine.rootContext()->setContextProperty(QStringLiteral("systemInfoProvider"), services->systemInfo());
    m_engine.rootContext()->setContextProperty(QStringLiteral("diskUsageController"), services->diskUsage());
    m_engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), services->settings());
    m_engine.rootContext()->setContextProperty(QStringLiteral("adminController"), services->admin());
    m_engine.rootContext()->setContextProperty(QStringLiteral("favoritesController"), services->favorites());
    m_engine.rootContext()->setContextProperty(QStringLiteral("fileTypeIconResolver"), services->fileTypeIcons());

    QObject::connect(
        &m_engine,
        &QQmlApplicationEngine::objectCreationFailed,
        qApp,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
}

QQuickWindow *QmlEngineBootstrap::loadMainWindow()
{
    m_engine.loadFromModule(QStringLiteral("FM"), QStringLiteral("App"));

    for (QObject *object : m_engine.rootObjects()) {
        if (auto *window = qobject_cast<QQuickWindow *>(object)) {
            return window;
        }
    }

    return nullptr;
}
