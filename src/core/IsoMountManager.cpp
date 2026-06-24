#include "IsoMountManager.h"

#include "IsoSupport.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QStorageInfo>
#include <QThreadPool>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <virtdisk.h>

#include <memory>
#endif

namespace {

struct NativeMountResult {
    bool success = false;
    QString rootPath;
    QString error;
    quintptr nativeHandle = 0;
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

} // namespace

IsoMountManager::IsoMountManager(QObject *parent)
    : QObject(parent)
{
}

bool IsoMountManager::canMountIsoPath(const QString &path) const
{
    return IsoSupport::isIsoImagePath(path);
}

QStringList IsoMountManager::availableDriveLetters() const
{
    QStringList used;
    for (const QFileInfo &drive : QDir::drives()) {
        const QString path = drive.absoluteFilePath();
        if (path.size() >= 1) {
            used.append(path.left(1).toUpper());
        }
    }

    QStringList result;
    for (QChar ch = QLatin1Char('D'); ch <= QLatin1Char('Z'); ch = QChar(ch.unicode() + 1)) {
        const QString letter(ch);
        if (!used.contains(letter, Qt::CaseInsensitive)) {
            result.append(letter);
        }
    }
    return result;
}

bool IsoMountManager::isMountedImage(const QString &imagePath) const
{
    return m_mountsByImage.contains(normalizedLocalPath(imagePath));
}

bool IsoMountManager::isManagedMountRoot(const QString &rootPath) const
{
    return m_imagesByRoot.contains(normalizeRootPath(rootPath));
}

bool IsoMountManager::isInsideManagedMount(const QString &path) const
{
    const QString normalizedPath = QDir::fromNativeSeparators(path).trimmed();
    if (normalizedPath.isEmpty()) {
        return false;
    }

    for (const QString &root : m_imagesByRoot.keys()) {
        if (normalizedPath.compare(root, Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (normalizedPath.startsWith(root, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QString IsoMountManager::mountedRootForImage(const QString &imagePath) const
{
    const auto it = m_mountsByImage.constFind(normalizedLocalPath(imagePath));
    return it == m_mountsByImage.cend() ? QString() : it->rootPath;
}

QList<IsoMountManager::Mount> IsoMountManager::mounts() const
{
    return m_mountsByImage.values();
}

IsoMountManager::Mount IsoMountManager::mountForRoot(const QString &rootPath) const
{
    const QString imagePath = m_imagesByRoot.value(normalizeRootPath(rootPath));
    return imagePath.isEmpty() ? Mount() : m_mountsByImage.value(imagePath);
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
#else
        NativeMountResult result;
        result.error = QStringLiteral("ISO mounting is only supported on Windows");
#endif

        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, normalizedImage, driveLetter, result]() {
            if (!self) return;
            if (result.success) {
                self->rememberMount(normalizedImage, result.rootPath, driveLetter, result.nativeHandle);
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
    const QString normalizedRoot = normalizeRootPath(rootPath);
    const QString imagePath = m_imagesByRoot.value(normalizedRoot);
    if (imagePath.isEmpty()) {
        emit statusMessage(QStringLiteral("This drive is not an app-managed ISO mount"));
        emit unmountFinished(normalizedRoot, false, QStringLiteral("This drive is not an app-managed ISO mount"));
        return;
    }

    const quintptr nativeHandle = m_mountsByImage.value(imagePath).nativeHandle;
    emit unmountStarted(normalizedRoot);
    emit statusMessage(QStringLiteral("Unmounting ISO image"));

    QPointer<IsoMountManager> self(this);
    QThreadPool::globalInstance()->start([self, normalizedRoot, nativeHandle]() {
#ifdef Q_OS_WIN
        const QString error = unmountIsoNative(nativeHandle);
#else
        const QString error = QStringLiteral("ISO unmounting is only supported on Windows");
#endif
        const bool success = error.isEmpty();

        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, normalizedRoot, success, error]() {
            if (!self) return;
            if (success) {
                self->forgetMountRoot(normalizedRoot);
                emit self->statusMessage(QStringLiteral("ISO image unmounted"));
            } else {
                emit self->statusMessage(error);
            }
            emit self->unmountFinished(normalizedRoot, success, error);
        }, Qt::QueuedConnection);
    });
}

void IsoMountManager::unmountAll()
{
#ifdef Q_OS_WIN
    const auto mounts = m_mountsByImage.values();
    for (const Mount &mount : mounts) {
        if (mount.nativeHandle != 0) {
            (void)unmountIsoNative(mount.nativeHandle);
        }
    }
#endif
    m_mountsByImage.clear();
    m_imagesByRoot.clear();
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
    return QString(letter.toUpper()) + QStringLiteral(":/");
}

void IsoMountManager::rememberMount(const QString &imagePath, const QString &rootPath, QChar requestedLetter, quintptr nativeHandle)
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
    m_mountsByImage.insert(normalizedImage, mount);
    m_imagesByRoot.insert(normalizedRoot, normalizedImage);
    emit mountsChanged();
}

void IsoMountManager::forgetMountRoot(const QString &rootPath)
{
    const QString normalizedRoot = normalizeRootPath(rootPath);
    const QString imagePath = m_imagesByRoot.take(normalizedRoot);
    if (!imagePath.isEmpty()) {
        m_mountsByImage.remove(imagePath);
        emit mountsChanged();
    }
}
