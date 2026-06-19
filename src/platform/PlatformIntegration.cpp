#include "PlatformIntegration.h"

#include "../app/AppServices.h"

PlatformIntegration::PlatformIntegration(QObject *parent)
    : QObject(parent)
{
}

void PlatformIntegration::attach(QWindow *window, AppServices *services)
{
#if defined(Q_OS_WIN) || (defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN))
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
