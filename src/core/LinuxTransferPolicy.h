#pragma once

#include <QString>

namespace LinuxTransferPolicy {

enum class StorageType {
    Unknown,
    Hdd,
    SataSsd,
    NvmeSsd,
    Usb,
    Network,
    Optical,
};

enum class FileSystemClass {
    Unknown,
    OrdinaryLocal,
    ForeignFuse,
    NetworkLike,
};

enum class Mode {
    Optimized,
    Balanced,
    Conservative,
};

enum class IoPriority {
    Normal,
    ReducedBestEffort,
    Idle,
};

struct Endpoint {
    QString normalizedPath;
    QString mountRoot;
    QString fileSystemType;
    QString deviceIdentifier;
    QString blockDeviceName;
    QString physicalDeviceName;
    StorageType storageType = StorageType::Unknown;
    FileSystemClass fileSystemClass = FileSystemClass::Unknown;
    bool valid = false;
};

struct Decision {
    Mode mode = Mode::Conservative;
    QString reason;
    qint64 copyBufferLimit = 1024 * 1024;
    bool boundedCacheWriteback = true;
    IoPriority copyIoPriority = IoPriority::ReducedBestEffort;
    int archiveNice = 19;
    IoPriority archiveIoPriority = IoPriority::Idle;
    bool archiveDutyCycle = true;
};

Endpoint discoverEndpoint(const QString &path);
Decision decide(const Endpoint &source, const Endpoint &destination);
void traceDecision(const char *operation, const Endpoint &source, const Endpoint &destination, const Decision &decision);

QString modeName(Mode mode);
QString storageTypeName(StorageType type);
QString ioPriorityName(IoPriority priority);

} // namespace LinuxTransferPolicy
