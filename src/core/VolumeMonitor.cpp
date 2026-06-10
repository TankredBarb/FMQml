#include "VolumeMonitor.h"

#include "ArchiveSupport.h"
#include "DriveUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QStorageInfo>
#include <QtConcurrent/QtConcurrentRun>
#include <QSet>
#include <algorithm>
#include <utility>
#include <vector>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cfgmgr32.h>
#include <dbt.h>
#include <setupapi.h>
#include <winioctl.h>
#endif

namespace {

#ifdef Q_OS_WIN
QString windowsErrorMessage(DWORD error)
{
    wchar_t *messageBuffer = nullptr;
    const DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&messageBuffer),
        0,
        nullptr);

    QString message = size > 0 && messageBuffer
        ? QString::fromWCharArray(messageBuffer, static_cast<int>(size)).trimmed()
        : QStringLiteral("Windows error %1").arg(error);

    if (messageBuffer) {
        LocalFree(messageBuffer);
    }
    return message;
}

UINT windowsDriveType(const QString &rootPath)
{
    QString native = QDir::toNativeSeparators(rootPath);
    if (!native.endsWith(QLatin1Char('\\'))) {
        native += QLatin1Char('\\');
    }
    return GetDriveTypeW(reinterpret_cast<LPCWSTR>(native.utf16()));
}

QString normalizedDriveLetterRoot(const QString &rootPath)
{
    const QString comparable = VolumeMonitor::volumeKeyForRoot(rootPath);
    if (comparable.size() >= 3 && comparable.at(1) == QLatin1Char(':')) {
        return comparable.left(3);
    }
    return {};
}

struct EjectResult {
    bool success = false;
    QString message;
    bool attempted = true;
    bool vetoed = false;
};

QString configManagerErrorMessage(CONFIGRET result)
{
    if (result == CR_REMOVE_VETOED) {
        return QStringLiteral("The device eject was vetoed by Windows.");
    }
    return QStringLiteral("Configuration Manager error %1").arg(result);
}

QString vetoMessage(PNP_VETO_TYPE vetoType, const wchar_t *vetoName)
{
    QString message = QStringLiteral("The device eject was vetoed by Windows");
    const QString name = vetoName && vetoName[0] != L'\0'
        ? QString::fromWCharArray(vetoName)
        : QString();
    if (!name.isEmpty()) {
        message += QStringLiteral(" (%1)").arg(name);
    }
    if (vetoType != PNP_VetoTypeUnknown) {
        message += QStringLiteral(".");
    }
    return message;
}

bool storageDeviceNumberForHandle(HANDLE device, STORAGE_DEVICE_NUMBER *number)
{
    if (!number || device == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD bytesReturned = 0;
    return DeviceIoControl(device,
                           IOCTL_STORAGE_GET_DEVICE_NUMBER,
                           nullptr,
                           0,
                           number,
                           sizeof(*number),
                           &bytesReturned,
                           nullptr) != FALSE;
}

EjectResult requestEjectForDevInst(DEVINST devInst)
{
    PNP_VETO_TYPE vetoType = PNP_VetoTypeUnknown;
    wchar_t vetoName[MAX_PATH] = {};
    const CONFIGRET result = CM_Request_Device_EjectW(devInst, &vetoType, vetoName, MAX_PATH, 0);
    if (result == CR_SUCCESS) {
        return {true, {}, true, false};
    }
    if (result == CR_REMOVE_VETOED) {
        return {false, vetoMessage(vetoType, vetoName), true, true};
    }
    return {false, configManagerErrorMessage(result), true, false};
}

EjectResult ejectVolumeWithConfigManager(const QString &rootPath)
{
    const QString root = normalizedDriveLetterRoot(rootPath);
    if (root.isEmpty()) {
        return {false, {}, false, false};
    }

    const QChar letter = root.at(0).toUpper();
    const QString volumePath = QStringLiteral("\\\\.\\%1:").arg(letter);
    const HANDLE volume = CreateFileW(reinterpret_cast<LPCWSTR>(volumePath.utf16()),
                                      0,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr,
                                      OPEN_EXISTING,
                                      0,
                                      nullptr);
    if (volume == INVALID_HANDLE_VALUE) {
        return {false, {}, false, false};
    }

    STORAGE_DEVICE_NUMBER volumeNumber{};
    const bool hasVolumeNumber = storageDeviceNumberForHandle(volume, &volumeNumber);
    CloseHandle(volume);
    if (!hasVolumeNumber) {
        return {false, {}, false, false};
    }

    HDEVINFO deviceInfo = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK,
                                               nullptr,
                                               nullptr,
                                               DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        return {false, {}, false, false};
    }

    EjectResult result{false, {}, false, false};
    for (DWORD index = 0; ; ++index) {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(deviceInfo, nullptr, &GUID_DEVINTERFACE_DISK, index, &interfaceData)) {
            break;
        }

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);
        if (requiredSize == 0) {
            continue;
        }

        std::vector<BYTE> detailBuffer(requiredSize);
        auto *detailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(detailBuffer.data());
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA devInfoData{};
        devInfoData.cbSize = sizeof(devInfoData);
        if (!SetupDiGetDeviceInterfaceDetailW(deviceInfo,
                                              &interfaceData,
                                              detailData,
                                              requiredSize,
                                              nullptr,
                                              &devInfoData)) {
            continue;
        }

        const HANDLE disk = CreateFileW(detailData->DevicePath,
                                        0,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        nullptr,
                                        OPEN_EXISTING,
                                        0,
                                        nullptr);
        if (disk == INVALID_HANDLE_VALUE) {
            continue;
        }

        STORAGE_DEVICE_NUMBER diskNumber{};
        const bool hasDiskNumber = storageDeviceNumberForHandle(disk, &diskNumber);
        CloseHandle(disk);
        if (!hasDiskNumber || diskNumber.DeviceNumber != volumeNumber.DeviceNumber) {
            continue;
        }

        DEVINST target = devInfoData.DevInst;
        for (int attempt = 0; attempt < 3 && target != 0; ++attempt) {
            result = requestEjectForDevInst(target);
            if (result.success) {
                SetupDiDestroyDeviceInfoList(deviceInfo);
                return result;
            }

            DEVINST parent = 0;
            if (CM_Get_Parent(&parent, target, 0) != CR_SUCCESS || parent == target) {
                break;
            }
            target = parent;
        }

        break;
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);
    return result;
}

EjectResult ejectVolumeByHandle(const QString &rootPath)
{
    const QString root = normalizedDriveLetterRoot(rootPath);
    if (root.isEmpty()) {
        return {false, QStringLiteral("Only drive-letter volumes can be ejected."), true, false};
    }

    const QChar letter = root.at(0).toUpper();
    const QString devicePath = QStringLiteral("\\\\.\\%1:").arg(letter);
    const HANDLE volume = CreateFileW(reinterpret_cast<LPCWSTR>(devicePath.utf16()),
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr,
                                      OPEN_EXISTING,
                                      0,
                                      nullptr);

    if (volume == INVALID_HANDLE_VALUE) {
        return {false, windowsErrorMessage(GetLastError()), true, false};
    }

    auto closeAndReturn = [volume](bool success, const QString &message) {
        CloseHandle(volume);
        return EjectResult{success, message, true, false};
    };

    DWORD bytesReturned = 0;
    if (!DeviceIoControl(volume, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr)) {
        const DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) {
            return closeAndReturn(false, QStringLiteral("The device is busy or in use."));
        }
        return closeAndReturn(false, windowsErrorMessage(error));
    }

    if (!DeviceIoControl(volume, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr)) {
        const DWORD error = GetLastError();
        DeviceIoControl(volume, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
        return closeAndReturn(false, windowsErrorMessage(error));
    }

    PREVENT_MEDIA_REMOVAL removal{};
    removal.PreventMediaRemoval = FALSE;
    DeviceIoControl(volume,
                    IOCTL_STORAGE_MEDIA_REMOVAL,
                    &removal,
                    sizeof(removal),
                    nullptr,
                    0,
                    &bytesReturned,
                    nullptr);

    if (!DeviceIoControl(volume, IOCTL_STORAGE_EJECT_MEDIA, nullptr, 0, nullptr, 0, &bytesReturned, nullptr)) {
        const DWORD error = GetLastError();
        DeviceIoControl(volume, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
        return closeAndReturn(false, windowsErrorMessage(error));
    }

    return closeAndReturn(true, {});
}

EjectResult ejectVolumeNative(const QString &rootPath)
{
    const EjectResult configManagerResult = ejectVolumeWithConfigManager(rootPath);
    if (configManagerResult.success || configManagerResult.vetoed) {
        return configManagerResult;
    }

    const EjectResult handleResult = ejectVolumeByHandle(rootPath);
    if (!handleResult.success && !configManagerResult.message.isEmpty() && handleResult.message.isEmpty()) {
        return configManagerResult;
    }
    return handleResult;
}
#endif

bool isUriPath(const QString &path)
{
    const QString value = path.trimmed();
    const int index = value.indexOf(QStringLiteral("://"));
    if (index <= 0) {
        return false;
    }

    const QString scheme = value.left(index);
    if (scheme.isEmpty() || !scheme.at(0).isLetter()) {
        return false;
    }
    for (const QChar ch : scheme) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('+') && ch != QLatin1Char('.') && ch != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

} // namespace

VolumeMonitor::VolumeMonitor(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &VolumeMonitor::refreshNow);

    m_pollTimer.setInterval(5000);
    connect(&m_pollTimer, &QTimer::timeout, this, [this]() {
        scheduleRefresh(0, 0);
    });
    m_pollTimer.start();

    if (auto *app = QCoreApplication::instance()) {
        app->installNativeEventFilter(this);
    }

    refreshNow();
}

VolumeMonitor::~VolumeMonitor()
{
    if (auto *app = QCoreApplication::instance()) {
        app->removeNativeEventFilter(this);
    }
}

const QList<VolumeInfo> &VolumeMonitor::volumes() const
{
    return m_volumes;
}

bool VolumeMonitor::hasVolumeRoot(const QString &rootPath) const
{
    return m_volumesByKey.contains(volumeKeyForRoot(rootPath));
}

bool VolumeMonitor::isKnownEjectableRoot(const QString &rootPath) const
{
    const auto it = m_volumesByKey.constFind(volumeKeyForRoot(rootPath));
    return it != m_volumesByKey.cend() && it->isEjectable;
}

bool VolumeMonitor::isKnownReadyRoot(const QString &rootPath) const
{
    const auto it = m_volumesByKey.constFind(volumeKeyForRoot(rootPath));
    return it != m_volumesByKey.cend() && it->isReady;
}

QString VolumeMonitor::displayNameForRoot(const QString &rootPath) const
{
    const QString key = volumeKeyForRoot(rootPath);
    const auto it = m_volumesByKey.constFind(key);
    if (it != m_volumesByKey.cend()) {
        return it->displayName;
    }
    const auto removedIt = m_recentlyRemovedByKey.constFind(key);
    if (removedIt != m_recentlyRemovedByKey.cend()) {
        return removedIt->displayName;
    }
    const auto pendingIt = m_pendingRemovedByKey.constFind(key);
    if (pendingIt != m_pendingRemovedByKey.cend()) {
        return pendingIt->info.displayName;
    }
    QStorageInfo storage(rootPath);
    storage.refresh();
    const QString displayName = DriveUtils::volumeDisplayName(storage);
    return displayName.isEmpty() ? DriveUtils::rootDisplayName(rootPath) : displayName;
}

QString VolumeMonitor::rootForPath(const QString &path) const
{
    return rootForPathInMap(path, m_volumesByKey);
}

QString VolumeMonitor::recentlyRemovedRootForPath(const QString &path) const
{
    return rootForPathInMap(path, m_recentlyRemovedByKey);
}

QString VolumeMonitor::unavailableRootForPath(const QString &path) const
{
    QString root = rootForPathInMap(path, m_recentlyRemovedByKey);
    if (!root.isEmpty()) {
        return root;
    }

    QHash<QString, VolumeInfo> pendingByKey;
    for (auto it = m_pendingRemovedByKey.cbegin(); it != m_pendingRemovedByKey.cend(); ++it) {
        pendingByKey.insert(it.key(), it.value().info);
    }
    root = rootForPathInMap(path, pendingByKey);
    if (!root.isEmpty()) {
        return root;
    }

    for (auto it = m_volumesByKey.cbegin(); it != m_volumesByKey.cend(); ++it) {
        if (!it->isEjectable || it->isReady || !pathBelongsToRoot(path, it->rootPath)) {
            continue;
        }
        return it->rootPath;
    }

    return {};
}

bool VolumeMonitor::pathBelongsToRoot(const QString &path, const QString &rootPath) const
{
    const QString comparable = comparablePath(path);
    const QString root = volumeKeyForRoot(rootPath);
    if (comparable.isEmpty() || root.isEmpty()) {
        return false;
    }
    if (comparable.compare(root, Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QString prefix = root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
#ifdef Q_OS_WIN
    return comparable.startsWith(prefix, Qt::CaseInsensitive);
#else
    return comparable.startsWith(prefix);
#endif
}

#ifndef Q_OS_WIN
static QString normalizedLinuxMountPath(const QString &path)
{
    QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    if (normalized != QLatin1String("/") && normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    return normalized;
}

static bool pathIsDirectChildOf(const QString &path, const QString &parent)
{
    const QString normalizedPath = normalizedLinuxMountPath(path);
    QString normalizedParent = normalizedLinuxMountPath(parent);
    if (normalizedPath.isEmpty() || normalizedParent.isEmpty() || normalizedPath == normalizedParent) {
        return false;
    }
    if (!normalizedParent.endsWith(QLatin1Char('/'))) {
        normalizedParent += QLatin1Char('/');
    }
    const QString tail = normalizedPath.mid(normalizedParent.size());
    return normalizedPath.startsWith(normalizedParent) && !tail.isEmpty() && !tail.contains(QLatin1Char('/'));
}

static bool isLinuxNetworkFileSystem(const QString &fileSystem)
{
    static const QSet<QString> networkFileSystems = {
        QStringLiteral("nfs"),
        QStringLiteral("nfs4"),
        QStringLiteral("cifs"),
        QStringLiteral("smb3"),
        QStringLiteral("sshfs"),
        QStringLiteral("fuse.sshfs"),
        QStringLiteral("davfs"),
        QStringLiteral("fuse.davfs"),
    };
    return networkFileSystems.contains(fileSystem.toLower());
}

static bool isLinuxPseudoFileSystem(const QString &fileSystem)
{
    static const QSet<QString> pseudoFileSystems = {
        QStringLiteral("autofs"),
        QStringLiteral("binfmt_misc"),
        QStringLiteral("bpf"),
        QStringLiteral("cgroup"),
        QStringLiteral("cgroup2"),
        QStringLiteral("configfs"),
        QStringLiteral("debugfs"),
        QStringLiteral("devpts"),
        QStringLiteral("devtmpfs"),
        QStringLiteral("efivarfs"),
        QStringLiteral("fusectl"),
        QStringLiteral("fuse.portal"),
        QStringLiteral("hugetlbfs"),
        QStringLiteral("mqueue"),
        QStringLiteral("proc"),
        QStringLiteral("pstore"),
        QStringLiteral("securityfs"),
        QStringLiteral("sysfs"),
        QStringLiteral("tracefs"),
    };
    return pseudoFileSystems.contains(fileSystem.toLower());
}

static bool isLinuxUserFacingMount(const QStorageInfo &storage)
{
    const QString root = normalizedLinuxMountPath(storage.rootPath());
    if (root.isEmpty()) {
        return false;
    }

    const QString fileSystem = QString::fromLatin1(storage.fileSystemType()).toLower();
    if (root == QLatin1String("/")) {
        return true;
    }
    if (isLinuxNetworkFileSystem(fileSystem)) {
        return true;
    }
    if (isLinuxPseudoFileSystem(fileSystem)) {
        return false;
    }

    const QString userName = QString::fromLocal8Bit(qgetenv("USER")).trimmed();
    if (!userName.isEmpty()) {
        if (pathIsDirectChildOf(root, QStringLiteral("/run/media/%1").arg(userName))
            || pathIsDirectChildOf(root, QStringLiteral("/media/%1").arg(userName))) {
            return true;
        }
    }

    return pathIsDirectChildOf(root, QStringLiteral("/media"))
        || pathIsDirectChildOf(root, QStringLiteral("/mnt"));
}

static void applyLinuxVolumeHints(VolumeInfo &info, const QStorageInfo &storage)
{
    const QString fileSystem = QString::fromLatin1(storage.fileSystemType()).toLower();

    if (isLinuxNetworkFileSystem(fileSystem)) {
        info.driveType = QStringLiteral("network");
        info.isNetwork = true;
    }
}
#endif

void VolumeMonitor::requestEject(const QString &rootPath)
{
    const QString normalizedRoot = volumeKeyForRoot(rootPath);
    if (normalizedRoot.isEmpty()) {
        emit ejectFinished(rootPath, false, QStringLiteral("Invalid device path."));
        return;
    }

#ifdef Q_OS_WIN
    QPointer<VolumeMonitor> self(this);
    (void)QtConcurrent::run([self, normalizedRoot]() {
        const EjectResult result = ejectVolumeNative(normalizedRoot);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, normalizedRoot, result]() {
            if (!self) {
                return;
            }
            emit self->ejectFinished(normalizedRoot, result.success, result.message);
            self->scheduleRefresh(150, 2);
        }, Qt::QueuedConnection);
    });
#else
    emit ejectFinished(normalizedRoot, false, QStringLiteral("Device eject is not supported on this platform yet."));
#endif
}

bool VolumeMonitor::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(result)

#ifdef Q_OS_WIN
    const auto *msg = static_cast<MSG *>(message);
    if (!msg || msg->message != WM_DEVICECHANGE) {
        return false;
    }

    switch (msg->wParam) {
    case DBT_DEVICEARRIVAL:
    case DBT_DEVICEREMOVECOMPLETE:
    case DBT_DEVNODES_CHANGED:
    case DBT_DEVICEREMOVEPENDING:
        emit deviceTopologyChanged();
        scheduleRefresh(150, 2);
        break;
    default:
        break;
    }
#else
    Q_UNUSED(message)
#endif
    return false;
}

void VolumeMonitor::refreshNow()
{
    applySnapshot(enumerateVolumes());
    if (m_followUpRefreshes > 0) {
        --m_followUpRefreshes;
        m_refreshTimer.start(m_followUpRefreshes == 0 ? 1000 : 350);
    }
}

void VolumeMonitor::scheduleRefresh()
{
    scheduleRefresh(150, 2);
}

QList<VolumeInfo> VolumeMonitor::enumerateVolumes() const
{
    QList<VolumeInfo> result;

    for (QStorageInfo storage : QStorageInfo::mountedVolumes()) {
        storage.refresh();
        if (!storage.isValid() || storage.rootPath().isEmpty()) {
            continue;
        }
#ifndef Q_OS_WIN
        if (!isLinuxUserFacingMount(storage)) {
            continue;
        }
#endif

        VolumeInfo info;
        info.rootPath = volumeKeyForRoot(storage.rootPath());
        if (info.rootPath.isEmpty()) {
            continue;
        }

        info.displayName = DriveUtils::volumeDisplayName(storage);
        if (info.displayName.isEmpty()) {
            info.displayName = info.rootPath;
        }
        info.fileSystem = QString::fromLatin1(storage.fileSystemType());
        info.driveType = DriveUtils::detectDriveType(storage);
        info.isReady = storage.isReady();
        info.totalBytes = storage.bytesTotal();
        info.freeBytes = storage.bytesFree();
        info.isCritical = info.totalBytes > 0
            && (static_cast<double>(info.freeBytes) / static_cast<double>(info.totalBytes)) < 0.10;
        info.isRemovable = info.driveType == QLatin1String("usb");
        info.isOptical = info.driveType == QLatin1String("optical");
        info.isNetwork = info.driveType == QLatin1String("network");
        info.isEjectable = info.isRemovable || info.isOptical;
#ifndef Q_OS_WIN
        applyLinuxVolumeHints(info, storage);
#endif

        result.append(info);
    }

    std::sort(result.begin(), result.end(), [](const VolumeInfo &lhs, const VolumeInfo &rhs) {
        return lhs.rootPath.compare(rhs.rootPath, Qt::CaseInsensitive) < 0;
    });

    return result;
}

void VolumeMonitor::applySnapshot(const QList<VolumeInfo> &volumes)
{
    QHash<QString, VolumeInfo> nextByKey;
    for (const VolumeInfo &volume : volumes) {
        nextByKey.insert(volumeKeyForRoot(volume.rootPath), volume);
    }

    bool changed = false;
    QHash<QString, VolumeInfo> effectiveByKey = nextByKey;
    QList<VolumeInfo> removedVolumes;
    QList<VolumeInfo> addedVolumes;
    QList<VolumeInfo> changedVolumes;

    for (auto it = m_volumesByKey.cbegin(); it != m_volumesByKey.cend(); ++it) {
        if (nextByKey.contains(it.key())) {
            continue;
        }

        auto pendingIt = m_pendingRemovedByKey.find(it.key());
        if (pendingIt == m_pendingRemovedByKey.end()) {
            PendingRemoval pending;
            pending.info = it.value();
            pending.missedSnapshots = 1;
            m_pendingRemovedByKey.insert(it.key(), pending);
            effectiveByKey.insert(it.key(), it.value());
            scheduleRefresh(350, 1);
            continue;
        }

        ++pendingIt->missedSnapshots;
        if (pendingIt->missedSnapshots < 2) {
            effectiveByKey.insert(it.key(), pendingIt->info);
            scheduleRefresh(350, 1);
            continue;
        }

        const VolumeInfo removed = pendingIt->info;
        m_recentlyRemovedByKey.insert(it.key(), removed);
        m_pendingRemovedByKey.erase(pendingIt);
        removedVolumes.append(removed);
        changed = true;
    }

    for (auto it = nextByKey.cbegin(); it != nextByKey.cend(); ++it) {
        m_pendingRemovedByKey.remove(it.key());
        const auto oldIt = m_volumesByKey.constFind(it.key());
        if (oldIt == m_volumesByKey.cend()) {
            m_recentlyRemovedByKey.remove(it.key());
            addedVolumes.append(it.value());
            changed = true;
            continue;
        }
        if (volumeInfoChanged(oldIt.value(), it.value())) {
            changedVolumes.append(it.value());
            changed = true;
        }
    }

    QList<VolumeInfo> effectiveVolumes = effectiveByKey.values();
    std::sort(effectiveVolumes.begin(), effectiveVolumes.end(), [](const VolumeInfo &lhs, const VolumeInfo &rhs) {
        return lhs.rootPath.compare(rhs.rootPath, Qt::CaseInsensitive) < 0;
    });

    m_volumes = effectiveVolumes;
    m_volumesByKey = effectiveByKey;

    for (const VolumeInfo &volume : std::as_const(removedVolumes)) {
        emit volumeRemoved(volume.rootPath, volume.displayName);
    }
    for (const VolumeInfo &volume : std::as_const(addedVolumes)) {
        emit volumeAdded(volume.rootPath);
    }
    for (const VolumeInfo &volume : std::as_const(changedVolumes)) {
        emit volumeChanged(volume.rootPath);
    }
    if (changed) {
        emit volumesChanged();
    }
}

void VolumeMonitor::scheduleRefresh(int delayMs, int followUpCount)
{
    m_followUpRefreshes = std::max(m_followUpRefreshes, followUpCount);
    if (!m_refreshTimer.isActive()) {
        m_refreshTimer.start(std::max(0, delayMs));
    }
}

QString VolumeMonitor::rootForPathInMap(const QString &path, const QHash<QString, VolumeInfo> &volumesByKey) const
{
    QString bestRoot;
    for (auto it = volumesByKey.cbegin(); it != volumesByKey.cend(); ++it) {
        if (!pathBelongsToRoot(path, it.value().rootPath)) {
            continue;
        }
        if (it.value().rootPath.size() > bestRoot.size()) {
            bestRoot = it.value().rootPath;
        }
    }
    return bestRoot;
}

QString VolumeMonitor::volumeKeyForRoot(const QString &rootPath)
{
    QString path = QDir::cleanPath(QDir::fromNativeSeparators(rootPath.trimmed()));
    if (path.isEmpty()) {
        return {};
    }

#ifdef Q_OS_WIN
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':')) {
        path = path.left(2).toUpper() + QLatin1Char('/');
    }
#endif
    return path;
}

QString VolumeMonitor::comparablePath(const QString &path)
{
    QString value = path.trimmed();
    if (value.isEmpty()) {
        return {};
    }
    if (ArchiveSupport::isArchivePath(value)) {
        value = ArchiveSupport::physicalArchivePath(value);
    } else if (isUriPath(value)) {
        return {};
    }

    value = QDir::cleanPath(QDir::fromNativeSeparators(value));
#ifdef Q_OS_WIN
    if (value.size() >= 2 && value.at(1) == QLatin1Char(':')) {
        value = value.left(1).toUpper() + value.mid(1);
    }
#endif
    return value;
}

bool VolumeMonitor::volumeInfoChanged(const VolumeInfo &lhs, const VolumeInfo &rhs)
{
    return lhs.displayName != rhs.displayName
        || lhs.fileSystem != rhs.fileSystem
        || lhs.driveType != rhs.driveType
        || lhs.isReady != rhs.isReady
        || lhs.isRemovable != rhs.isRemovable
        || lhs.isOptical != rhs.isOptical
        || lhs.isNetwork != rhs.isNetwork
        || lhs.isEjectable != rhs.isEjectable;
}
