#include "SystemInfoProvider.h"
#include <QSysInfo>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <QSettings>
#endif

SystemInfoProvider::SystemInfoProvider(QObject *parent)
    : QObject(parent)
{
    m_osName = QSysInfo::prettyProductName();
    m_computerName = QSysInfo::machineHostName();
    m_cpuArchitecture = QSysInfo::currentCpuArchitecture();
    m_startTime = QDateTime::currentSecsSinceEpoch();
    m_cpuCores = QThread::idealThreadCount();

#ifdef Q_OS_WIN
    // CPU Name from registry
    QSettings settings("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", QSettings::NativeFormat);
    m_cpuName = settings.value("ProcessorNameString").toString().trimmed();

    // RAM total
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        m_totalRamGB = statex.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
    } else {
        m_totalRamGB = 16.0;
    }
#else
    m_cpuName = "Generic Processor";
    m_totalRamGB = 16.0;
#endif

    m_timer = new QTimer(this);
    m_timer->setInterval(2000);
    connect(m_timer, &QTimer::timeout, this, &SystemInfoProvider::updateStats);
    m_timer->start();

    // Initial update
    updateStats();
}

QString SystemInfoProvider::uptime() const
{
    qint64 current = QDateTime::currentSecsSinceEpoch();
    qint64 diff = current - m_startTime;
    qint64 hours = diff / 3600;
    qint64 minutes = (diff % 3600) / 60;
    qint64 seconds = diff % 60;
    if (hours > 0) {
        return QString("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds);
    }
    if (minutes > 0) {
        return QString("%1m %2s").arg(minutes).arg(seconds);
    }
    return QString("%1s").arg(seconds);
}

#ifdef Q_OS_WIN
static double getCpuUsageWin() {
    static FILETIME prevIdleTime = {0,0};
    static FILETIME prevKernelTime = {0,0};
    static FILETIME prevUserTime = {0,0};

    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return 0.0;

    auto fileTimeToQuad = [](const FILETIME &ft) {
        ULARGE_INTEGER u;
        u.LowPart = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return u.QuadPart;
    };

    double usage = 0.05; // default fallback if diff is 0

    if (prevIdleTime.dwLowDateTime != 0 || prevIdleTime.dwHighDateTime != 0) {
        uint64_t idle = fileTimeToQuad(idleTime) - fileTimeToQuad(prevIdleTime);
        uint64_t kernel = fileTimeToQuad(kernelTime) - fileTimeToQuad(prevKernelTime);
        uint64_t user = fileTimeToQuad(userTime) - fileTimeToQuad(prevUserTime);
        uint64_t system = kernel + user;

        if (system > 0) {
            usage = static_cast<double>(system - idle) / system;
        }
    }

    prevIdleTime = idleTime;
    prevKernelTime = kernelTime;
    prevUserTime = userTime;

    return usage;
}
#endif

void SystemInfoProvider::updateStats()
{
    double newCpu = 0.05;
    double newRam = 0.45;
    double newUsedRamGB = m_totalRamGB * 0.45;

#ifdef Q_OS_WIN
    // CPU
    newCpu = getCpuUsageWin();

    // RAM
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        newRam = statex.dwMemoryLoad / 100.0;
        newUsedRamGB = (statex.ullTotalPhys - statex.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
    }
#else
    // Fallback/mock logic for other platforms
    newCpu = 0.15;
    newRam = 0.42;
    newUsedRamGB = m_totalRamGB * 0.42;
#endif

    // Bounds checking
    if (newCpu < 0.0) newCpu = 0.0;
    if (newCpu > 1.0) newCpu = 1.0;
    if (newRam < 0.0) newRam = 0.0;
    if (newRam > 1.0) newRam = 1.0;

    if (qAbs(m_cpuUsage - newCpu) > 0.01) {
        m_cpuUsage = newCpu;
        emit cpuUsageChanged();
    }
    if (qAbs(m_ramUsage - newRam) > 0.01 || qAbs(m_usedRamGB - newUsedRamGB) > 0.05) {
        m_ramUsage = newRam;
        m_usedRamGB = newUsedRamGB;
        emit ramUsageChanged();
    }
    emit uptimeChanged();
}
