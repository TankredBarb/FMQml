#include "LinuxTransferPolicy.h"

#include <QDebug>
#include <QTemporaryDir>

using namespace LinuxTransferPolicy;

namespace {

int fail(const QString &message)
{
    qCritical().noquote() << message;
    return 1;
}

Endpoint endpoint(const QString &mount,
                  const QString &fileSystem,
                  const QString &device,
                  const QString &physical,
                  StorageType storage = StorageType::SataSsd,
                  FileSystemClass fileSystemClass = FileSystemClass::OrdinaryLocal)
{
    Endpoint result;
    result.normalizedPath = mount + QStringLiteral("/file");
    result.mountRoot = mount;
    result.fileSystemType = fileSystem;
    result.deviceIdentifier = QStringLiteral("/dev/") + device;
    result.blockDeviceName = device;
    result.physicalDeviceName = physical;
    result.storageType = storage;
    result.fileSystemClass = fileSystemClass;
    result.valid = true;
    return result;
}

struct Case {
    const char *name;
    Endpoint source;
    Endpoint destination;
    Mode expected;
    const char *reason;
};

} // namespace

int main()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        return fail(QStringLiteral("could not create discovery fixture"));
    }
    const Endpoint existingEndpoint = discoverEndpoint(temporaryDirectory.path());
    const Endpoint missingEndpoint = discoverEndpoint(temporaryDirectory.filePath(QStringLiteral("missing/target.bin")));
    if (!missingEndpoint.valid
        || missingEndpoint.mountRoot != existingEndpoint.mountRoot
        || missingEndpoint.fileSystemType != existingEndpoint.fileSystemType
        || missingEndpoint.deviceIdentifier != existingEndpoint.deviceIdentifier) {
        return fail(QStringLiteral("nonexistent destination did not inherit its existing parent's endpoint"));
    }

    const QList<Case> cases{
        {"same ext4 mount", endpoint("/data", "ext4", "sdb1", "sdb"), endpoint("/data", "ext4", "sdb1", "sdb"), Mode::Optimized, "same-mount"},
        {"same btrfs mount", endpoint("/", "btrfs", "sda4", "sda"), endpoint("/", "btrfs", "sda4", "sda"), Mode::Optimized, "same-mount"},
        {"ext4 independent SSDs", endpoint("/a", "ext4", "sda1", "sda"), endpoint("/b", "ext4", "sdb1", "sdb"), Mode::Balanced, "independent-local-devices"},
        {"btrfs independent SSDs", endpoint("/a", "btrfs", "sda1", "sda"), endpoint("/b", "btrfs", "nvme0n1p1", "nvme0n1", StorageType::NvmeSsd), Mode::Balanced, "independent-local-devices"},
        {"partitions on one SSD", endpoint("/a", "ext4", "sda1", "sda"), endpoint("/b", "ext4", "sda2", "sda"), Mode::Conservative, "same-physical-device-cross-mount"},
        {"NTFS to Btrfs", endpoint("/a", "ntfs", "sda1", "sda", StorageType::SataSsd, FileSystemClass::ForeignFuse), endpoint("/b", "btrfs", "sdb1", "sdb"), Mode::Conservative, "foreign-or-fuse"},
        {"NTFS to ext4", endpoint("/a", "ntfs", "sda1", "sda", StorageType::SataSsd, FileSystemClass::ForeignFuse), endpoint("/b", "ext4", "sdb1", "sdb"), Mode::Conservative, "foreign-or-fuse"},
        {"FUSE destination", endpoint("/a", "ext4", "sda1", "sda"), endpoint("/b", "fuse.test", {}, {}, StorageType::SataSsd, FileSystemClass::ForeignFuse), Mode::Conservative, "foreign-or-fuse"},
        {"network destination", endpoint("/a", "ext4", "sda1", "sda"), endpoint("/net", "nfs4", {}, {}, StorageType::Network, FileSystemClass::NetworkLike), Mode::Conservative, "network-like"},
        {"unresolved topology", endpoint("/a", "ext4", "sda1", {}), endpoint("/b", "ext4", "sdb1", "sdb"), Mode::Conservative, "unknown-topology"},
        {"different native filesystems", endpoint("/a", "ext4", "sda1", "sda"), endpoint("/b", "btrfs", "sdb1", "sdb"), Mode::Conservative, "different-filesystem-type"},
    };

    for (const Case &test : cases) {
        const Decision result = decide(test.source, test.destination);
        if (result.mode != test.expected || result.reason != QLatin1String(test.reason)) {
            return fail(QStringLiteral("%1: got mode=%2 reason=%3")
                            .arg(QLatin1String(test.name), modeName(result.mode), result.reason));
        }
    }

    Endpoint nvme = endpoint("/nvme", "ext4", "nvme0n1p1", "nvme0n1", StorageType::NvmeSsd);
    const Decision optimized = decide(nvme, nvme);
    if (optimized.copyBufferLimit != 1024 * 1024
        || !optimized.boundedCacheWriteback || optimized.archiveDutyCycle
        || optimized.copyIoPriority != IoPriority::ReducedBestEffort
        || optimized.archiveNice != 0 || optimized.archiveIoPriority != IoPriority::Normal) {
        return fail(QStringLiteral("optimized policy details are incorrect"));
    }

    const Decision balanced = decide(endpoint("/a", "ext4", "sda1", "sda", StorageType::SataSsd),
                                     endpoint("/b", "ext4", "sdb1", "sdb", StorageType::Hdd));
    if (balanced.copyBufferLimit != 512 * 1024
        || !balanced.boundedCacheWriteback || balanced.archiveDutyCycle
        || balanced.copyIoPriority != IoPriority::ReducedBestEffort
        || balanced.archiveNice != 19 || balanced.archiveIoPriority != IoPriority::ReducedBestEffort) {
        return fail(QStringLiteral("balanced policy details are incorrect"));
    }

    const Decision safeFallback = decide(Endpoint{}, endpoint("/b", "ext4", "sdb1", "sdb"));
    if (safeFallback.copyBufferLimit != 1024 * 1024
        || !safeFallback.boundedCacheWriteback || !safeFallback.archiveDutyCycle
        || safeFallback.copyIoPriority != IoPriority::ReducedBestEffort
        || safeFallback.archiveNice != 19 || safeFallback.archiveIoPriority != IoPriority::Idle) {
        return fail(QStringLiteral("conservative fallback details are incorrect"));
    }
    return 0;
}
