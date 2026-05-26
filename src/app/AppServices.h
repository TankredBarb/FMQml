#pragma once

#include <QObject>

#include "../controllers/PropertiesController.h"
#include "../controllers/QuickLookController.h"
#include "../controllers/ThemeController.h"
#include "../controllers/WorkspaceController.h"
#include "../core/SystemInfoProvider.h"

class AppServices final : public QObject {
    Q_OBJECT

public:
    explicit AppServices(QObject *parent = nullptr);

    WorkspaceController *workspace();
    ThemeController *theme();
    QuickLookController *quickLook();
    PropertiesController *properties();
    SystemInfoProvider *systemInfo();

public slots:
    void shutdown();

private:
    WorkspaceController m_workspace;
    ThemeController m_theme;
    QuickLookController m_quickLook;
    PropertiesController m_properties;
    SystemInfoProvider m_systemInfo;
};
