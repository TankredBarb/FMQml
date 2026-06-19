#pragma once

#include <QObject>

#ifdef Q_OS_WIN
#include "WindowsTaskbarProgress.h"
#endif
#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
#include "LinuxTaskbarProgress.h"
#endif

class AppServices;
class QWindow;

class PlatformIntegration final : public QObject {
    Q_OBJECT

public:
    explicit PlatformIntegration(QObject *parent = nullptr);

    void attach(QWindow *window, AppServices *services);

private:
#ifdef Q_OS_WIN
    WindowsTaskbarProgress m_taskbarProgress;
#endif
#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
    LinuxTaskbarProgress m_taskbarProgress;
#endif
};
