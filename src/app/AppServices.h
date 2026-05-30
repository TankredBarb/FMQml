#pragma once

#include <QObject>

#include "../controllers/AdminController.h"
#include "../controllers/AppSettingsController.h"
#include "../controllers/FavoritesController.h"
#include "../controllers/PropertiesController.h"
#include "../controllers/QuickLookController.h"
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
    AppSettingsController *settings();
    AdminController *admin();
    FavoritesController *favorites();
    FileTypeIconResolver *fileTypeIcons();

public slots:
    void shutdown();

private:
    void restoreInitialWorkspaceState();

    WorkspaceController m_workspace;
    ThemeController m_theme;
    QuickLookController m_quickLook;
    PropertiesController m_properties;
    SystemInfoProvider m_systemInfo;
    AppSettingsController m_settings;
    AdminController m_admin;
    FavoritesController m_favorites;
    FileTypeIconResolver m_fileTypeIcons;
};
