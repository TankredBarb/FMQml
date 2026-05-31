#include "AppServices.h"

#include "../core/ArchiveSupport.h"
#include "../core/IsoSupport.h"
#include "../core/LocalFileProvider.h"
#include "../models/DirectoryModel.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <Qt>
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
}

AppServices::AppServices(QObject *parent)
    : QObject(parent)
{
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
        if (QFileInfo(path).isFile()
            && !(ArchiveSupport::archiveBackendAvailable() && ArchiveSupport::isArchiveFilePath(path))
            && !IsoSupport::isIsoImagePath(path)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            return;
        }

        FilePanelController *panel = m_workspace.activePanel() == 0
            ? m_workspace.leftPanel()
            : m_workspace.rightPanel();
        panel->openPath(path);
    });
    connect(m_workspace.leftPanel(), &FilePanelController::pathNavigated, &m_favorites, &FavoritesController::recordVisit);
    connect(m_workspace.rightPanel(), &FilePanelController::pathNavigated, &m_favorites, &FavoritesController::recordVisit);
    restoreInitialWorkspaceState();
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
