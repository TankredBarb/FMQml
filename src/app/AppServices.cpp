#include "AppServices.h"

#include "../core/ArchiveSupport.h"
#include "../core/IsoSupport.h"
#include "../core/LocalFileProvider.h"
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
}

AppServices::AppServices(QObject *parent)
    : QObject(parent)
{
    qApp->installEventFilter(this);
    m_quickLook.setIsoMountManager(m_workspace.isoMountManager());
    m_favorites.setIsoMountManager(m_workspace.isoMountManager());
    m_settings.setThemeController(&m_theme);
    LocalFileProvider::setUseNativeFileEnumerators(m_settings.useNativeFileEnumerators());
    connect(&m_settings, &AppSettingsController::useNativeFileEnumeratorsChanged, this, [this]() {
        LocalFileProvider::setUseNativeFileEnumerators(m_settings.useNativeFileEnumerators());
        m_workspace.leftPanel()->refresh();
        m_workspace.rightPanel()->refresh();
        m_workspace.treeModel()->refresh();
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
    restoreInitialWorkspaceState();
}

bool AppServices::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
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
        const QVariant shortcutsEnabled = window->property("panelShortcutsEnabled");
        if (shortcutsEnabled.isValid() && !shortcutsEnabled.toBool()) {
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

FileTypeIconResolver *AppServices::fileTypeIcons()
{
    return &m_fileTypeIcons;
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

    m_workspace.leftPanel()->directoryModel()->setSortRole(
        static_cast<DirectoryModel::SortRole>(boundedInt(state, QStringLiteral("leftSortRole"), 0, 0, 5)));
    m_workspace.rightPanel()->directoryModel()->setSortRole(
        static_cast<DirectoryModel::SortRole>(boundedInt(state, QStringLiteral("rightSortRole"), 0, 0, 5)));
    m_workspace.leftPanel()->directoryModel()->setSortOrder(
        boundedInt(state, QStringLiteral("leftSortOrder"), int(Qt::AscendingOrder), 0, 1) == int(Qt::DescendingOrder)
            ? Qt::DescendingOrder
            : Qt::AscendingOrder);
    m_workspace.rightPanel()->directoryModel()->setSortOrder(
        boundedInt(state, QStringLiteral("rightSortOrder"), int(Qt::AscendingOrder), 0, 1) == int(Qt::DescendingOrder)
            ? Qt::DescendingOrder
            : Qt::AscendingOrder);

    m_workspace.leftPanel()->openPath(m_settings.safeFolderPath(state.value(QStringLiteral("leftPath")).toString()));
    m_workspace.rightPanel()->openPath(m_settings.safeFolderPath(state.value(QStringLiteral("rightPath")).toString()));
    m_workspace.setActivePanel(m_workspace.splitEnabled()
        ? boundedInt(state, QStringLiteral("activePanel"), 0, 0, 1)
        : 0);
}
