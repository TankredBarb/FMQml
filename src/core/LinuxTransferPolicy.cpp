#include "LinuxTransferPolicy.h"

#include "DriveUtils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>

namespace LinuxTransferPolicy {
namespace {

FileSystemClass classifyFileSystem(const QString &fileSystem)
{
    const QString fs = fileSystem.trimmed().toLower();
    if (fs.isEmpty()) {
        return FileSystemClass::Unknown;
    }
    if (DriveUtils::linuxFileSystemIsNetwork(fs.toLatin1())) {
        return FileSystemClass::NetworkLike;
    }
    if (fs.startsWith(QLatin1String("fuse"))
        || fs == QLatin1String("ntfs") || fs == QLatin1String("ntfs3")
        || fs == QLatin1String("exfat") || fs == QLatin1String("vfat")) {
        return FileSystemClass::ForeignFuse;
    }
    static const QStringList ordinary{
        QStringLiteral("btrfs"), QStringLiteral("ext2"), QStringLiteral("ext3"),
        QStringLiteral("ext4"), QStringLiteral("f2fs"), QStringLiteral("jfs"),
        QStringLiteral("reiserfs"), QStringLiteral("xfs"), QStringLiteral("zfs"),
    };
    return ordinary.contains(fs) ? FileSystemClass::OrdinaryLocal : FileSystemClass::Unknown;
}

qint64 optimizedBuffer(StorageType type)
{
    switch (type) {
    case StorageType::Hdd:
    case StorageType::Usb:
        return 512 * 1024;
    case StorageType::SataSsd:
        return 4 * 1024 * 1024;
    case StorageType::NvmeSsd:
        return 8 * 1024 * 1024;
    default:
        return 1024 * 1024;
    }
}

QString endpointRiskReason(const Endpoint &endpoint)
{
    if (!endpoint.valid
        || endpoint.fileSystemClass == FileSystemClass::Unknown
        || endpoint.storageType == StorageType::Unknown) {
        return QStringLiteral("unknown-topology");
    }
    if (endpoint.fileSystemClass == FileSystemClass::NetworkLike
        || endpoint.storageType == StorageType::Network) {
        return QStringLiteral("network-like");
    }
    if (endpoint.fileSystemClass == FileSystemClass::ForeignFuse) {
        return QStringLiteral("foreign-or-fuse");
    }
    if (endpoint.storageType == StorageType::Usb
        || endpoint.storageType == StorageType::Optical) {
        return QStringLiteral("removable-or-slow-storage");
    }
    return {};
}

Decision conservative(const QString &reason)
{
    Decision result;
    result.reason = reason;
    return result;
}

} // namespace

Endpoint discoverEndpoint(const QString &path)
{
    Endpoint endpoint;
    endpoint.normalizedPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    QString storageLookupPath = endpoint.normalizedPath;
    QFileInfo storageLookupInfo(storageLookupPath);
    while (!storageLookupInfo.exists()) {
        const QString parentPath = storageLookupInfo.absolutePath();
        if (parentPath == storageLookupPath) {
            break;
        }
        storageLookupPath = parentPath;
        storageLookupInfo.setFile(storageLookupPath);
    }
    const QStorageInfo storage(storageLookupPath);
    if (!storage.isValid() || !storage.isReady()) {
        return endpoint;
    }

    endpoint.mountRoot = QDir::cleanPath(storage.rootPath());
    endpoint.fileSystemType = QString::fromLatin1(storage.fileSystemType()).trimmed().toLower();
    endpoint.deviceIdentifier = QFile::decodeName(storage.device()).trimmed();
    endpoint.blockDeviceName = DriveUtils::linuxBlockDeviceName(storage);
    endpoint.physicalDeviceName = DriveUtils::linuxParentPhysicalBlockDeviceName(endpoint.blockDeviceName);
    endpoint.fileSystemClass = classifyFileSystem(endpoint.fileSystemType);

    const QString detected = DriveUtils::detectDriveType(storage);
    if (detected == QLatin1String("network")) {
        endpoint.storageType = StorageType::Network;
    } else if (detected == QLatin1String("optical")) {
        endpoint.storageType = StorageType::Optical;
    } else if (detected == QLatin1String("usb")) {
        endpoint.storageType = StorageType::Usb;
    } else if (detected == QLatin1String("hdd") && !endpoint.blockDeviceName.isEmpty()) {
        endpoint.storageType = StorageType::Hdd;
    } else if (detected == QLatin1String("ssd") && !endpoint.blockDeviceName.isEmpty()) {
        endpoint.storageType = endpoint.physicalDeviceName.startsWith(QLatin1String("nvme"))
            ? StorageType::NvmeSsd : StorageType::SataSsd;
    }
    endpoint.valid = !endpoint.mountRoot.isEmpty() && !endpoint.fileSystemType.isEmpty();
    return endpoint;
}

Decision decide(const Endpoint &source, const Endpoint &destination)
{
    const QString sourceRisk = endpointRiskReason(source);
    if (!sourceRisk.isEmpty()) {
        return conservative(sourceRisk);
    }
    const QString destinationRisk = endpointRiskReason(destination);
    if (!destinationRisk.isEmpty()) {
        return conservative(destinationRisk);
    }
    if (source.fileSystemType.compare(destination.fileSystemType, Qt::CaseInsensitive) != 0) {
        return conservative(QStringLiteral("different-filesystem-type"));
    }
    if (source.mountRoot == destination.mountRoot) {
        Decision result;
        result.mode = Mode::Optimized;
        result.reason = QStringLiteral("same-mount");
        result.copyBufferLimit = 1024 * 1024;
        result.boundedCacheWriteback = true;
        result.copyIoPriority = IoPriority::ReducedBestEffort;
        result.archiveNice = 0;
        result.archiveIoPriority = IoPriority::Normal;
        result.archiveDutyCycle = false;
        return result;
    }
    if (source.physicalDeviceName.isEmpty() || destination.physicalDeviceName.isEmpty()) {
        return conservative(QStringLiteral("unknown-topology"));
    }
    if (source.physicalDeviceName == destination.physicalDeviceName) {
        return conservative(QStringLiteral("same-physical-device-cross-mount"));
    }

    Decision result;
    result.mode = Mode::Balanced;
    result.reason = QStringLiteral("independent-local-devices");
    result.copyBufferLimit = qMin(optimizedBuffer(source.storageType), optimizedBuffer(destination.storageType));
    result.boundedCacheWriteback = true;
    result.copyIoPriority = IoPriority::ReducedBestEffort;
    result.archiveNice = 19;
    result.archiveIoPriority = IoPriority::ReducedBestEffort;
    result.archiveDutyCycle = false;
    return result;
}

QString modeName(Mode mode)
{
    switch (mode) {
    case Mode::Optimized: return QStringLiteral("optimized");
    case Mode::Balanced: return QStringLiteral("balanced");
    case Mode::Conservative: return QStringLiteral("conservative");
    }
    return QStringLiteral("conservative");
}

QString storageTypeName(StorageType type)
{
    switch (type) {
    case StorageType::Hdd: return QStringLiteral("hdd");
    case StorageType::SataSsd: return QStringLiteral("sata-ssd");
    case StorageType::NvmeSsd: return QStringLiteral("nvme-ssd");
    case StorageType::Usb: return QStringLiteral("usb");
    case StorageType::Network: return QStringLiteral("network");
    case StorageType::Optical: return QStringLiteral("optical");
    case StorageType::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString ioPriorityName(IoPriority priority)
{
    switch (priority) {
    case IoPriority::Normal: return QStringLiteral("normal");
    case IoPriority::ReducedBestEffort: return QStringLiteral("reduced-best-effort");
    case IoPriority::Idle: return QStringLiteral("idle");
    }
    return QStringLiteral("idle");
}

void traceDecision(const char *operation, const Endpoint &source, const Endpoint &destination, const Decision &decision)
{
    if (!qEnvironmentVariableIsSet("FM_LINUX_TRANSFER_POLICY_TRACE")) {
        return;
    }
    QDebug log = qInfo().noquote();
    log << "[LinuxTransferPolicy]"
        << "operation=" << operation
        << "source=" << DriveUtils::displayPath(source.normalizedPath)
        << "destination=" << DriveUtils::displayPath(destination.normalizedPath)
        << "sourceMount=" << source.mountRoot
        << "destinationMount=" << destination.mountRoot
        << "sourceFs=" << source.fileSystemType
        << "destinationFs=" << destination.fileSystemType
        << "sourceDevice=" << source.deviceIdentifier
        << "destinationDevice=" << destination.deviceIdentifier
        << "sourcePhysical=" << source.physicalDeviceName
        << "destinationPhysical=" << destination.physicalDeviceName
        << "sourceStorage=" << storageTypeName(source.storageType)
        << "destinationStorage=" << storageTypeName(destination.storageType)
        << "mode=" << modeName(decision.mode)
        << "reason=" << decision.reason;
    if (qstrcmp(operation, "copy") == 0) {
        log << "buffer=" << decision.copyBufferLimit
            << "boundedCache=" << decision.boundedCacheWriteback
            << "ioprio=" << ioPriorityName(decision.copyIoPriority);
    } else {
        log << "nice=" << decision.archiveNice
            << "ioprio=" << ioPriorityName(decision.archiveIoPriority)
            << "throttle=" << (decision.archiveDutyCycle ? QStringLiteral("60/40ms") : QStringLiteral("off"));
    }
}

} // namespace LinuxTransferPolicy
