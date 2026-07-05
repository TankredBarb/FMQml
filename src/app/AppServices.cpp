#include "AppServices.h"

#include "../core/ArchiveSupport.h"
#include "../core/FileProviderPluginRegistry.h"
#include "../core/IsoSupport.h"
#include "../models/DirectoryModel.h"

#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMetaObject>
#include <QPointer>
#include <Qt>
#include <QtConcurrent/QtConcurrentRun>
#include <QUrl>

namespace {
int boundedInt(const QVariantMap &state, const QString &key, int fallback, int min, int max)
{
    bool ok = false;
    const int value = state.value(key, fallback).toInt(&ok);
    if (!ok) {
        return fallback;
    }
    return qBound(min, value, max);
}

bool panelShortcutLoggingEnabled()
{
    static const bool enabled = qEnvironmentVariableIntValue("FM_PANEL_SHORTCUT_LOG") != 0;
    return enabled;
}

bool inputRoutingLoggingEnabled()
{
    static const bool enabled = qEnvironmentVariableIntValue("FM_INPUT_ROUTING_LOG") != 0;
    return enabled;
}

bool isInputRoutingKey(int key, Qt::KeyboardModifiers modifiers)
{
    switch (key) {
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
    case Qt::Key_F2:
    case Qt::Key_F3:
    case Qt::Key_F4:
    case Qt::Key_F5:
    case Qt::Key_Delete:
    case Qt::Key_Space:
        return true;
    case Qt::Key_F:
    case Qt::Key_L:
        return modifiers.testFlag(Qt::ControlModifier);
    default:
        return false;
    }
}

const char *eventTypeName(QEvent::Type type)
{
    switch (type) {
    case QEvent::ShortcutOverride:
        return "ShortcutOverride";
    case QEvent::KeyPress:
        return "KeyPress";
    default:
        return "Other";
    }
}

struct FavoriteOpenResolution {
    enum class Action {
        Navigate,
        OpenExternal,
        Ignore
    };

    Action action = Action::Ignore;
    QString path;
};

FavoriteOpenResolution resolveFavoriteOpenPath(QString path)
{
    path = path.trimmed();
    if (path.isEmpty()) {
        return {};
    }

    if (ArchiveSupport::isArchivePath(path)) {
        return {FavoriteOpenResolution::Action::Navigate, path};
    }

    const QFileInfo info(path);
    if (!info.isFile()) {
        return {FavoriteOpenResolution::Action::Navigate, path};
    }

    const QString suffix = info.suffix().toLower();
    if (ArchiveSupport::isArchiveExtension(suffix) || IsoSupport::isIsoImageExtension(suffix)) {
        return {FavoriteOpenResolution::Action::Navigate, path};
    }

    return {FavoriteOpenResolution::Action::OpenExternal, path};
}

bool pathBelongsToProviderRoot(const QString &path, const QString &rootPath)
{
    QString normalizedPath = path.trimmed();
    QString normalizedRoot = rootPath.trimmed();
    if (normalizedPath.isEmpty() || normalizedRoot.isEmpty() || !normalizedRoot.contains(QStringLiteral("://"))) {
        return false;
    }
    if (normalizedPath.compare(normalizedRoot, Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (!normalizedRoot.endsWith(QLatin1Char('/'))) {
        normalizedRoot += QLatin1Char('/');
    }
    return normalizedPath.startsWith(normalizedRoot, Qt::CaseInsensitive);
}
}

AppServices::AppServices(QObject *parent)
    : QObject(parent)
{
    qApp->installEventFilter(this);
    m_quickLook.setIsoMountManager(m_workspace.isoMountManager());
    m_favorites.setIsoMountManager(m_workspace.isoMountManager());
    m_settings.setThemeController(&m_theme);
    m_systemTray.setThemeController(&m_theme);
    m_systemTray.setOperationQueue(m_workspace.operationQueue());
    m_systemTray.setSettings(&m_settings);
    connect(m_workspace.operationQueue(), &OperationQueue::administratorOperationSucceeded,
            &m_admin, &AdminController::refreshAdminModeAfterOperation);
    connect(m_workspace.leftPanel(), &FilePanelController::administratorOperationSucceeded,
            &m_admin, &AdminController::refreshAdminModeAfterOperation);
    connect(m_workspace.rightPanel(), &FilePanelController::administratorOperationSucceeded,
            &m_admin, &AdminController::refreshAdminModeAfterOperation);
    const auto releaseQuickLookForRemovedRoot = [this](const QString &rootPath) {
        const bool providerRoot = rootPath.contains(QStringLiteral("://"));
        const bool matches = providerRoot
            ? (pathBelongsToProviderRoot(m_quickLook.path(), rootPath)
               || pathBelongsToProviderRoot(m_quickLook.absolutePath(), rootPath))
            : (m_workspace.volumeMonitor()->pathBelongsToRoot(m_quickLook.path(), rootPath)
               || m_workspace.volumeMonitor()->pathBelongsToRoot(m_quickLook.absolutePath(), rootPath));
        if (matches) {
            m_quickLook.preview(QStringLiteral("devices://"));
        }
    };
    connect(m_workspace.volumeMonitor(), &VolumeMonitor::volumeRemoved, this,
            [releaseQuickLookForRemovedRoot](const QString &rootPath, const QString &) {
                releaseQuickLookForRemovedRoot(rootPath);
            });
    connect(&m_workspace, &WorkspaceController::deviceEjectStarted, this,
            [releaseQuickLookForRemovedRoot](const QString &rootPath, const QString &) {
                releaseQuickLookForRemovedRoot(rootPath);
            });
    connect(&m_workspace, &WorkspaceController::deviceRemoved, this,
            [releaseQuickLookForRemovedRoot](const QString &rootPath, const QString &) {
                releaseQuickLookForRemovedRoot(rootPath);
            });
    connect(&m_favorites, &FavoritesController::openPathRequested, &m_workspace, [this](const QString &path) {
        FilePanelController *panel = m_workspace.activePanel() == 0
            ? m_workspace.leftPanel()
            : m_workspace.rightPanel();
        QPointer<AppServices> self(this);
        QPointer<FilePanelController> targetPanel(panel);
        (void)QtConcurrent::run([self, targetPanel, path]() {
            const FavoriteOpenResolution resolution = resolveFavoriteOpenPath(path);
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self.data(), [self, targetPanel, resolution]() {
                if (!self) {
                    return;
                }
                switch (resolution.action) {
                case FavoriteOpenResolution::Action::OpenExternal:
                    QDesktopServices::openUrl(QUrl::fromLocalFile(resolution.path));
                    return;
                case FavoriteOpenResolution::Action::Navigate:
                    if (targetPanel) {
                        targetPanel->openPath(resolution.path);
                    }
                    return;
                case FavoriteOpenResolution::Action::Ignore:
                    return;
                }
            }, Qt::QueuedConnection);
        });
    });
    connect(m_workspace.leftPanel(), &FilePanelController::pathNavigated, &m_favorites, &FavoritesController::recordVisit);
    connect(m_workspace.rightPanel(), &FilePanelController::pathNavigated, &m_favorites, &FavoritesController::recordVisit);
    connect(&m_pluginActions, &PluginActionController::pluginsChanged,
            m_workspace.placesModel(), &PlacesModel::refresh);
    connect(&m_pluginActions, &PluginActionController::placesRefreshRequested,
            m_workspace.placesModel(), &PlacesModel::refresh);
    FileProviderPluginRegistry::instance().loadDefaultPluginDirectories();
    m_workspace.placesModel()->refresh();
    m_workspace.placesModel()->refreshProviderPlacesAsync();
    restoreInitialWorkspaceState();
}

bool AppServices::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (inputRoutingLoggingEnabled() && isInputRoutingKey(keyEvent->key(), keyEvent->modifiers())) {
            qInfo().noquote()
                << "[InputRouting]"
                << "stage=cpp-event"
                << "type=" << eventTypeName(event->type())
                << "key=" << keyEvent->key()
                << "modifiers=" << int(keyEvent->modifiers())
                << "acceptedBefore=" << keyEvent->isAccepted()
                << "focusWindow=" << (qApp->focusWindow() ? qApp->focusWindow()->metaObject()->className() : "null")
                << "focusObject=" << (qApp->focusObject() ? qApp->focusObject()->metaObject()->className() : "null")
                << "watched=" << (watched ? watched->metaObject()->className() : "null");
        }
        if (canHandlePanelTransferShortcut(keyEvent->key(), keyEvent->modifiers())) {
            if (panelShortcutLoggingEnabled()) {
                qInfo().noquote()
                    << "[PanelShortcut]"
                    << eventTypeName(event->type())
                    << "key=" << keyEvent->key()
                    << "modifiers=" << int(keyEvent->modifiers())
                    << "activePanel=" << m_workspace.activePanel()
                    << "selected=" << (m_workspace.activePanel() == 0
                        ? m_workspace.leftPanel()->directoryModel()->selectedCount()
                        : m_workspace.rightPanel()->directoryModel()->selectedCount());
            }
            keyEvent->accept();
            if (event->type() == QEvent::KeyPress) {
                handlePanelTransferShortcut(keyEvent->key(), keyEvent->modifiers());
            }
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

bool AppServices::canHandlePanelTransferShortcut(int key, Qt::KeyboardModifiers modifiers)
{
    if (key != Qt::Key_F5 || (modifiers != Qt::NoModifier && modifiers != Qt::ShiftModifier)) {
        return false;
    }

    if (QWindow *window = qApp->focusWindow()) {
        const QVariant transferEnabled = window->property("canTransferToOpposite");
        if (transferEnabled.isValid() && !transferEnabled.toBool()) {
            return false;
        }
    }

    if (!m_workspace.splitEnabled() || m_workspace.operationQueue()->busy()) {
        return false;
    }

    FilePanelController *active = m_workspace.activePanel() == 0
        ? m_workspace.leftPanel()
        : m_workspace.rightPanel();
    if (!active || !active->directoryModel() || active->directoryModel()->selectedCount() <= 0) {
        return false;
    }

    return true;
}

bool AppServices::handlePanelTransferShortcut(int key, Qt::KeyboardModifiers modifiers)
{
    if (!canHandlePanelTransferShortcut(key, modifiers)) {
        return false;
    }

    if (modifiers == Qt::ShiftModifier) {
        m_workspace.moveActiveSelectionToOpposite();
    } else {
        m_workspace.copyActiveSelectionToOpposite();
    }
    return true;
}

WorkspaceController *AppServices::workspace()
{
    return &m_workspace;
}

ThemeController *AppServices::theme()
{
    return &m_theme;
}

QuickLookController *AppServices::quickLook()
{
    return &m_quickLook;
}

PropertiesController *AppServices::properties()
{
    return &m_properties;
}

ProviderPropertiesController *AppServices::providerProperties()
{
    return &m_providerProperties;
}

SystemInfoProvider *AppServices::systemInfo()
{
    return &m_systemInfo;
}

DiskUsageController *AppServices::diskUsage()
{
    return &m_diskUsage;
}

FileSearchController *AppServices::fileSearch()
{
    return &m_fileSearch;
}

AppSettingsController *AppServices::settings()
{
    return &m_settings;
}

AdminController *AppServices::admin()
{
    return &m_admin;
}

FavoritesController *AppServices::favorites()
{
    return &m_favorites;
}

PluginActionController *AppServices::pluginActions()
{
    return &m_pluginActions;
}

FileTypeIconResolver *AppServices::fileTypeIcons()
{
    return &m_fileTypeIcons;
}

SystemTrayController *AppServices::systemTray()
{
    return &m_systemTray;
}

void AppServices::shutdown()
{
    m_workspace.isoMountManager()->unmountAll();
}

void AppServices::restoreInitialWorkspaceState()
{
    const QVariantMap state = m_settings.workspaceState();
    const bool showHidden = state.value(QStringLiteral("showHidden"), false).toBool();

    m_workspace.leftPanel()->directoryModel()->setShowHidden(showHidden);
    m_workspace.rightPanel()->directoryModel()->setShowHidden(showHidden);
    m_workspace.treeModel()->setShowHidden(showHidden);

    m_workspace.setActivePanel(0);
    m_workspace.setSplitEnabled(state.value(QStringLiteral("splitEnabled"), false).toBool());

    m_workspace.leftPanel()->setViewMode(boundedInt(state, QStringLiteral("leftViewMode"), 0, 0, 2));
    m_workspace.rightPanel()->setViewMode(boundedInt(state, QStringLiteral("rightViewMode"), 0, 0, 2));

    m_workspace.leftPanel()->setPanelSortPolicy(
        boundedInt(state, QStringLiteral("leftSortRole"), 0, 0, 5),
        boundedInt(state, QStringLiteral("leftSortOrder"), int(Qt::AscendingOrder), 0, 1));
    m_workspace.rightPanel()->setPanelSortPolicy(
        boundedInt(state, QStringLiteral("rightSortRole"), 0, 0, 5),
        boundedInt(state, QStringLiteral("rightSortOrder"), int(Qt::AscendingOrder), 0, 1));
    m_workspace.leftPanel()->directoryModel()->setMixFilesAndFolders(
        state.value(QStringLiteral("leftMixFilesAndFolders"), false).toBool());
    m_workspace.rightPanel()->directoryModel()->setMixFilesAndFolders(
        state.value(QStringLiteral("rightMixFilesAndFolders"), false).toBool());

    const bool aggressiveStartupOpen = m_settings.shellFirstQmlRestore();
    const QString leftPath = m_settings.safeFolderPath(state.value(QStringLiteral("leftPath")).toString());
    if (aggressiveStartupOpen) {
        m_workspace.leftPanel()->openStartupRestoredFolder(leftPath);
    } else {
        m_workspace.leftPanel()->openPath(leftPath);
    }
    const QString rightPath = m_settings.safeFolderPath(state.value(QStringLiteral("rightPath")).toString());
    if (aggressiveStartupOpen) {
        m_workspace.rightPanel()->openStartupRestoredFolder(rightPath);
    } else {
        m_workspace.rightPanel()->openPath(rightPath);
    }
    m_workspace.setActivePanel(m_workspace.splitEnabled()
        ? boundedInt(state, QStringLiteral("activePanel"), 0, 0, 1)
        : 0);
}
