#include "QmlEngineBootstrap.h"

#include "AppServices.h"
#include "../core/IconProvider.h"
#include "../core/SvgRecolorProvider.h"
#include "../core/ThumbnailProvider.h"

#include <QCoreApplication>
#include <QQmlContext>
#include <QQuickWindow>
#include <QtGlobal>

QmlEngineBootstrap::QmlEngineBootstrap(AppServices *services)
    : m_services(services)
{
    m_ownedEngine = std::make_unique<QQmlApplicationEngine>();
    m_engine = m_ownedEngine.get();
#ifdef HAS_QT_MULTIMEDIA
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"));
#ifdef FM_QT_PLUGIN_PATH
    QCoreApplication::addLibraryPath(QString::fromUtf8(FM_QT_PLUGIN_PATH));
#endif
    m_engine->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
#ifdef FM_QT_QML_IMPORT_PATH
    m_engine->addImportPath(QString::fromUtf8(FM_QT_QML_IMPORT_PATH));
#endif
#endif

    m_engine->addImageProvider(QStringLiteral("icon"), new IconProvider);
    m_engine->addImageProvider(QStringLiteral("svgrecolor"), new SvgRecolorProvider);
    m_engine->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider(services->thumbnails()));
    m_engine->rootContext()->setContextProperty(QStringLiteral("workspaceController"), services->workspace());
    m_engine->rootContext()->setContextProperty(QStringLiteral("themeController"), services->theme());
    m_engine->rootContext()->setContextProperty(QStringLiteral("quickLookController"), services->quickLook());
    m_engine->rootContext()->setContextProperty(QStringLiteral("propertiesController"), services->properties());
    m_engine->rootContext()->setContextProperty(QStringLiteral("providerPropertiesController"), services->providerProperties());
    m_engine->rootContext()->setContextProperty(QStringLiteral("systemInfoProvider"), services->systemInfo());
    m_engine->rootContext()->setContextProperty(QStringLiteral("diskUsageController"), services->diskUsage());
    m_engine->rootContext()->setContextProperty(QStringLiteral("fileSearchController"), services->fileSearch());
    m_engine->rootContext()->setContextProperty(QStringLiteral("appSettings"), services->settings());
    m_engine->rootContext()->setContextProperty(QStringLiteral("adminController"), services->admin());
    m_engine->rootContext()->setContextProperty(QStringLiteral("favoritesController"), services->favorites());
    m_engine->rootContext()->setContextProperty(QStringLiteral("pluginActionController"), services->pluginActions());
    m_engine->rootContext()->setContextProperty(QStringLiteral("fileTypeIconResolver"), services->fileTypeIcons());
    m_engine->rootContext()->setContextProperty(QStringLiteral("systemTrayController"), services->systemTray());
    m_engine->rootContext()->setContextProperty(QStringLiteral("thumbnailController"), services->thumbnails());
    m_engine->rootContext()->setContextProperty(QStringLiteral("thumbnailDebugOverlayEnabled"),
                                                qEnvironmentVariableIntValue("FM_THUMBNAIL_DEBUG_OVERLAY") != 0);
    m_engine->rootContext()->setContextProperty(QStringLiteral("thumbnailTraceEnabled"),
                                                qEnvironmentVariableIntValue("FM_THUMBNAIL_TRACE") != 0);
    m_engine->rootContext()->setContextProperty(QStringLiteral("inputRoutingLogEnabled"),
                                                qEnvironmentVariableIntValue("FM_INPUT_ROUTING_LOG") != 0);

    QObject::connect(
        m_engine,
        &QQmlApplicationEngine::objectCreationFailed,
        qApp,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
}

QQmlApplicationEngine *QmlEngineBootstrap::engine()
{
    return m_engine;
}

QQuickWindow *QmlEngineBootstrap::loadMainWindow()
{
    const qsizetype existingRootCount = m_engine->rootObjects().size();
    m_engine->loadFromModule(QStringLiteral("FM"), QStringLiteral("App"));

    const auto roots = m_engine->rootObjects();
    for (qsizetype i = existingRootCount; i < roots.size(); ++i) {
        QObject *object = roots.at(i);
        if (auto *window = qobject_cast<QQuickWindow *>(object)) {
            return window;
        }
    }

    return nullptr;
}
