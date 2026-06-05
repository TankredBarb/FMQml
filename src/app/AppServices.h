#pragma once

#include <QObject>
#include <Qt>

#include "../controllers/AdminController.h"
#include "../controllers/AppSettingsController.h"
#include "../controllers/DiskUsageController.h"
#include "../controllers/FavoritesController.h"
#include "../controllers/FileSearchController.h"
#include "../controllers/PropertiesController.h"
#include "../controllers/QuickLookController.h"
#include "../controllers/SystemTrayController.h"
#include "../controllers/ThemeController.h"
#include "../controllers/WorkspaceController.h"
#include "../core/SystemInfoProvider.h"
#include "../core/FileTypeIconResolver.h"

class AppServices final : public QObject {
    Q_OBJECT

public:
    explicit AppServices(QObject *parent = nullptr);

    WorkspaceController *workspace();
    ThemeController *theme();
    QuickLookController *quickLook();
    PropertiesController *properties();
    SystemInfoProvider *systemInfo();
    DiskUsageController *diskUsage();
    FileSearchController *fileSearch();
    AppSettingsController *settings();
    AdminController *admin();
    FavoritesController *favorites();
    FileTypeIconResolver *fileTypeIcons();
    SystemTrayController *systemTray();

public slots:
    void shutdown();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void restoreInitialWorkspaceState();
    bool canHandlePanelTransferShortcut(int key, Qt::KeyboardModifiers modifiers);
    bool handlePanelTransferShortcut(int key, Qt::KeyboardModifiers modifiers);

    WorkspaceController m_workspace;
    ThemeController m_theme;
    QuickLookController m_quickLook;
    PropertiesController m_properties;
    SystemInfoProvider m_systemInfo;
    DiskUsageController m_diskUsage;
    FileSearchController m_fileSearch;
    AppSettingsController m_settings;
    AdminController m_admin;
    FavoritesController m_favorites;
    FileTypeIconResolver m_fileTypeIcons;
    SystemTrayController m_systemTray;
};
