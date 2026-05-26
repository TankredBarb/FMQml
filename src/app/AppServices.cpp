#include "AppServices.h"

AppServices::AppServices(QObject *parent)
    : QObject(parent)
{
    m_quickLook.setIsoMountManager(m_workspace.isoMountManager());
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

void AppServices::shutdown()
{
    m_workspace.isoMountManager()->unmountAll();
}
