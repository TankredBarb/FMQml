#include "DriveUtils.h"

#ifdef Q_OS_WIN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#include <windows.h>
#include <winioctl.h>
#endif

#include <QDir>
#include <QtGlobal>

namespace DriveUtils {

#ifdef Q_OS_WIN
static QString detectFixedDriveType(const QString &root)
{
    const QString driveLetter = root.left(1);
    if (driveLetter.isEmpty() || driveLetter == QChar('/'))
        return QStringLiteral("ssd");

    const QString physicalPath = QStringLiteral("\\\\.\\%1:").arg(driveLetter);
    const HANDLE hDevice = CreateFileW(
        physicalPath.toStdWString().c_str(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hDevice == INVALID_HANDLE_VALUE)
        return QStringLiteral("ssd");

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;

    DEVICE_SEEK_PENALTY_DESCRIPTOR seekDesc{};
    DWORD bytesReturned = 0;
    const BOOL result = DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
                                        &query, sizeof(query),
                                        &seekDesc, sizeof(seekDesc),
                                        &bytesReturned, nullptr);

    QString detected = QStringLiteral("ssd");
    if (result) {
        detected = seekDesc.IncursSeekPenalty ? QStringLiteral("hdd") : QStringLiteral("ssd");
    }

    // Check bus type for NVMe detection
    query.PropertyId = StorageAdapterProperty;
    query.QueryType = PropertyStandardQuery;
    STORAGE_ADAPTER_DESCRIPTOR adapterDesc{};
    if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
                        &query, sizeof(query),
                        &adapterDesc, sizeof(adapterDesc),
                        &bytesReturned, nullptr)
        && static_cast<int>(adapterDesc.BusType) == 17) {
        detected = QStringLiteral("nvme");
    }

    CloseHandle(hDevice);
    return detected;
}
#endif

QString detectDriveType(const QStorageInfo &info)
{
#ifdef Q_OS_WIN
    const QString root = info.rootPath();
    const QString native = root.endsWith('/') || root.endsWith('\\')
                               ? root
                               : root + QStringLiteral("\\");
    const std::wstring wpath = native.toStdWString();
    const UINT driveType = ::GetDriveTypeW(wpath.c_str());

    switch (driveType) {
    case DRIVE_REMOVABLE:
        return QStringLiteral("usb");
    case DRIVE_FIXED:
        return detectFixedDriveType(root);
    case DRIVE_REMOTE:
        return QStringLiteral("network");
    case DRIVE_CDROM:
        return QStringLiteral("optical");
    case DRIVE_RAMDISK:
        return QStringLiteral("ssd");
    default:
        break;
    }
#else
    Q_UNUSED(info)
#endif
    return QStringLiteral("hdd");
}

QString rootDisplayName(const QString &rootPath)
{
    QString normalized = QDir::fromNativeSeparators(rootPath).trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

#ifdef Q_OS_WIN
    if (normalized.size() >= 2 && normalized.at(1) == QLatin1Char(':')) {
        return normalized.left(2).toUpper();
    }
#endif

    if (normalized == QLatin1String("/")) {
        return normalized;
    }
    while (normalized.size() > 1 && normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    return QDir::toNativeSeparators(normalized);
}

QString formatSize(qint64 bytes)
{
    if (bytes < 0) {
        return QStringLiteral("—");
    }

    constexpr qint64 KB = 1024LL;
    constexpr qint64 MB = 1024LL * KB;
    constexpr qint64 GB = 1024LL * MB;
    constexpr qint64 TB = 1024LL * GB;

    if (bytes >= TB) {
        double val = static_cast<double>(bytes) / static_cast<double>(TB);
        return QStringLiteral("%1 TB").arg(val, 0, 'f', val < 10.0 ? 2 : (val < 100.0 ? 1 : 0));
    }
    if (bytes >= GB) {
        double val = static_cast<double>(bytes) / static_cast<double>(GB);
        return QStringLiteral("%1 GB").arg(val, 0, 'f', val < 10.0 ? 2 : (val < 100.0 ? 1 : 0));
    }
    if (bytes >= MB) {
        double val = static_cast<double>(bytes) / static_cast<double>(MB);
        return QStringLiteral("%1 MB").arg(val, 0, 'f', val < 10.0 ? 1 : 0);
    }
    if (bytes >= KB) {
        double val = static_cast<double>(bytes) / static_cast<double>(KB);
        return QStringLiteral("%1 KB").arg(val, 0, 'f', 0);
    }
    return QStringLiteral("%1 B").arg(bytes);
}

} // namespace DriveUtils
