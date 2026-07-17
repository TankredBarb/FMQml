#include "DriveUtils.h"

#ifdef Q_OS_WIN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#include <windows.h>
#include <winioctl.h>
#endif

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSet>
#include <QUrl>
#include <QtGlobal>

#include <optional>

namespace DriveUtils {

namespace {

bool displayTokenEquals(QString lhs, QString rhs)
{
    lhs = QDir::fromNativeSeparators(lhs.trimmed());
    rhs = QDir::fromNativeSeparators(rhs.trimmed());
    while (lhs.size() > 1 && lhs.endsWith(QLatin1Char('/'))) {
        lhs.chop(1);
    }
    while (rhs.size() > 1 && rhs.endsWith(QLatin1Char('/'))) {
        rhs.chop(1);
    }
#ifdef Q_OS_WIN
    return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
#else
    return lhs == rhs;
#endif
}

QString volumeLabel(const QStorageInfo &info)
{
    if (info.rootPath().trimmed().isEmpty()) {
        return info.displayName().trimmed();
    }

#ifdef Q_OS_WIN
    QString nativeRoot = QDir::toNativeSeparators(info.rootPath());
    if (!nativeRoot.endsWith(QLatin1Char('\\'))) {
        nativeRoot += QLatin1Char('\\');
    }

    wchar_t volumeName[MAX_PATH + 1] = {};
    if (GetVolumeInformationW(reinterpret_cast<LPCWSTR>(nativeRoot.utf16()),
                              volumeName,
                              MAX_PATH + 1,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr,
                              0)) {
        return QString::fromWCharArray(volumeName).trimmed();
    }
#endif
    return info.displayName().trimmed();
}

} // namespace

#ifndef Q_OS_WIN
QString linuxBlockDeviceName(const QStorageInfo &info)
{
    QString devicePath = QFile::decodeName(info.device()).trimmed();
    if (devicePath.isEmpty() || !devicePath.startsWith(QLatin1Char('/'))) {
        return {};
    }

    QFileInfo deviceInfo(devicePath);
    const QString canonical = deviceInfo.canonicalFilePath();
    if (!canonical.isEmpty()) {
        devicePath = canonical;
    }
    return QFileInfo(devicePath).fileName();
}

QString linuxBlockDeviceSysPath(const QString &deviceName)
{
    if (deviceName.isEmpty()) {
        return {};
    }
    const QFileInfo sysInfo(QStringLiteral("/sys/class/block/%1").arg(deviceName));
    return sysInfo.exists() ? sysInfo.canonicalFilePath() : QString();
}

QString linuxImmediateParentBlockDeviceName(const QString &deviceName)
{
    const QString sysPath = linuxBlockDeviceSysPath(deviceName);
    if (sysPath.isEmpty()) {
        return {};
    }

    const QString parentName = QFileInfo(QFileInfo(sysPath).absolutePath()).fileName();
    return parentName == deviceName ? QString() : parentName;
}

std::optional<int> linuxReadBlockIntAttribute(const QString &deviceName, const QString &relativePath)
{
    if (deviceName.isEmpty()) {
        return std::nullopt;
    }

    QFile file(QStringLiteral("/sys/class/block/%1/%2").arg(deviceName, relativePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString parentName = linuxImmediateParentBlockDeviceName(deviceName);
        if (!parentName.isEmpty()) {
            return linuxReadBlockIntAttribute(parentName, relativePath);
        }
        return std::nullopt;
    }

    bool ok = false;
    const int value = QString::fromLatin1(file.readAll()).trimmed().toInt(&ok);
    return ok ? std::optional<int>(value) : std::nullopt;
}

QString linuxParentPhysicalBlockDeviceNameImpl(const QString &deviceName, QSet<QString> *visited)
{
    if (deviceName.isEmpty() || visited->contains(deviceName)) {
        return {};
    }
    visited->insert(deviceName);

    const QString sysPath = linuxBlockDeviceSysPath(deviceName);
    if (sysPath.isEmpty()) {
        return {};
    }

    const QDir slavesDir(QStringLiteral("/sys/class/block/%1/slaves").arg(deviceName));
    const QStringList slaves = slavesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (slaves.size() == 1) {
        return linuxParentPhysicalBlockDeviceNameImpl(slaves.constFirst(), visited);
    }
    if (slaves.size() > 1 || sysPath.contains(QStringLiteral("/virtual/"))) {
        return {};
    }

    if (QFileInfo(QStringLiteral("/sys/class/block/%1/partition").arg(deviceName)).exists()) {
        const QString parent = linuxImmediateParentBlockDeviceName(deviceName);
        return parent.isEmpty() ? QString() : linuxParentPhysicalBlockDeviceNameImpl(parent, visited);
    }
    return deviceName;
}

QString linuxParentPhysicalBlockDeviceName(const QString &deviceName)
{
    QSet<QString> visited;
    return linuxParentPhysicalBlockDeviceNameImpl(deviceName, &visited);
}

bool linuxBlockDeviceUsesUsbBus(const QString &deviceName)
{
    const QString sysPath = linuxBlockDeviceSysPath(deviceName);
    return sysPath.contains(QStringLiteral("/usb"), Qt::CaseInsensitive);
}

bool linuxFileSystemIsNetwork(const QByteArray &fileSystem)
{
    const QString fs = QString::fromLatin1(fileSystem).toLower();
    return fs == QLatin1String("nfs")
        || fs == QLatin1String("nfs4")
        || fs == QLatin1String("cifs")
        || fs == QLatin1String("smb3")
        || fs == QLatin1String("sshfs")
        || fs == QLatin1String("fuse.sshfs")
        || fs == QLatin1String("davfs")
        || fs == QLatin1String("fuse.davfs");
}

bool linuxFileSystemIsOptical(const QByteArray &fileSystem)
{
    const QString fs = QString::fromLatin1(fileSystem).toLower();
    return fs == QLatin1String("iso9660") || fs == QLatin1String("udf");
}
#endif

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
    if (linuxFileSystemIsNetwork(info.fileSystemType())) {
        return QStringLiteral("network");
    }
    if (linuxFileSystemIsOptical(info.fileSystemType())) {
        return QStringLiteral("optical");
    }

    const QString deviceName = linuxBlockDeviceName(info);
    if (linuxBlockDeviceUsesUsbBus(deviceName)) {
        return QStringLiteral("usb");
    }

    const std::optional<int> removable = linuxReadBlockIntAttribute(deviceName, QStringLiteral("removable"));
    if (removable && *removable == 1) {
        return QStringLiteral("usb");
    }

    const std::optional<int> rotational = linuxReadBlockIntAttribute(deviceName, QStringLiteral("queue/rotational"));
    if (rotational) {
        return *rotational == 0 ? QStringLiteral("ssd") : QStringLiteral("hdd");
    }
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

QString displayPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    QString localPath = trimmed;
    if (trimmed.contains(QStringLiteral("://"))) {
        const QUrl url(trimmed);
        if (url.isValid() && !url.scheme().isEmpty() && url.scheme() != QLatin1String("file")) {
            return trimmed;
        }
        if (url.isValid() && url.isLocalFile()) {
            localPath = url.toLocalFile();
        }
    }

    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(localPath));
    if (normalized.isEmpty()) {
        return {};
    }

#ifndef Q_OS_WIN
    const QString home = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    if (!home.isEmpty()) {
        if (normalized == home) {
            return QStringLiteral("~/");
        }
        const QString homePrefix = home.endsWith(QLatin1Char('/')) ? home : home + QLatin1Char('/');
        if (normalized.startsWith(homePrefix)) {
            return QStringLiteral("~/") + normalized.mid(homePrefix.size());
        }
    }
#endif

    return QDir::toNativeSeparators(normalized);
}

QString volumeDisplayName(const QStorageInfo &info)
{
    const QString rootName = rootDisplayName(info.rootPath());
    const QString label = volumeLabel(info);
    if (label.isEmpty()
        || displayTokenEquals(label, rootName)
        || displayTokenEquals(label, info.rootPath())) {
        return rootName.isEmpty() ? info.rootPath() : rootName;
    }
    if (rootName.isEmpty()) {
        return label;
    }
    return QStringLiteral("%1 %2").arg(rootName, label);
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
