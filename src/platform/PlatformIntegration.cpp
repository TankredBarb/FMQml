#include "PlatformIntegration.h"

#include "../app/AppServices.h"

PlatformIntegration::PlatformIntegration(QObject *parent)
    : QObject(parent)
{
}

void PlatformIntegration::attach(QWindow *window, AppServices *services)
{
#ifdef Q_OS_WIN
    if (!services) {
        return;
    }
    m_taskbarProgress.attachWindow(window);
    m_taskbarProgress.setOperationQueue(services->workspace()->operationQueue());
    m_taskbarProgress.setIsoMountManager(services->workspace()->isoMountManager());
#else
    Q_UNUSED(window);
    Q_UNUSED(services);
#endif
}
