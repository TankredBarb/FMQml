#include "IsoMountManager.h"

#include "IsoSupport.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QThread>
#include <QThreadPool>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <virtdisk.h>

#include <memory>
#endif

namespace {

bool isoTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIntValue("FM_ISO_TRACE") > 0;
    return enabled;
}

struct NativeMountResult {
    bool success = false;
    QString rootPath;
    QString error;
    quintptr nativeHandle = 0;
    QString nativeDevice;
    QString mountedDevice;
};

#ifdef Q_OS_WIN

QString windowsErrorMessage(DWORD code)
{
    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);
    QString message = length > 0 && buffer
        ? QString::fromWCharArray(buffer, int(length)).trimmed()
        : QStringLiteral("Windows error %1").arg(code);
    if (buffer) {
        LocalFree(buffer);
    }
    return message;
}

QString dosDeviceTarget(const QString &dosName)
{
    DWORD capacity = 512;
    for (int attempt = 0; attempt < 5; ++attempt) {
        std::unique_ptr<wchar_t[]> buffer(new wchar_t[capacity]);
        const DWORD length = QueryDosDeviceW(reinterpret_cast<LPCWSTR>(dosName.utf16()), buffer.get(), capacity);
        if (length > 0) {
            return QString::fromWCharArray(buffer.get()).toLower();
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return {};
        }
        capacity *= 2;
    }
    return {};
}

QString targetForPhysicalPath(const QString &physicalPath)
{
    QString name = QDir::fromNativeSeparators(physicalPath);
    if (name.startsWith(QStringLiteral("//./"))) {
        name = name.mid(4);
    } else if (name.startsWith(QStringLiteral("\\\\.\\"))) {
        name = name.mid(4);
    }
    const int slash = name.indexOf(QLatin1Char('/'));
    if (slash >= 0) {
        name = name.left(slash);
    }
    return dosDeviceTarget(name);
}

QStringList volumePathNames(const QString &volumeName)
{
    DWORD required = 0;
    GetVolumePathNamesForVolumeNameW(reinterpret_cast<LPCWSTR>(volumeName.utf16()), nullptr, 0, &required);
    if (required == 0) {
        return {};
    }

    std::unique_ptr<wchar_t[]> buffer(new wchar_t[required]);
    if (!GetVolumePathNamesForVolumeNameW(reinterpret_cast<LPCWSTR>(volumeName.utf16()), buffer.get(), required, &required)) {
        return {};
    }

    QStringList paths;
    const wchar_t *cursor = buffer.get();
    while (*cursor) {
        const QString path = QDir::fromNativeSeparators(QString::fromWCharArray(cursor));
        paths.append(path);
        cursor += wcslen(cursor) + 1;
    }
    return paths;
}

QString driveRootForVolumeName(const QString &volumeName)
{
    for (const QString &path : volumePathNames(volumeName)) {
        if (path.size() == 3 && path.at(1) == QLatin1Char(':')) {
            return path.left(2).toUpper() + QLatin1Char('/');
        }
    }
    return {};
}

QString volumeNameForDeviceTarget(const QString &deviceTarget)
{
    if (deviceTarget.isEmpty()) {
        return {};
    }

    DWORD capacity = MAX_PATH;
    std::unique_ptr<wchar_t[]> buffer(new wchar_t[capacity]);
    HANDLE find = FindFirstVolumeW(buffer.get(), capacity);
    if (find == INVALID_HANDLE_VALUE) {
        return {};
    }

    QString result;
    while (true) {
        const QString volumeName = QString::fromWCharArray(buffer.get());
        QString dosName = volumeName;
        if (dosName.startsWith(QStringLiteral("\\\\?\\"))) {
            dosName = dosName.mid(4);
        }
        if (dosName.endsWith(QLatin1Char('\\')) || dosName.endsWith(QLatin1Char('/'))) {
            dosName.chop(1);
        }

        if (dosDeviceTarget(dosName) == deviceTarget) {
            result = volumeName;
            break;
        }

        if (!FindNextVolumeW(find, buffer.get(), capacity)) {
            if (GetLastError() == ERROR_MORE_DATA) {
                capacity *= 2;
                buffer.reset(new wchar_t[capacity]);
                continue;
            }
            break;
        }
    }

    FindVolumeClose(find);
    return result;
}

bool assignDriveLetter(const QString &volumeName, QChar letter, QString *error)
{
    if (letter.isNull()) {
        return true;
    }

    const QString rootPath = QString(letter.toUpper()) + QStringLiteral(":\\");
    const QString requested = QDir::fromNativeSeparators(rootPath).left(2).toUpper() + QLatin1Char('/');
    for (const QString &path : volumePathNames(volumeName)) {
        if (path.compare(requested, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    if (GetDriveTypeW(reinterpret_cast<LPCWSTR>(rootPath.utf16())) != DRIVE_NO_ROOT_DIR) {
        if (error) {
            *error = QStringLiteral("Drive letter %1 is no longer available").arg(letter.toUpper());
        }
        return false;
    }

    if (SetVolumeMountPointW(reinterpret_cast<LPCWSTR>(rootPath.utf16()),
                             reinterpret_cast<LPCWSTR>(volumeName.utf16()))) {
        return true;
    }

    if (error) {
        *error = windowsErrorMessage(GetLastError());
    }
    return false;
}

QString waitForVolumeRoot(const QString &deviceTarget, QChar requestedLetter, QString *assignError)
{
    for (int attempt = 0; attempt < 50; ++attempt) {
        const QString volumeName = volumeNameForDeviceTarget(deviceTarget);
        if (!volumeName.isEmpty()) {
            if (!requestedLetter.isNull()) {
                if (!assignDriveLetter(volumeName, requestedLetter, assignError) && assignError && assignError->isEmpty()) {
                    *assignError = QStringLiteral("System assignment failed");
                }
            }

            const QString requestedRoot = requestedLetter.isNull()
                ? QString()
                : QString(requestedLetter.toUpper()) + QStringLiteral(":/");
            for (const QString &path : volumePathNames(volumeName)) {
                if (!requestedRoot.isEmpty() && path.compare(requestedRoot, Qt::CaseInsensitive) == 0) {
                    return requestedRoot;
                }
            }
            const QString rootPath = driveRootForVolumeName(volumeName);
            if (!rootPath.isEmpty()) {
                return rootPath;
            }
        }
        Sleep(100);
    }
    return {};
}

NativeMountResult mountIsoNative(const QString &imagePath, QChar requestedLetter)
{
    NativeMountResult result;
    static constexpr GUID microsoftVirtualStorageVendor = {
        0xec984aec,
        0xa0f9,
        0x47e9,
        {0x90, 0x1f, 0x71, 0x41, 0x5a, 0x66, 0x34, 0x5b}
    };

    VIRTUAL_STORAGE_TYPE storageType = {};
    storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_ISO;
    storageType.VendorId = microsoftVirtualStorageVendor;

    OPEN_VIRTUAL_DISK_PARAMETERS openParameters = {};
    openParameters.Version = OPEN_VIRTUAL_DISK_VERSION_1;

    HANDLE handle = INVALID_HANDLE_VALUE;
    DWORD status = OpenVirtualDisk(
        &storageType,
        reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(imagePath).utf16()),
        VIRTUAL_DISK_ACCESS_ATTACH_RO | VIRTUAL_DISK_ACCESS_DETACH | VIRTUAL_DISK_ACCESS_GET_INFO,
        OPEN_VIRTUAL_DISK_FLAG_NONE,
        &openParameters,
        &handle);
    if (status != ERROR_SUCCESS) {
        result.error = QStringLiteral("OpenVirtualDisk failed: %1").arg(windowsErrorMessage(status));
        return result;
    }

    ATTACH_VIRTUAL_DISK_PARAMETERS attachParameters = {};
    attachParameters.Version = ATTACH_VIRTUAL_DISK_VERSION_1;
    status = AttachVirtualDisk(
        handle,
        nullptr,
        ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY,
        0,
        &attachParameters,
        nullptr);
    if (status != ERROR_SUCCESS && status != ERROR_ALREADY_EXISTS) {
        result.error = QStringLiteral("AttachVirtualDisk failed: %1").arg(windowsErrorMessage(status));
        CloseHandle(handle);
        return result;
    }

    ULONG pathChars = 0;
    status = GetVirtualDiskPhysicalPath(handle, &pathChars, nullptr);
    if (status != ERROR_INSUFFICIENT_BUFFER || pathChars == 0) {
        result.error = QStringLiteral("GetVirtualDiskPhysicalPath failed: %1").arg(windowsErrorMessage(status));
        DetachVirtualDisk(handle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
        CloseHandle(handle);
        return result;
    }

    std::unique_ptr<wchar_t[]> physicalPathBuffer(new wchar_t[pathChars]);
    status = GetVirtualDiskPhysicalPath(handle, &pathChars, physicalPathBuffer.get());
    if (status != ERROR_SUCCESS) {
        result.error = QStringLiteral("GetVirtualDiskPhysicalPath failed: %1").arg(windowsErrorMessage(status));
        DetachVirtualDisk(handle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
        CloseHandle(handle);
        return result;
    }

    const QString deviceTarget = targetForPhysicalPath(QString::fromWCharArray(physicalPathBuffer.get()));
    QString assignError;
    const QString rootPath = waitForVolumeRoot(deviceTarget, requestedLetter, &assignError);
    if (rootPath.isEmpty()) {
            result.error = assignError.isEmpty()
            ? QStringLiteral("Mounted image could not be exposed to the system")
            : QStringLiteral("Mounted image could not be exposed to the system: %1").arg(assignError);
        DetachVirtualDisk(handle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
        CloseHandle(handle);
        return result;
    }

    result.success = true;
    result.rootPath = rootPath;
    result.nativeHandle = reinterpret_cast<quintptr>(handle);
    if (!requestedLetter.isNull()) {
        const QChar actualLetter = rootPath.isEmpty() ? QChar() : rootPath.at(0).toUpper();
        if (!actualLetter.isNull() && actualLetter != requestedLetter.toUpper()) {
            result.error = assignError.isEmpty()
                ? QStringLiteral("Mounted at %1 instead of requested %2:")
                      .arg(rootPath.left(2))
                      .arg(requestedLetter.toUpper())
                : QStringLiteral("Mounted at %1 instead of requested %2: %3")
                      .arg(rootPath.left(2))
                      .arg(requestedLetter.toUpper())
                      .arg(assignError);
        } else if (!assignError.isEmpty()) {
            result.error = assignError;
        }
    }
    return result;
}

QString unmountIsoNative(quintptr nativeHandle)
{
    HANDLE handle = reinterpret_cast<HANDLE>(nativeHandle);
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return QStringLiteral("Invalid ISO mount handle");
    }

    const DWORD status = DetachVirtualDisk(handle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
    CloseHandle(handle);
    if (status != ERROR_SUCCESS) {
        return QStringLiteral("DetachVirtualDisk failed: %1").arg(windowsErrorMessage(status));
    }
    return {};
}

#endif

#ifdef Q_OS_LINUX

bool runUdisksctl(const QStringList &arguments, QString *output, QString *error)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QStringLiteral("udisksctl"), arguments);
    if (!process.waitForStarted(5000)) {
        if (error) {
            *error = QStringLiteral("Could not start udisksctl");
        }
        return false;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        if (error) {
            *error = QStringLiteral("udisksctl timed out");
        }
        return false;
    }
    const QString commandOutput = QString::fromUtf8(process.readAll()).trimmed();
    if (isoTraceEnabled()) {
        qInfo().noquote() << "[IsoTrace] udisksctl"
                          << "args=" << arguments
                          << "exitCode=" << process.exitCode()
                          << "output=" << commandOutput;
    }
    if (output) {
        *output = commandOutput;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = commandOutput.isEmpty()
                ? QStringLiteral("udisksctl failed")
                : QStringLiteral("udisksctl failed: %1").arg(commandOutput);
        }
        return false;
    }
    return true;
}

QString lastDevicePath(const QString &output)
{
    static const QRegularExpression devicePattern(QStringLiteral(R"((/dev/[^\s.]+))"));
    QRegularExpressionMatchIterator matches = devicePattern.globalMatch(output);
    QString result;
    while (matches.hasNext()) {
        result = matches.next().captured(1);
    }
    return result;
}

QString mountedRootPath(const QString &output)
{
    static const QRegularExpression mountPattern(QStringLiteral(R"(\bat\s+(.+?)\.?\s*$)"));
    const QRegularExpressionMatch match = mountPattern.match(output);
    return match.hasMatch() ? QDir::cleanPath(match.captured(1).trimmed()) : QString();
}

QString loopDeviceFor(const QString &devicePath)
{
    static const QRegularExpression loopPattern(QStringLiteral(R"(^(/dev/loop\d+))"));
    const QRegularExpressionMatch match = loopPattern.match(devicePath);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString unescapeKernelPath(QString value)
{
    static const QRegularExpression escapePattern(QStringLiteral(R"(\\([0-7]{3}))"));
    QRegularExpressionMatch match;
    while ((match = escapePattern.match(value)).hasMatch()) {
        const QChar character(ushort(match.captured(1).toInt(nullptr, 8)));
        value.replace(match.capturedStart(), match.capturedLength(), character);
    }
    return value;
}

QString loopBackingFile(const QString &loopDevice)
{
    const QString loopName = QFileInfo(loopDevice).fileName();
    QFile file(QStringLiteral("/sys/class/block/%1/loop/backing_file").arg(loopName));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return unescapeKernelPath(QString::fromUtf8(file.readAll()).trimmed());
}

struct LinuxMountedDevice {
    QString rootPath;
    QString devicePath;
};

LinuxMountedDevice waitForLinuxMount(const QString &loopDevice)
{
    for (int attempt = 0; attempt < 50; ++attempt) {
        for (QStorageInfo storage : QStorageInfo::mountedVolumes()) {
            storage.refresh();
            if (!storage.isValid() || storage.rootPath().isEmpty()) {
                continue;
            }
            const QString mountedDevice = QString::fromUtf8(storage.device());
            if (loopDeviceFor(mountedDevice) == loopDevice) {
                return {QDir::cleanPath(storage.rootPath()), mountedDevice};
            }
        }
        QThread::msleep(100);
    }
    return {};
}

NativeMountResult mountIsoNative(const QString &imagePath)
{
    NativeMountResult result;
    QString output;
    QString error;
    if (!runUdisksctl({QStringLiteral("loop-setup"), QStringLiteral("--file"), imagePath,
                       QStringLiteral("--no-user-interaction")},
                      &output, &error)) {
        result.error = error;
        return result;
    }

    const QString devicePath = lastDevicePath(output);
    if (devicePath.isEmpty()) {
        result.error = QStringLiteral("UDisks2 did not report a loop device");
        return result;
    }

    const bool mountCommandSucceeded = runUdisksctl(
        {QStringLiteral("mount"), QStringLiteral("--block-device"), devicePath,
         QStringLiteral("--no-user-interaction")},
        &output, &error);
    const bool alreadyMounted = !mountCommandSucceeded
        && output.contains(QStringLiteral("org.freedesktop.UDisks2.Error.AlreadyMounted"),
                           Qt::CaseInsensitive);
    if (!mountCommandSucceeded && !alreadyMounted) {
        QString ignoredOutput;
        QString ignoredError;
        runUdisksctl({QStringLiteral("loop-delete"), QStringLiteral("--block-device"), devicePath,
                      QStringLiteral("--no-user-interaction")},
                     &ignoredOutput, &ignoredError);
        result.error = error;
        return result;
    }

    const LinuxMountedDevice mounted = waitForLinuxMount(devicePath);
    const QString rootPath = !mounted.rootPath.isEmpty() ? mounted.rootPath : mountedRootPath(output);
    if (rootPath.isEmpty()) {
        QString ignoredOutput;
        QString ignoredError;
        runUdisksctl({QStringLiteral("unmount"), QStringLiteral("--block-device"), devicePath,
                      QStringLiteral("--no-user-interaction")},
                     &ignoredOutput, &ignoredError);
        runUdisksctl({QStringLiteral("loop-delete"), QStringLiteral("--block-device"), devicePath,
                      QStringLiteral("--no-user-interaction")},
                     &ignoredOutput, &ignoredError);
        result.error = QStringLiteral("UDisks2 did not report a mount location");
        return result;
    }

    result.success = true;
    result.rootPath = rootPath;
    result.nativeDevice = devicePath;
    result.mountedDevice = !mounted.devicePath.isEmpty() ? mounted.devicePath : lastDevicePath(output);
    if (result.mountedDevice.isEmpty()) {
        result.mountedDevice = devicePath;
    }
    return result;
}

QString unmountIsoNative(const QString &mountedDevice, const QString &loopDevice)
{
    QString output;
    QString error;
    if (!runUdisksctl({QStringLiteral("unmount"), QStringLiteral("--block-device"), mountedDevice,
                       QStringLiteral("--no-user-interaction")},
                      &output, &error)) {
        return error;
    }
    for (int attempt = 0; attempt < 20 && QFileInfo::exists(loopDevice); ++attempt) {
        QThread::msleep(50);
    }
    if (QFileInfo::exists(loopDevice)
        && !runUdisksctl({QStringLiteral("loop-delete"), QStringLiteral("--block-device"), loopDevice,
                          QStringLiteral("--no-user-interaction")},
                         &output, &error)
        && isoTraceEnabled()) {
        qWarning().noquote() << "[IsoTrace] loop-cleanup-warning"
                             << "loopDevice=" << loopDevice
                             << "error=" << error;
    }
    return {};
}

#endif

} // namespace

IsoMountManager::IsoMountManager(QObject *parent)
    : QObject(parent)
{
    adoptLinuxIsoMounts();
}

void IsoMountManager::adoptLinuxIsoMounts()
{
#ifdef Q_OS_LINUX
    for (QStorageInfo storage : QStorageInfo::mountedVolumes()) {
        storage.refresh();
        if (!storage.isValid() || storage.rootPath().isEmpty()) {
            continue;
        }

        const QString mountedDevice = QString::fromUtf8(storage.device());
        const QString loopDevice = loopDeviceFor(mountedDevice);
        if (loopDevice.isEmpty()) {
            continue;
        }

        const QString imagePath = loopBackingFile(loopDevice);
        if (!IsoSupport::isIsoImagePath(imagePath)) {
            continue;
        }

        if (isoTraceEnabled()) {
            qInfo().noquote() << "[IsoTrace] adopt-existing-mount"
                              << "image=" << imagePath
                              << "root=" << storage.rootPath()
                              << "mountedDevice=" << mountedDevice
                              << "loopDevice=" << loopDevice;
        }
        rememberMount(imagePath, storage.rootPath(), {}, 0, loopDevice, mountedDevice);
    }
#endif
}

bool IsoMountManager::canMountIsoPath(const QString &path) const
{
    if (!IsoSupport::isIsoImagePath(path)) {
        return false;
    }
#ifdef Q_OS_LINUX
    return !QStandardPaths::findExecutable(QStringLiteral("udisksctl")).isEmpty();
#else
    return true;
#endif
}

bool IsoMountManager::isMountedImage(const QString &imagePath) const
{
    return m_rootsByImage.contains(normalizedLocalPath(imagePath));
}

bool IsoMountManager::isManagedMountRoot(const QString &rootPath) const
{
    return !managedMountRootForPath(rootPath).isEmpty();
}

QString IsoMountManager::managedMountRootForPath(const QString &path) const
{
    const QString normalizedPath = normalizeRootPath(path);
    if (isoTraceEnabled()) {
        qInfo().noquote() << "[IsoTrace] managed-root-lookup"
                          << "input=" << path
                          << "normalized=" << normalizedPath
                          << "roots=" << m_mountsByRoot.keys();
    }
    if (normalizedPath.isEmpty()) {
        return {};
    }
    if (m_mountsByRoot.contains(normalizedPath)) {
        if (isoTraceEnabled()) {
            qInfo().noquote() << "[IsoTrace] managed-root-match" << "root=" << normalizedPath << "mode=exact";
        }
        return normalizedPath;
    }

    const QString canonicalPath = normalizeRootPath(QFileInfo(normalizedPath).canonicalFilePath());
    if (canonicalPath.isEmpty()) {
        if (isoTraceEnabled()) {
            qInfo().noquote() << "[IsoTrace] managed-root-miss" << "reason=no-canonical-path";
        }
        return {};
    }
    for (auto it = m_mountsByRoot.cbegin(); it != m_mountsByRoot.cend(); ++it) {
        const QString canonicalRoot = normalizeRootPath(QFileInfo(it.key()).canonicalFilePath());
        if (!canonicalRoot.isEmpty() && canonicalRoot == canonicalPath) {
            if (isoTraceEnabled()) {
                qInfo().noquote() << "[IsoTrace] managed-root-match"
                                  << "root=" << it.key() << "mode=canonical";
            }
            return it.key();
        }
    }
    if (isoTraceEnabled()) {
        qInfo().noquote() << "[IsoTrace] managed-root-miss"
                          << "canonical=" << canonicalPath;
    }
    return {};
}

bool IsoMountManager::isInsideManagedMount(const QString &path) const
{
    const QString normalizedPath = normalizeRootPath(path);
    if (normalizedPath.isEmpty()) {
        return false;
    }

    for (const QString &root : m_mountsByRoot.keys()) {
        if (normalizedPath.compare(root, Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (normalizedPath.startsWith(root + QLatin1Char('/'), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QString IsoMountManager::mountedRootForImage(const QString &imagePath) const
{
    const QList<QString> roots = m_rootsByImage.values(normalizedLocalPath(imagePath));
    return roots.isEmpty() ? QString() : roots.constFirst();
}

QList<IsoMountManager::Mount> IsoMountManager::mounts() const
{
    return m_mountsByRoot.values();
}

IsoMountManager::Mount IsoMountManager::mountForRoot(const QString &rootPath) const
{
    return m_mountsByRoot.value(managedMountRootForPath(rootPath));
}

void IsoMountManager::mountIsoToLetter(const QString &imagePath, const QString &letter)
{
    const QString normalizedImage = normalizedLocalPath(imagePath);
    const QChar driveLetter = normalizeLetter(letter);
    if (!IsoSupport::isIsoImagePath(normalizedImage)) {
        emit mountFinished(imagePath, {}, false, QStringLiteral("Invalid ISO image"));
        emit statusMessage(QStringLiteral("Invalid ISO image"));
        return;
    }
    if (!QFileInfo::exists(normalizedImage)) {
        emit mountFinished(imagePath, {}, false, QStringLiteral("ISO source file does not exist"));
        emit statusMessage(QStringLiteral("ISO source file does not exist"));
        return;
    }

    if (isMountedImage(normalizedImage)) {
        const QString rootPath = mountedRootForImage(normalizedImage);
        emit statusMessage(QStringLiteral("ISO image is already mounted"));
        emit mountFinished(normalizedImage, rootPath, true, {});
        return;
    }

    const QString requestedRootPath = rootPathForLetter(driveLetter);
    emit mountStarted(normalizedImage, requestedRootPath);
    emit statusMessage(QStringLiteral("Mounting ISO image"));

    QPointer<IsoMountManager> self(this);
    QThreadPool::globalInstance()->start([self, normalizedImage, driveLetter]() {
#ifdef Q_OS_WIN
        const NativeMountResult result = mountIsoNative(normalizedImage, driveLetter);
#elif defined(Q_OS_LINUX)
        const NativeMountResult result = mountIsoNative(normalizedImage);
#else
        NativeMountResult result;
        result.error = QStringLiteral("ISO mounting is not supported on this platform");
#endif

        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, normalizedImage, driveLetter, result]() {
            if (!self) return;
            if (isoTraceEnabled()) {
                qInfo().noquote() << "[IsoTrace] mount-result"
                                  << "success=" << result.success
                                  << "root=" << result.rootPath
                                  << "mountedDevice=" << result.mountedDevice
                                  << "loopDevice=" << result.nativeDevice
                                  << "error=" << result.error;
            }
            if (result.success) {
                self->rememberMount(normalizedImage, result.rootPath, driveLetter,
                                    result.nativeHandle, result.nativeDevice, result.mountedDevice);
                emit self->statusMessage(QStringLiteral("ISO image mounted"));
            } else {
                emit self->statusMessage(result.error.isEmpty() ? QStringLiteral("ISO mount failed") : result.error);
            }
            emit self->mountFinished(normalizedImage, result.rootPath, result.success, result.error);
        }, Qt::QueuedConnection);
    });
}

void IsoMountManager::unmountIsoRoot(const QString &rootPath)
{
    const QString normalizedRoot = managedMountRootForPath(rootPath);
    const Mount mount = m_mountsByRoot.value(normalizedRoot);
    if (mount.imagePath.isEmpty()) {
        emit statusMessage(QStringLiteral("This drive is not an app-managed ISO mount"));
        emit unmountFinished(normalizedRoot, false, QStringLiteral("This drive is not an app-managed ISO mount"));
        return;
    }
    if (m_unmountingRoots.contains(normalizedRoot)) {
        emit statusMessage(QStringLiteral("ISO unmount is already in progress"));
        return;
    }

    if (isoTraceEnabled()) {
        qInfo().noquote() << "[IsoTrace] unmount-request"
                          << "inputRoot=" << rootPath
                          << "managedRoot=" << normalizedRoot
                          << "image=" << mount.imagePath
                          << "mountedDevice=" << mount.mountedDevice
                          << "loopDevice=" << mount.nativeDevice;
    }
    emit unmountStarted(normalizedRoot);
    emit statusMessage(QStringLiteral("Unmounting ISO image"));
    m_unmountingRoots.insert(normalizedRoot);

    QPointer<IsoMountManager> self(this);
    QThreadPool::globalInstance()->start([self, normalizedRoot, mount]() {
#ifdef Q_OS_WIN
        const QString error = unmountIsoNative(mount.nativeHandle);
#elif defined(Q_OS_LINUX)
        const QString error = mount.nativeDevice.isEmpty()
            ? QStringLiteral("Missing UDisks2 loop device for ISO mount")
            : unmountIsoNative(mount.mountedDevice.isEmpty() ? mount.nativeDevice : mount.mountedDevice,
                               mount.nativeDevice);
#else
        const QString error = QStringLiteral("ISO unmounting is not supported on this platform");
#endif
        const bool success = error.isEmpty();

        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, normalizedRoot, success, error]() {
            if (!self) return;
            self->m_unmountingRoots.remove(normalizedRoot);
            if (isoTraceEnabled()) {
                qInfo().noquote() << "[IsoTrace] unmount-result"
                                  << "root=" << normalizedRoot
                                  << "success=" << success
                                  << "error=" << error;
            }
            if (success) {
                self->forgetMountRoot(normalizedRoot);
                emit self->statusMessage(QStringLiteral("ISO image unmounted"));
            } else {
                const QString message = error.contains(QStringLiteral("busy"), Qt::CaseInsensitive)
                    ? QStringLiteral("ISO is still in use by a preview or another process. Close files opened from it and try again.")
                    : error;
                emit self->statusMessage(message);
            }
            emit self->unmountFinished(normalizedRoot, success, error);
        }, Qt::QueuedConnection);
    });
}

void IsoMountManager::unmountAll()
{
    if (isoTraceEnabled()) {
        qInfo().noquote() << "[IsoTrace] unmount-all" << "count=" << m_mountsByRoot.size();
    }
#ifdef Q_OS_WIN
    const auto mounts = m_mountsByRoot.values();
    for (const Mount &mount : mounts) {
        if (mount.nativeHandle != 0) {
            (void)unmountIsoNative(mount.nativeHandle);
        }
    }
#endif
#ifdef Q_OS_LINUX
    const auto mounts = m_mountsByRoot.values();
    for (const Mount &mount : mounts) {
        if (!mount.nativeDevice.isEmpty()) {
            (void)unmountIsoNative(mount.mountedDevice.isEmpty() ? mount.nativeDevice : mount.mountedDevice,
                                   mount.nativeDevice);
        }
    }
#endif
    m_mountsByRoot.clear();
    m_rootsByImage.clear();
}

QString IsoMountManager::normalizedLocalPath(const QString &path)
{
    return QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
}

QString IsoMountManager::normalizeRootPath(const QString &rootPath)
{
    QString path = QDir::fromNativeSeparators(rootPath).trimmed();
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':')) {
        path = path.left(2).toUpper() + QLatin1Char('/');
    } else if (path.size() > 1) {
        path = QDir::cleanPath(path);
    }
    return path;
}

QChar IsoMountManager::normalizeLetter(const QString &letter)
{
    if (letter.isEmpty()) {
        return {};
    }
    const QChar ch = letter.trimmed().at(0).toUpper();
    if (ch < QLatin1Char('A') || ch > QLatin1Char('Z')) {
        return {};
    }
    return ch;
}

QString IsoMountManager::rootPathForLetter(QChar letter)
{
    if (letter.isNull()) {
        return {};
    }
    return QString(letter.toUpper()) + QStringLiteral(":/");
}

void IsoMountManager::rememberMount(const QString &imagePath, const QString &rootPath, QChar requestedLetter,
                                    quintptr nativeHandle, const QString &nativeDevice,
                                    const QString &mountedDevice)
{
    const QString normalizedImage = normalizedLocalPath(imagePath);
    const QString normalizedRoot = normalizeRootPath(rootPath);
    Mount mount;
    mount.imagePath = normalizedImage;
    mount.rootPath = normalizedRoot;
    mount.letter = normalizedRoot.isEmpty() ? QChar() : normalizedRoot.at(0).toUpper();
    mount.requestedLetter = requestedLetter.toUpper();
    mount.mountedAt = QDateTime::currentDateTime();
    mount.nativeHandle = nativeHandle;
    mount.nativeDevice = nativeDevice;
    mount.mountedDevice = mountedDevice;
    if (isoTraceEnabled()) {
        qInfo().noquote() << "[IsoTrace] mount-remembered"
                          << "image=" << mount.imagePath
                          << "root=" << mount.rootPath
                          << "mountedDevice=" << mount.mountedDevice
                          << "loopDevice=" << mount.nativeDevice;
    }
    const Mount previous = m_mountsByRoot.value(normalizedRoot);
    if (!previous.imagePath.isEmpty()) {
        m_rootsByImage.remove(previous.imagePath, normalizedRoot);
    }
    m_mountsByRoot.insert(normalizedRoot, mount);
    if (!m_rootsByImage.contains(normalizedImage, normalizedRoot)) {
        m_rootsByImage.insert(normalizedImage, normalizedRoot);
    }
    emit mountsChanged();
}

void IsoMountManager::forgetMountRoot(const QString &rootPath)
{
    const QString normalizedRoot = normalizeRootPath(rootPath);
    const Mount mount = m_mountsByRoot.take(normalizedRoot);
    if (!mount.imagePath.isEmpty()) {
        m_rootsByImage.remove(mount.imagePath, normalizedRoot);
        emit mountsChanged();
    }
}
