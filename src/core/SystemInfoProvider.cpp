#include "SystemInfoProvider.h"
#include <QSysInfo>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QStringList>

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
    // CPU Name from /proc/cpuinfo
    QFile cpuInfo(QLatin1String("/proc/cpuinfo"));
    if (cpuInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&cpuInfo);
        QString line;
        while (stream.readLineInto(&line)) {
            if (line.startsWith(QLatin1String("model name"), Qt::CaseInsensitive)) {
                int colonIdx = line.indexOf(QLatin1Char(':'));
                if (colonIdx != -1) {
                    m_cpuName = line.mid(colonIdx + 1).trimmed();
                    break;
                }
            }
        }
        cpuInfo.close();
    }
    if (m_cpuName.isEmpty()) {
        m_cpuName = QLatin1String("Generic Processor");
    }

    // RAM total from /proc/meminfo
    quint64 memTotalKB = 0;
    QFile memInfo(QLatin1String("/proc/meminfo"));
    if (memInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&memInfo);
        QString line;
        while (stream.readLineInto(&line)) {
            if (line.startsWith(QLatin1String("MemTotal:"), Qt::CaseInsensitive)) {
                QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    memTotalKB = parts[1].toULongLong();
                }
                break;
            }
        }
        memInfo.close();
    }
    if (memTotalKB > 0) {
        m_totalRamGB = memTotalKB / (1024.0 * 1024.0);
    } else {
        m_totalRamGB = 16.0;
    }
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

#ifndef Q_OS_WIN
static double getCpuUsageLinux() {
    static quint64 prevUser = 0, prevNice = 0, prevSystem = 0, prevIdle = 0,
                   prevIowait = 0, prevIrq = 0, prevSoftirq = 0, prevSteal = 0;

    QFile statFile(QLatin1String("/proc/stat"));
    if (!statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0.15; // fallback
    }

    QTextStream stream(&statFile);
    QString line = stream.readLine();
    statFile.close();

    if (!line.startsWith(QLatin1String("cpu "))) {
        return 0.15;
    }

    QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 5) {
        return 0.15;
    }

    quint64 user = parts[1].toULongLong();
    quint64 nice = parts[2].toULongLong();
    quint64 system = parts[3].toULongLong();
    quint64 idle = parts[4].toULongLong();
    quint64 iowait = parts.size() > 5 ? parts[5].toULongLong() : 0;
    quint64 irq = parts.size() > 6 ? parts[6].toULongLong() : 0;
    quint64 softirq = parts.size() > 7 ? parts[7].toULongLong() : 0;
    quint64 steal = parts.size() > 8 ? parts[8].toULongLong() : 0;

    quint64 prevTotalIdle = prevIdle + prevIowait;
    quint64 totalIdle = idle + iowait;

    quint64 prevNonIdle = prevUser + prevNice + prevSystem + prevIrq + prevSoftirq + prevSteal;
    quint64 nonIdle = user + nice + system + irq + softirq + steal;

    quint64 prevTotal = prevTotalIdle + prevNonIdle;
    quint64 total = totalIdle + nonIdle;

    double usage = 0.15;

    if (prevTotal != 0) {
        quint64 totalDiff = total - prevTotal;
        quint64 idleDiff = totalIdle - prevTotalIdle;
        if (totalDiff > 0) {
            usage = static_cast<double>(totalDiff - idleDiff) / totalDiff;
        }
    }

    prevUser = user;
    prevNice = nice;
    prevSystem = system;
    prevIdle = idle;
    prevIowait = iowait;
    prevIrq = irq;
    prevSoftirq = softirq;
    prevSteal = steal;

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
    // CPU usage from /proc/stat
    newCpu = getCpuUsageLinux();

    // RAM usage from /proc/meminfo
    quint64 memTotalKB = 0;
    quint64 memAvailableKB = 0;
    QFile memInfo(QLatin1String("/proc/meminfo"));
    if (memInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&memInfo);
        QString line;
        while (stream.readLineInto(&line)) {
            if (line.startsWith(QLatin1String("MemTotal:"), Qt::CaseInsensitive)) {
                QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    memTotalKB = parts[1].toULongLong();
                }
            } else if (line.startsWith(QLatin1String("MemAvailable:"), Qt::CaseInsensitive)) {
                QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    memAvailableKB = parts[1].toULongLong();
                }
            }
        }
        memInfo.close();
    }

    if (memTotalKB > 0 && memAvailableKB > 0 && memTotalKB >= memAvailableKB) {
        newUsedRamGB = (memTotalKB - memAvailableKB) / (1024.0 * 1024.0);
        newRam = static_cast<double>(memTotalKB - memAvailableKB) / memTotalKB;
    } else {
        newCpu = 0.15;
        newRam = 0.42;
        newUsedRamGB = m_totalRamGB * 0.42;
    }
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
