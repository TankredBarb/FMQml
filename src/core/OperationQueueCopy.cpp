#include "OperationQueue.h"
#include "OperationQueuePrivate.h"

#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#include "CleanupSubsystem.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QScopeGuard>
#include <QSet>
#include <QStorageInfo>
#include <QUuid>
#include <QVector>

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

using OperationQueuePrivate::allocateNeutralProviderTransferFile;
using OperationQueuePrivate::allocateProviderTransferFile;
using OperationQueuePrivate::CopyFrame;
using OperationQueuePrivate::mibPerSecond;
using OperationQueuePrivate::normalizedPath;
using OperationQueuePrivate::pathLogName;
using OperationQueuePrivate::providerFailureReason;
using OperationQueuePrivate::providerMaterializeLoggingEnabled;
using OperationQueuePrivate::ProviderUnknownSizeProgressBytes;
using OperationQueuePrivate::SmallFileLimit;
using OperationQueuePrivate::samePath;
using OperationQueuePrivate::isDescendantPath;

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace {
constexpr qint64 DirectArchiveExtractThreshold = 64 * 1024 * 1024;
constexpr qint64 CopyProgressUpdateIntervalMs = 100;
constexpr qint64 LinuxCrossFilesystemCopyBufferSize = 1 * 1024 * 1024;
constexpr qint64 LinuxCrossFilesystemCopyCacheWindow = 32 * 1024 * 1024;
}


#ifdef Q_OS_LINUX
namespace {
// Best-effort, lowest priority within the class. The UI thread keeps the
// default priority 4, so its readdir/stat calls are served preferentially by
// the I/O scheduler (especially effective with BFQ on HDD/USB).
constexpr int LinuxIoPrioClassShift = 13;
constexpr int LinuxIoPrioClassBE = 2;
constexpr int LinuxIoPrioWhoThread = 1;
constexpr int LinuxIoPrioBELowest = 7;

int linuxThreadIoPriority()
{
    return static_cast<int>(syscall(SYS_ioprio_get, LinuxIoPrioWhoThread, 0));
}

bool setLinuxThreadIoPriority(int value)
{
    return syscall(SYS_ioprio_set, LinuxIoPrioWhoThread, 0, value) == 0;
}

int linuxIoPrioValue(int ioClass, int priority)
{
    return (ioClass << LinuxIoPrioClassShift) | priority;
}
} // namespace

class LinuxIoPriorityGuard
{
public:
    LinuxIoPriorityGuard()
        : m_previousPriority(linuxThreadIoPriority())
        , m_hasPreviousPriority(m_previousPriority >= 0)
        , m_changed(setLinuxThreadIoPriority(linuxIoPrioValue(LinuxIoPrioClassBE, LinuxIoPrioBELowest)))
    {
    }

    ~LinuxIoPriorityGuard()
    {
        if (m_changed && m_hasPreviousPriority) {
            (void)setLinuxThreadIoPriority(m_previousPriority);
        }
    }

    LinuxIoPriorityGuard(const LinuxIoPriorityGuard &) = delete;
    LinuxIoPriorityGuard &operator=(const LinuxIoPriorityGuard &) = delete;

private:
    const int m_previousPriority = -1;
    const bool m_hasPreviousPriority = false;
    const bool m_changed = false;
};

class LinuxCopyCachePolicy
{
public:
    LinuxCopyCachePolicy(QIODevice *source, QIODevice *destination)
        : m_sourceFd(fileDescriptor(source))
        , m_destinationFd(fileDescriptor(destination))
    {
        if (m_sourceFd >= 0) {
            (void)posix_fadvise(m_sourceFd, 0, 0, POSIX_FADV_SEQUENTIAL);
        }
    }

    void bytesCopied(qint64 bytes)
    {
        if (bytes <= 0) {
            return;
        }

        m_position += bytes;
        const qint64 window = LinuxCrossFilesystemCopyCacheWindow;
        const qint64 readyUntil = (m_position / window) * window;
        while (m_advisedUntil + window <= readyUntil) {
            const qint64 windowStart = m_advisedUntil;

            // Pipeline: before starting writeback for the current window, wait
            // for the previous window's writeback to complete. On fast disks
            // this usually returns immediately; on slow ones it applies
            // backpressure to keep dirty pages bounded and UI I/O responsive.
            if (windowStart > 0) {
                const qint64 prevStart = windowStart - window;
                awaitWriteback(prevStart, window);
                dropDestPages(prevStart, window);
            }

            startDestinationWriteback(windowStart, window);
            dropSourcePages(windowStart, window);

            m_advisedUntil += window;
        }
    }

    void finish()
    {
        const qint64 window = LinuxCrossFilesystemCopyCacheWindow;

        if (m_advisedUntil > 0) {
            const qint64 lastStart = m_advisedUntil - window;
            awaitWriteback(lastStart, window);
            dropDestPages(lastStart, window);
        }

        if (m_position <= m_advisedUntil) {
            return;
        }

        const qint64 tailStart = m_advisedUntil;
        const qint64 tailSize = m_position - m_advisedUntil;
        startDestinationWriteback(tailStart, tailSize);
        dropSourcePages(tailStart, tailSize);
        awaitWriteback(tailStart, tailSize);
        dropDestPages(tailStart, tailSize);
        m_advisedUntil = m_position;
    }

private:
    static int fileDescriptor(QIODevice *device)
    {
        auto *file = qobject_cast<QFile *>(device);
        if (!file || !file->isOpen()) {
            return -1;
        }
        return file->handle();
    }

    void awaitWriteback(qint64 offset, qint64 length) const
    {
        if (m_destinationFd < 0) {
            return;
        }
        // This is still a best-effort cache/writeback policy; copy correctness
        // is enforced by normal write/flush/close error handling.
        (void)sync_file_range(m_destinationFd,
                              static_cast<off64_t>(offset),
                              static_cast<off64_t>(length),
                              SYNC_FILE_RANGE_WAIT_BEFORE
                                  | SYNC_FILE_RANGE_WRITE
                                  | SYNC_FILE_RANGE_WAIT_AFTER);
    }

    void startDestinationWriteback(qint64 offset, qint64 length) const
    {
        if (m_destinationFd < 0) {
            return;
        }
        (void)sync_file_range(m_destinationFd,
                              static_cast<off64_t>(offset),
                              static_cast<off64_t>(length),
                              SYNC_FILE_RANGE_WRITE);
    }

    void dropSourcePages(qint64 offset, qint64 length) const
    {
        if (m_sourceFd >= 0) {
            (void)posix_fadvise(m_sourceFd,
                                static_cast<off_t>(offset),
                                static_cast<off_t>(length),
                                POSIX_FADV_DONTNEED);
        }
    }

    void dropDestPages(qint64 offset, qint64 length) const
    {
        if (m_destinationFd >= 0) {
            (void)posix_fadvise(m_destinationFd,
                                static_cast<off_t>(offset),
                                static_cast<off_t>(length),
                                POSIX_FADV_DONTNEED);
        }
    }

    const int m_sourceFd = -1;
    const int m_destinationFd = -1;
    qint64 m_position = 0;
    qint64 m_advisedUntil = 0;
};

bool isLinuxCrossFilesystemCopy(const QString &sourcePath, const QString &targetPath)
{
    const QFileInfo sourceInfo(sourcePath);
    const QFileInfo targetInfo(targetPath);
    QStorageInfo sourceStorage(sourceInfo.absoluteFilePath());
    QStorageInfo targetStorage(targetInfo.absolutePath());
    sourceStorage.refresh();
    targetStorage.refresh();

    if (!sourceStorage.isValid() || !targetStorage.isValid()) {
        return false;
    }

    return sourceStorage.device() != targetStorage.device()
        || sourceStorage.rootPath() != targetStorage.rootPath();
}
#endif

qint64 OperationQueue::totalBytesFor(const QStringList &sources) const
{
    qint64 total = 0;
    for (const QString &source : sources) {
        if (m_abort) {
            break;
        }
        total += totalBytesForPath(source);
    }
    return total;
}


qint64 OperationQueue::totalBytesForPath(const QString &path) const
{
    qint64 total = 0;
    QVector<QString> stack;
    QSet<QString> visitedDirectories;
    stack.push_back(path);

    while (!stack.isEmpty()) {
        if (m_abort) {
            break;
        }

        const QString currentPath = stack.back();
        stack.pop_back();

        FileProvider* provider = getProviderForPath(currentPath);
        const std::optional<FileEntry> info = provider->entryInfo(currentPath);
        if (!info) {
            continue;
        }

        if (!info->isDirectory || provider->isSymLink(currentPath)) {
            if (info->size > 0) {
                total += info->size;
            } else if (provider->scheme() != QLatin1String("file")) {
                total += ProviderUnknownSizeProgressBytes;
            }
            continue;
        }

        const QString normalizedCurrent = normalizedPath(*provider, currentPath);
        if (visitedDirectories.contains(normalizedCurrent)) {
            continue;
        }
        visitedDirectories.insert(normalizedCurrent);

        const QStringList children = childPaths(currentPath);
        for (const QString &child : children) {
            if (m_abort) {
                break;
            }
            stack.push_back(child);
        }
    }
    return total;
}


void OperationQueue::copyPath(const QString &sourcePath,
                              const QString &destinationPath,
                              qint64 totalBytes,
                              qint64 &copiedBytes,
                              Type labelType,
                              bool replaceExactDestination)
{
    if (m_abort) return;

    if (copyLocalDirectoryToProviderBatch(sourcePath, destinationPath, totalBytes, copiedBytes)) {
        return;
    }

    if (copyProviderDirectoryToProviderStagedBatch(sourcePath, destinationPath, totalBytes, copiedBytes)) {
        return;
    }

    if (copyProviderDirectoryToLocalBatch(sourcePath, destinationPath, totalBytes, copiedBytes)) {
        return;
    }

    QVector<CopyFrame> stack;
    stack.push_back({sourcePath, destinationPath});

    while (!stack.isEmpty()) {
        if (m_abort) return;

        const CopyFrame frame = stack.back();
        stack.pop_back();

        FileProvider* srcProvider = getProviderForPath(frame.sourcePath);
        FileProvider* destProvider = getProviderForPath(frame.destinationPath);

        const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(frame.sourcePath);
        const QString fileName = destinationNameForCopy(srcProvider, frame.sourcePath);

        if (!srcProvider->canCopyPath(frame.sourcePath)) {
            throw std::runtime_error(QStringLiteral("Cannot copy %1 from this location")
                                         .arg(frame.sourcePath)
                                         .toStdString());
        }

        const QString label = srcProvider->scheme() == QLatin1String("file")
            && destProvider->scheme() == QLatin1String("file")
            ? OperationQueuePrivate::operationItemLabel(labelType, fileName)
            : fileName;
        QMetaObject::invokeMethod(this, [this, label]() {
            setCurrentLabel(label);
        }, Qt::QueuedConnection);

        if (srcProvider == destProvider && samePath(*srcProvider, frame.sourcePath, frame.destinationPath)) {
            copiedBytes += totalBytesForPath(frame.sourcePath);
            QMetaObject::invokeMethod(this, [this]() {
                setStatusMessage("Some files skipped (source is same as destination)");
            }, Qt::QueuedConnection);
            continue;
        }

        QString targetPath = frame.destinationPath;
        if (pathExists(targetPath)) {
            if (replaceExactDestination) {
                if (!removePathIfExists(targetPath)) {
                    throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(targetPath).toStdString());
                }
            } else {
                ConflictResolution res = waitForResolution(frame.sourcePath, targetPath);
                if (res == ConflictResolution::Skip) {
                    copiedBytes += totalBytesForPath(frame.sourcePath);
                    continue;
                } else if (res == ConflictResolution::KeepBoth) {
                    targetPath = uniqueDestinationPath(targetPath);
                } else if (res == ConflictResolution::Replace) {
                    // Safety check: is targetPath the source archive itself (or its parent)?
                    if (ArchiveSupport::isArchivePath(frame.sourcePath)) {
                        QString physicalPath = ArchiveSupport::physicalArchivePath(frame.sourcePath);
                        FileProvider* localProvider = getProviderForPath(physicalPath);
                        if (samePath(*localProvider, targetPath, physicalPath) || isDescendantPath(*localProvider, physicalPath, targetPath)) {
                            // Override Replace with KeepBoth to prevent destroying the source archive
                            res = ConflictResolution::KeepBoth;
                            targetPath = uniqueDestinationPath(targetPath);
                            QMetaObject::invokeMethod(this, [this]() {
                                setStatusMessage("Cannot replace the source archive. The item has been renamed.");
                            }, Qt::QueuedConnection);
                        }
                    }

                    if (res == ConflictResolution::Replace) {
                        if (!removePathIfExists(targetPath)) {
                            throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(targetPath).toStdString());
                        }
                    }
                } else if (res == ConflictResolution::Cancel) {
                    m_abort = true;
                    return;
                }
            }
        }

        if (m_abort) return;

        if (isRealDirectory(frame.sourcePath)) {
            if (srcProvider == destProvider && isDescendantPath(*srcProvider, targetPath, frame.sourcePath)) {
                throw std::runtime_error(
                    QStringLiteral("Cannot copy folder %1 into itself or one of its subfolders")
                        .arg(frame.sourcePath)
                        .toStdString());
            }

            if (!makePath(targetPath)) {
                throw std::runtime_error(QStringLiteral("Cannot create folder %1").arg(targetPath).toStdString());
            }

            const QStringList children = childPaths(frame.sourcePath);
            for (auto it = children.crbegin(); it != children.crend(); ++it) {
                const QString childDestination = destProvider->childPath(targetPath, destinationNameForCopy(srcProvider, *it));
                stack.push_back({*it, childDestination});
            }
            continue;
        }

        if (!ensureParentDirectory(targetPath)) {
            throw std::runtime_error(providerFailureReason(
                destProvider,
                QStringLiteral("Cannot create parent directory for %1").arg(targetPath)).toStdString());
        }

        const qint64 fileSize = sourceInfo ? sourceInfo->size : 0;
        if (srcProvider->scheme() == QLatin1String("file")
            && destProvider->scheme() != QLatin1String("file")) {
            QString directError;
            qint64 directProcessed = 0;
            const qint64 baseBytes = copiedBytes;
            const qint64 remainingBytes = (std::max<qint64>)(1, totalBytes - baseBytes);
            const qint64 contributionLimit = fileSize > 0 ? fileSize : remainingBytes;
            const bool copiedDirectly = destProvider->copyFromLocalFile(
                frame.sourcePath,
                targetPath,
                [this, baseBytes, contributionLimit, totalBytes, &directProcessed](qint64 processed, qint64 total) -> bool {
                    Q_UNUSED(total)
                    if (m_abort) {
                        return false;
                    }
                    directProcessed = (std::max<qint64>)(0, processed);
                    const qint64 boundedBytes = std::clamp<qint64>(directProcessed, 0, contributionLimit);
                    const qint64 progressBytes = std::clamp<qint64>(baseBytes + boundedBytes, 0, totalBytes);
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(progressBytes, totalBytes);
                    return true;
                },
                &directError);
            if (copiedDirectly) {
                const qint64 contribution = fileSize > 0 ? fileSize : (std::max<qint64>)(1, directProcessed);
                copiedBytes = (std::min)(totalBytes, copiedBytes + contribution);
                const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
                updateMetrics(copiedBytes, totalBytes);
                continue;
            }
            if (!directError.trimmed().isEmpty()) {
                if (m_abort) {
                    return;
                }
                throw std::runtime_error(directError.toStdString());
            }
        }

        if (srcProvider->scheme() != QLatin1String("file")
            && destProvider->scheme() != QLatin1String("file")) {
            const QString sequentialLabel = QStringLiteral("Sequential transfer: %1").arg(fileName);
            QMetaObject::invokeMethod(this, [this, sequentialLabel]() {
                setCurrentLabel(sequentialLabel);
            }, Qt::QueuedConnection);

            const bool timingActive = m_providerTransferTiming.active;
            QElapsedTimer fileTimer;
            if (timingActive) {
                fileTimer.start();
            }
            qint64 allocationMs = 0;
            qint64 stagingMs = 0;
            qint64 uploadMs = 0;
            qint64 cleanupMs = 0;
            qint64 stagedBytesForLog = 0;
            qint64 uploadedBytesForLog = 0;
            QString stagingParentForLog;
            const qint64 baseBytes = copiedBytes;
            const qint64 remainingBytes = (std::max<qint64>)(1, totalBytes - baseBytes);
            const qint64 contributionLimit = fileSize > 0 ? fileSize : remainingBytes;

            auto logProviderTransferFile = [&](const QString &result, const QString &error = {}) {
                if (!timingActive) {
                    return;
                }

                const qint64 elapsedMs = fileTimer.isValid() ? fileTimer.elapsed() : 0;
                ++m_providerTransferTiming.fileCount;
                m_providerTransferTiming.totalBytes += contributionLimit;
                m_providerTransferTiming.stagedBytes += stagedBytesForLog;
                m_providerTransferTiming.uploadedBytes += uploadedBytesForLog;
                m_providerTransferTiming.allocationMs += allocationMs;
                m_providerTransferTiming.stagingMs += stagingMs;
                m_providerTransferTiming.uploadMs += uploadMs;
                m_providerTransferTiming.cleanupMs += cleanupMs;
                if (result == QLatin1String("success")) {
                    ++m_providerTransferTiming.successfulFiles;
                } else if (result == QLatin1String("canceled")) {
                    ++m_providerTransferTiming.canceledFiles;
                } else {
                    ++m_providerTransferTiming.failedFiles;
                }

                qInfo().noquote()
                    << "[ProviderTransferFile]"
                    << "operationId=" << m_providerTransferTiming.operationId
                    << "result=" << result
                    << "sourceScheme=" << srcProvider->scheme()
                    << "destinationScheme=" << destProvider->scheme()
                    << "source=" << pathLogName(frame.sourcePath)
                    << "destination=" << pathLogName(targetPath)
                    << "bytes=" << contributionLimit
                    << "stagedBytes=" << stagedBytesForLog
                    << "uploadedBytes=" << uploadedBytesForLog
                    << "stagingParent=" << stagingParentForLog
                    << "allocationMs=" << allocationMs
                    << "stagingMs=" << stagingMs
                    << "uploadMs=" << uploadMs
                    << "cleanupMs=" << cleanupMs
                    << "totalMs=" << elapsedMs
                    << "stagingMiBs=" << mibPerSecond(stagedBytesForLog, stagingMs)
                    << "uploadMiBs=" << mibPerSecond(uploadedBytesForLog, uploadMs)
                    << "error=" << error.left(160);
            };

            QString transferLeaseId;
            QElapsedTimer allocationTimer;
            if (timingActive) {
                allocationTimer.start();
            }
            const QString stagedPath = allocateProviderTransferFile(frame.destinationPath, fileName, &transferLeaseId);
            if (timingActive) {
                allocationMs = allocationTimer.elapsed();
                stagingParentForLog = QFileInfo(stagedPath).absolutePath();
            }
            if (stagedPath.isEmpty()) {
                logProviderTransferFile(QStringLiteral("failed"), QStringLiteral("Cannot allocate provider transfer staging location"));
                throw std::runtime_error("Cannot allocate provider transfer staging location");
            }
            QFile stagedFileHandle(stagedPath);
            if (!stagedFileHandle.open(QIODevice::WriteOnly)) {
                if (timingActive) {
                    allocationMs = allocationTimer.elapsed();
                }
                const QString error = QStringLiteral("Cannot create transfer file: %1").arg(stagedFileHandle.errorString());
                CleanupSubsystem::instance().scheduleDeleteOnFailure(transferLeaseId);
                logProviderTransferFile(QStringLiteral("failed"), error);
                throw std::runtime_error(error.toStdString());
            }
            stagedFileHandle.close();
            const auto transferCleanup = qScopeGuard([&]() {
                if (!transferLeaseId.isEmpty()) {
                    CleanupSubsystem::instance().scheduleDeleteOnFailure(transferLeaseId);
                }
            });

            qint64 stagedProcessed = 0;
            QString stagingError;
            QElapsedTimer stagingTimer;
            if (timingActive) {
                stagingTimer.start();
            }
            const bool staged = srcProvider->copyToLocalFile(
                frame.sourcePath,
                stagedPath,
                [this, baseBytes, contributionLimit, totalBytes, &stagedProcessed](qint64 processed, qint64 total) -> bool {
                    Q_UNUSED(total)
                    if (m_abort) {
                        return false;
                    }
                    stagedProcessed = (std::max<qint64>)(0, processed);
                    const qint64 boundedBytes = std::clamp<qint64>(stagedProcessed, 0, contributionLimit);
                    const qint64 phaseBytes = boundedBytes / 2;
                    const qint64 progressBytes = std::clamp<qint64>(baseBytes + phaseBytes, 0, totalBytes);
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(progressBytes, totalBytes);
                    return true;
                },
                &stagingError);
            if (timingActive) {
                stagingMs = stagingTimer.elapsed();
                stagedBytesForLog = std::clamp<qint64>(stagedProcessed, 0, contributionLimit);
                if (staged && stagedBytesForLog <= 0) {
                    stagedBytesForLog = contributionLimit;
                }
            }

            if (staged) {
                if (m_abort) {
                    logProviderTransferFile(QStringLiteral("canceled"));
                    return;
                }

                qint64 uploadedProcessed = 0;
                QString uploadError;
                QElapsedTimer uploadTimer;
                if (timingActive) {
                    uploadTimer.start();
                }
                const bool uploaded = destProvider->copyFromLocalFile(
                    stagedPath,
                    targetPath,
                    [this, baseBytes, contributionLimit, totalBytes, &uploadedProcessed](qint64 processed, qint64 total) -> bool {
                        Q_UNUSED(total)
                        if (m_abort) {
                            return false;
                        }
                        uploadedProcessed = (std::max<qint64>)(0, processed);
                        const qint64 boundedBytes = std::clamp<qint64>(uploadedProcessed, 0, contributionLimit);
                        const qint64 phaseBytes = contributionLimit / 2 + (boundedBytes + 1) / 2;
                        const qint64 progressBytes = std::clamp<qint64>(baseBytes + phaseBytes, 0, totalBytes);
                        const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                        QMetaObject::invokeMethod(this, [this, progress]() {
                            setProgress(progress);
                        }, Qt::QueuedConnection);
                        updateMetrics(progressBytes, totalBytes);
                        return true;
                    },
                    &uploadError);
                if (timingActive) {
                    uploadMs = uploadTimer.elapsed();
                    uploadedBytesForLog = std::clamp<qint64>(uploadedProcessed, 0, contributionLimit);
                    if (uploaded && uploadedBytesForLog <= 0) {
                        uploadedBytesForLog = contributionLimit;
                    }
                }

                if (uploaded) {
                    const qint64 contribution = fileSize > 0
                        ? fileSize
                        : (std::max<qint64>)(1, (std::max)(stagedProcessed, uploadedProcessed));
                    copiedBytes = (std::min)(totalBytes, copiedBytes + contribution);
                    const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(copiedBytes, totalBytes);
                    QElapsedTimer cleanupTimer;
                    if (timingActive) {
                        cleanupTimer.start();
                    }
                    CleanupSubsystem::instance().scheduleDelete(transferLeaseId);
                    if (timingActive) {
                        cleanupMs = cleanupTimer.elapsed();
                    }
                    transferLeaseId.clear();
                    logProviderTransferFile(QStringLiteral("success"));
                    continue;
                }

                if (!uploadError.trimmed().isEmpty()) {
                    if (m_abort) {
                        logProviderTransferFile(QStringLiteral("canceled"), uploadError);
                        return;
                    }
                    logProviderTransferFile(QStringLiteral("failed"), uploadError);
                    throw std::runtime_error(uploadError.toStdString());
                }
            } else if (!stagingError.trimmed().isEmpty()) {
                if (m_abort) {
                    logProviderTransferFile(QStringLiteral("canceled"), stagingError);
                    return;
                }
                logProviderTransferFile(QStringLiteral("failed"), stagingError);
                throw std::runtime_error(stagingError.toStdString());
            }
        }

        const QString tempPath = targetPath + QStringLiteral(".part");
        struct PartCleanup {
            QString leaseId;
            bool finalized = false;
            ~PartCleanup()
            {
                if (leaseId.isEmpty()) {
                    return;
                }
                if (finalized) {
                    CleanupSubsystem::instance().completeWithoutDelete(leaseId);
                } else {
                    CleanupSubsystem::instance().scheduleDeleteOnFailure(leaseId);
                }
            }
        } partCleanup;
        if (destProvider->scheme() == QLatin1String("file")) {
            CleanupSubsystem::instance().registerArtifact(
                CleanupArtifactKind::PartFile,
                tempPath,
                QFileInfo(tempPath).absolutePath(),
                false,
                &partCleanup.leaseId);
        }
        if (pathExists(tempPath) && !removePathIfExists(tempPath)) {
            throw std::runtime_error(providerFailureReason(
                destProvider,
                QStringLiteral("Cannot replace temporary file %1").arg(tempPath)).toStdString());
        }

        if (destProvider->scheme() == QLatin1String("file")) {
            if (srcProvider->scheme() == QLatin1String("portable")) {
                QString stagingLeaseId;
                const QString stagedPath = allocateNeutralProviderTransferFile(fileName, &stagingLeaseId);
                if (stagedPath.isEmpty()) {
                    throw std::runtime_error("Cannot allocate portable transfer staging location");
                }
                const auto stagingCleanup = qScopeGuard([&]() {
                    if (!stagingLeaseId.isEmpty()) {
                        CleanupSubsystem::instance().scheduleDeleteOnFailure(stagingLeaseId);
                    }
                });

                qint64 stagedProcessed = 0;
                qint64 finalProcessed = 0;
                const qint64 baseBytes = copiedBytes;
                const qint64 remainingBytes = (std::max<qint64>)(1, totalBytes - baseBytes);
                const qint64 contributionLimit = fileSize > 0 ? fileSize : remainingBytes;
                const bool materializeLoggingActive = providerMaterializeLoggingEnabled();
                QElapsedTimer materializeTimer;
                if (materializeLoggingActive) {
                    materializeTimer.start();
                }

                if (m_abort) {
                    return;
                }

                QString stagingError;
                const bool staged = srcProvider->copyToLocalFile(
                    frame.sourcePath,
                    stagedPath,
                    [this, baseBytes, contributionLimit, totalBytes, &stagedProcessed](qint64 processed, qint64 total) -> bool {
                        Q_UNUSED(total)
                        if (m_abort) {
                            return false;
                        }
                        stagedProcessed = (std::max<qint64>)(0, processed);
                        const qint64 boundedBytes = std::clamp<qint64>(stagedProcessed, 0, contributionLimit);
                        const qint64 progressBytes = std::clamp<qint64>(baseBytes + boundedBytes / 2, 0, totalBytes);
                        const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                        QMetaObject::invokeMethod(this, [this, progress]() {
                            setProgress(progress);
                        }, Qt::QueuedConnection);
                        return true;
                    },
                    &stagingError);
                if (!staged) {
                    if (m_abort) {
                        return;
                    }
                    throw std::runtime_error(stagingError.trimmed().isEmpty()
                                                 ? "Portable device staging failed"
                                                 : stagingError.toStdString());
                }

                if (m_abort) {
                    return;
                }

                QFile stagedFile(stagedPath);
                if (!stagedFile.open(QIODevice::ReadOnly)) {
                    throw std::runtime_error(QStringLiteral("Cannot read staged portable file: %1")
                                                 .arg(stagedFile.errorString())
                                                 .toStdString());
                }

                QFile outputFile(tempPath);
                if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    throw std::runtime_error(QStringLiteral("Cannot create temporary file %1: %2")
                                                 .arg(tempPath, outputFile.errorString())
                                                 .toStdString());
                }

                qint64 bufferSize = OperationQueuePrivate::bufferSizeForStorageType(getDriveTypeByPath(targetPath));
#ifdef Q_OS_LINUX
                bufferSize = (std::min)(bufferSize, LinuxCrossFilesystemCopyBufferSize);
                LinuxIoPriorityGuard linuxIoPriorityGuard;
                LinuxCopyCachePolicy linuxCopyCachePolicy(&stagedFile, &outputFile);
#endif
                QByteArray buffer;
                buffer.resize(static_cast<int>(bufferSize));
                QElapsedTimer progressPostTimer;
                progressPostTimer.start();
                auto postProgressIfDue = [this, &progressPostTimer](double progress, bool force = false) {
                    if (!force && progressPostTimer.elapsed() < CopyProgressUpdateIntervalMs) {
                        return;
                    }
                    progressPostTimer.restart();
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                };

                while (!stagedFile.atEnd()) {
                    if (m_abort) {
                        outputFile.close();
                        removePathIfExists(tempPath);
                        return;
                    }

                    const qint64 read = stagedFile.read(buffer.data(), buffer.size());
                    if (read < 0) {
                        const QString error = stagedFile.errorString();
                        outputFile.close();
                        removePathIfExists(tempPath);
                        throw std::runtime_error(QStringLiteral("Read failed: %1").arg(error).toStdString());
                    }
                    if (outputFile.write(buffer.constData(), read) != read) {
                        const QString error = outputFile.errorString();
                        outputFile.close();
                        removePathIfExists(tempPath);
                        throw std::runtime_error(QStringLiteral("Write failed: %1").arg(error).toStdString());
                    }
                    finalProcessed += read;
#ifdef Q_OS_LINUX
                    linuxCopyCachePolicy.bytesCopied(read);
#endif
                    const qint64 boundedBytes = std::clamp<qint64>(finalProcessed, 0, contributionLimit);
                    const qint64 progressBytes = std::clamp<qint64>(
                        baseBytes + contributionLimit / 2 + (boundedBytes + 1) / 2,
                        0,
                        totalBytes);
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                    postProgressIfDue(progress);
                    updateMetrics(progressBytes, totalBytes);
                }

#ifdef Q_OS_LINUX
                linuxCopyCachePolicy.finish();
#endif
                if (!outputFile.flush()) {
                    const QString error = outputFile.errorString();
                    outputFile.close();
                    removePathIfExists(tempPath);
                    throw std::runtime_error(QStringLiteral("Flush failed: %1").arg(error).toStdString());
                }
                outputFile.close();
                stagedFile.close();

                const qint64 contribution = fileSize > 0
                    ? fileSize
                    : (std::max<qint64>)(1, (std::max)(stagedProcessed, finalProcessed));
                if (materializeLoggingActive) {
                    const qint64 elapsedMs = materializeTimer.isValid() ? materializeTimer.elapsed() : 0;
                    const qint64 stagedBytesForLog = stagedProcessed > 0
                        ? std::clamp<qint64>(stagedProcessed, 0, contribution)
                        : contribution;
                    qInfo().noquote()
                        << "[ProviderMaterializeFile]"
                        << "operationId=" << m_providerTransferTiming.operationId
                        << "sourceScheme=" << srcProvider->scheme()
                        << "destinationScheme=" << destProvider->scheme()
                        << "index=" << 1
                        << "waveFiles=" << 1
                        << "source=" << pathLogName(frame.sourcePath)
                        << "destination=" << pathLogName(targetPath)
                        << "bytes=" << contribution
                        << "stagedBytes=" << stagedBytesForLog
                        << "elapsedMs=" << elapsedMs
                        << "throughputMiBs=" << mibPerSecond(stagedBytesForLog, elapsedMs);
                }

                copiedBytes = (std::min)(totalBytes, copiedBytes + contribution);
                const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
                postProgressIfDue(progress, true);
                updateMetrics(copiedBytes, totalBytes);

                if (m_abort) {
                    removePathIfExists(tempPath);
                    return;
                }
                if (pathExists(targetPath) && !removePathIfExists(targetPath)) {
                    removePathIfExists(tempPath);
                    throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(targetPath).toStdString());
                }
                if (!destProvider->movePath(tempPath, targetPath)) {
                    removePathIfExists(tempPath);
                    throw std::runtime_error(QStringLiteral("Cannot finalize %1").arg(targetPath).toStdString());
                }
                CleanupSubsystem::instance().scheduleDelete(stagingLeaseId);
                stagingLeaseId.clear();
                partCleanup.finalized = true;
                continue;
            }

            QString directError;
            qint64 directProcessed = 0;
            const qint64 baseBytes = copiedBytes;
            const qint64 remainingBytes = (std::max<qint64>)(1, totalBytes - baseBytes);
            const qint64 contributionLimit = fileSize > 0 ? fileSize : remainingBytes;
            const bool materializeLoggingActive = providerMaterializeLoggingEnabled();
            QElapsedTimer materializeTimer;
            if (materializeLoggingActive) {
                materializeTimer.start();
            }
            const bool copiedDirectly = srcProvider->copyToLocalFile(
                frame.sourcePath,
                tempPath,
                [this, baseBytes, contributionLimit, totalBytes, &directProcessed](qint64 processed, qint64 total) -> bool {
                    Q_UNUSED(total)
                    if (m_abort) {
                        return false;
                    }
                    directProcessed = (std::max<qint64>)(0, processed);
                    const qint64 boundedBytes = std::clamp<qint64>(directProcessed, 0, contributionLimit);
                    const qint64 progressBytes = std::clamp<qint64>(baseBytes + boundedBytes, 0, totalBytes);
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(progressBytes, totalBytes);
                    return true;
                },
                &directError);
            if (copiedDirectly) {
                const qint64 contribution = fileSize > 0 ? fileSize : (std::max<qint64>)(1, directProcessed);
                if (materializeLoggingActive) {
                    const qint64 elapsedMs = materializeTimer.isValid() ? materializeTimer.elapsed() : 0;
                    const qint64 stagedBytesForLog = directProcessed > 0
                        ? std::clamp<qint64>(directProcessed, 0, contribution)
                        : contribution;
                    qInfo().noquote()
                        << "[ProviderMaterializeFile]"
                        << "operationId=" << m_providerTransferTiming.operationId
                        << "sourceScheme=" << srcProvider->scheme()
                        << "destinationScheme=" << destProvider->scheme()
                        << "index=" << 1
                        << "waveFiles=" << 1
                        << "source=" << pathLogName(frame.sourcePath)
                        << "destination=" << pathLogName(targetPath)
                        << "bytes=" << contribution
                        << "stagedBytes=" << stagedBytesForLog
                        << "elapsedMs=" << elapsedMs
                        << "throughputMiBs=" << mibPerSecond(stagedBytesForLog, elapsedMs);
                }
                copiedBytes = (std::min)(totalBytes, copiedBytes + contribution);
                const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
                updateMetrics(copiedBytes, totalBytes);

                if (m_abort) {
                    removePathIfExists(tempPath);
                    return;
                }
                if (pathExists(targetPath) && !removePathIfExists(targetPath)) {
                    removePathIfExists(tempPath);
                    throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(targetPath).toStdString());
                }
                if (!destProvider->movePath(tempPath, targetPath)) {
                    removePathIfExists(tempPath);
                    throw std::runtime_error(QStringLiteral("Cannot finalize %1").arg(targetPath).toStdString());
                }
                partCleanup.finalized = true;
                continue;
            }
            if (!directError.trimmed().isEmpty()) {
                removePathIfExists(tempPath);
                if (m_abort) {
                    return;
                }
                throw std::runtime_error(directError.toStdString());
            }
        }

        if (ArchiveSupport::isArchivePath(frame.sourcePath)
            && destProvider->scheme() == QLatin1String("file")
            && fileSize >= DirectArchiveExtractThreshold) {
            QString error;
            const qint64 baseBytes = copiedBytes;
            const bool extracted = ArchiveFileProvider::extractArchiveEntryTo(
                frame.sourcePath,
                tempPath,
                &error,
                [this, baseBytes, fileSize, totalBytes](uint64_t processed) -> bool {
                    if (m_abort) {
                        return false;
                    }
                    const uint64_t maxBytes = static_cast<uint64_t>((std::numeric_limits<qint64>::max)());
                    const qint64 clampedBytes = std::clamp<qint64>(
                        static_cast<qint64>((std::min)(processed, maxBytes)),
                        0,
                        fileSize);
                    const qint64 progressBytes = baseBytes + clampedBytes;
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(progressBytes, totalBytes);
                    return true;
                });
            if (!extracted) {
                removePathIfExists(tempPath);
                if (m_abort) {
                    return;
                }
                throw std::runtime_error(error.isEmpty()
                    ? QStringLiteral("Cannot read %1").arg(frame.sourcePath).toStdString()
                    : error.toStdString());
            }

            copiedBytes += fileSize;
            if (m_abort) {
                removePathIfExists(tempPath);
                return;
            }
            if (pathExists(targetPath) && !removePathIfExists(targetPath)) {
                removePathIfExists(tempPath);
                throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(targetPath).toStdString());
            }
            if (!destProvider->movePath(tempPath, targetPath)) {
                removePathIfExists(tempPath);
                throw std::runtime_error(QStringLiteral("Cannot finalize %1").arg(targetPath).toStdString());
            }
            partCleanup.finalized = true;
            continue;
        }

        const QString sourceStagingParent = destProvider->scheme() == QLatin1String("file")
            ? destProvider->parentPath(tempPath)
            : QString{};
        std::unique_ptr<QIODevice> source = srcProvider->openRead(frame.sourcePath, sourceStagingParent);
        if (!source) {
            if (m_abort) {
                return;
            }
            throw std::runtime_error(providerFailureReason(
                srcProvider,
                QStringLiteral("Cannot read %1").arg(frame.sourcePath)).toStdString());
        }

        std::unique_ptr<QIODevice> destination = destProvider->openWrite(tempPath, true);
        if (!destination) {
            throw std::runtime_error(providerFailureReason(
                destProvider,
                QStringLiteral("Cannot write %1").arg(targetPath)).toStdString());
        }

        if (fileSize <= SmallFileLimit) {
            const QByteArray data = source->readAll();
            if (destination->write(data) != data.size()) {
                destination->close();
                destProvider->removePath(tempPath);
                throw std::runtime_error(QStringLiteral("Write failed: %1").arg(targetPath).toStdString());
            }
            copiedBytes += data.size();

            const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
            QMetaObject::invokeMethod(this, [this, progress]() {
                setProgress(progress);
            }, Qt::QueuedConnection);
            updateMetrics(copiedBytes, totalBytes);
        } else {
            QByteArray buffer;
            qint64 bufferSize = OperationQueuePrivate::bufferSizeForStorageType(getDriveTypeByPath(targetPath));
#ifdef Q_OS_LINUX
            bool conservativeLinuxCopy = srcProvider->scheme() == QLatin1String("file")
                && destProvider->scheme() == QLatin1String("file")
                && isLinuxCrossFilesystemCopy(frame.sourcePath, targetPath);
            if (conservativeLinuxCopy) {
                bufferSize = (std::min)(bufferSize, LinuxCrossFilesystemCopyBufferSize);
            }
#endif

            buffer.resize(static_cast<int>(bufferSize));
#ifdef Q_OS_LINUX
            std::unique_ptr<LinuxIoPriorityGuard> linuxIoPriorityGuard;
            std::unique_ptr<LinuxCopyCachePolicy> linuxCopyCachePolicy;
            if (conservativeLinuxCopy) {
                linuxIoPriorityGuard = std::make_unique<LinuxIoPriorityGuard>();
                linuxCopyCachePolicy = std::make_unique<LinuxCopyCachePolicy>(source.get(), destination.get());
            }
#endif
            QElapsedTimer progressPostTimer;
            progressPostTimer.start();
            auto postProgressIfDue = [this, &progressPostTimer](double progress, bool force = false) {
                if (!force && progressPostTimer.elapsed() < CopyProgressUpdateIntervalMs) {
                    return;
                }
                progressPostTimer.restart();
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
            };

            while (!source->atEnd()) {
                if (m_abort) {
                    destination->close();
                    destProvider->removePath(tempPath);
                    return;
                }

                const qint64 read = source->read(buffer.data(), buffer.size());
                if (read < 0) {
                    destination->close();
                    destProvider->removePath(tempPath);
                    throw std::runtime_error(QStringLiteral("Read failed: %1").arg(frame.sourcePath).toStdString());
                }
                if (destination->write(buffer.constData(), read) != read) {
                    destination->close();
                    destProvider->removePath(tempPath);
                    throw std::runtime_error(QStringLiteral("Write failed: %1").arg(targetPath).toStdString());
                }
                copiedBytes += read;

#ifdef Q_OS_LINUX
                if (linuxCopyCachePolicy) {
                    linuxCopyCachePolicy->bytesCopied(read);
                }
#endif
                const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
                postProgressIfDue(progress);
                updateMetrics(copiedBytes, totalBytes);
            }
#ifdef Q_OS_LINUX
            if (linuxCopyCachePolicy) {
                linuxCopyCachePolicy->finish();
            }
#endif
            const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
            postProgressIfDue(progress, true);
        }

        if (auto *destinationFile = qobject_cast<QFile *>(destination.get())) {
            if (!destinationFile->flush()) {
                const QString error = destinationFile->errorString();
                destination->close();
                source->close();
                destProvider->removePath(tempPath);
                throw std::runtime_error(QStringLiteral("Flush failed: %1 (%2)")
                                             .arg(targetPath, error)
                                             .toStdString());
            }
            if (srcProvider->scheme() == QLatin1String("file")
                && destProvider->scheme() == QLatin1String("file")
                && !destinationFile->setFileTime(QFileInfo(frame.sourcePath).lastModified(),
                                                 QFileDevice::FileModificationTime)) {
                const QString error = destinationFile->errorString();
                destination->close();
                source->close();
                destProvider->removePath(tempPath);
                throw std::runtime_error(QStringLiteral("Cannot preserve modification time for %1 (%2)")
                                             .arg(targetPath, error)
                                             .toStdString());
            }
        }
        destination->close();
        source->close();

        if (m_abort) {
            destProvider->removePath(tempPath);
            return;
        }

        if (pathExists(targetPath)) {
            if (!removePathIfExists(targetPath)) {
                destProvider->removePath(tempPath);
                throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(targetPath).toStdString());
            }
        }
        if (!destProvider->movePath(tempPath, targetPath)) {
            destProvider->removePath(tempPath);
            throw std::runtime_error(QStringLiteral("Cannot finalize %1").arg(targetPath).toStdString());
        }
        partCleanup.finalized = true;
    }
}


void OperationQueue::movePath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes)
{
    if (m_abort) return;

    FileProvider* srcProvider = getProviderForPath(sourcePath);
    FileProvider* destProvider = getProviderForPath(destinationPath);
    const QString fileName = destinationNameForCopy(srcProvider, sourcePath);
    const QString label = srcProvider->scheme() == QLatin1String("file")
        && destProvider->scheme() == QLatin1String("file")
        ? OperationQueuePrivate::operationItemLabel(Type::Move, fileName)
        : fileName;
    QMetaObject::invokeMethod(this, [this, label]() {
        setCurrentLabel(label);
    }, Qt::QueuedConnection);

    if (srcProvider == destProvider && samePath(*srcProvider, sourcePath, destinationPath)) {
        copiedBytes += std::max<qint64>(1, totalBytesForPath(destinationPath));
        QMetaObject::invokeMethod(this, [this]() {
            setStatusMessage("Some files skipped (source is same as destination)");
        }, Qt::QueuedConnection);
        return;
    }

    QString targetPath = destinationPath;
    if (pathExists(targetPath)) {
        ConflictResolution res = waitForResolution(sourcePath, targetPath);
        if (res == ConflictResolution::Skip) {
            copiedBytes += std::max<qint64>(1, totalBytesForPath(sourcePath));
            return;
        } else if (res == ConflictResolution::KeepBoth) {
            targetPath = uniqueDestinationPath(targetPath);
        } else if (res == ConflictResolution::Replace) {
            // Safety check: is targetPath the source archive itself (or its parent)?
            if (ArchiveSupport::isArchivePath(sourcePath)) {
                QString physicalPath = ArchiveSupport::physicalArchivePath(sourcePath);
                FileProvider* localProvider = getProviderForPath(physicalPath);
                if (samePath(*localProvider, targetPath, physicalPath) || isDescendantPath(*localProvider, physicalPath, targetPath)) {
                    // Override Replace with KeepBoth to prevent destroying the source archive
                    res = ConflictResolution::KeepBoth;
                    targetPath = uniqueDestinationPath(targetPath);
                    QMetaObject::invokeMethod(this, [this]() {
                        setStatusMessage("Cannot replace the source archive. The item has been renamed.");
                    }, Qt::QueuedConnection);
                }
            }

            if (res == ConflictResolution::Replace) {
                if (!removePathIfExists(targetPath)) {
                    throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(targetPath).toStdString());
                }
            }
        } else if (res == ConflictResolution::Cancel) {
            m_abort = true;
            return;
        }
    }

    if (m_abort) return;

    if (srcProvider == destProvider && srcProvider->movePath(sourcePath, targetPath)) {
        copiedBytes += std::max<qint64>(1, totalBytesForPath(targetPath));
        const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
        QMetaObject::invokeMethod(this, [this, progress]() {
            setProgress(progress);
        }, Qt::QueuedConnection);
        updateMetrics(copiedBytes, totalBytes);
        return;
    }

    copyPath(sourcePath, targetPath, totalBytes, copiedBytes, Type::Move);

    if (m_abort) return;

    if (!removeSourcePath(sourcePath)) {
        const QString message = providerFailureReason(
            srcProvider,
            QStringLiteral("Cannot remove source: it may be in use or protected"));
        throw std::runtime_error(message.toStdString());
    }
}

bool OperationQueue::pathExists(const QString &path) const
{
    return getProviderForPath(path)->pathExists(path);
}

bool OperationQueue::isRealDirectory(const QString &path) const
{
    FileProvider* provider = getProviderForPath(path);
    return provider->isDirectory(path) && !provider->isSymLink(path);
}

bool OperationQueue::removePathIfExists(const QString &path) const
{
    if (ArchiveSupport::archiveBackendAvailable() && ArchiveSupport::isArchiveFilePath(path)) {
        ArchiveFileProvider::invalidateCacheForPath(path);
    }
    if (!pathExists(path)) {
        getProviderForPath(path)->clearLastError();
        return true;
    }
    return getProviderForPath(path)->removePath(path);
}

bool OperationQueue::removeSourcePath(const QString &path) const
{
    // Skip removal for virtual paths inside archives
    if (ArchiveSupport::isArchivePath(path)) {
        return true;
    }
    return getProviderForPath(path)->removePath(path);
}

bool OperationQueue::ensureParentDirectory(const QString &path) const
{
    return getProviderForPath(path)->ensureParentDirectory(path);
}

bool OperationQueue::makePath(const QString &path) const
{
    return getProviderForPath(path)->makePath(path);
}

QStringList OperationQueue::childPaths(const QString &path) const
{
    return getProviderForPath(path)->childPaths(path);
}

QString OperationQueue::destinationNameForCopy(FileProvider *sourceProvider, const QString &sourcePath) const
{
    if (!sourceProvider) {
        return {};
    }

    const QString localName = sourceProvider->localCopyFileName(sourcePath).trimmed();
    if (!localName.isEmpty()) {
        return localName;
    }

    const std::optional<FileEntry> sourceInfo = sourceProvider->entryInfo(sourcePath);
    return sourceInfo ? sourceInfo->name : sourceProvider->fileName(sourcePath);
}

QString OperationQueue::uniqueDestinationPath(const QString &path) const
{
    if (!pathExists(path)) {
        return path;
    }

    FileProvider* provider = getProviderForPath(path);
    const QString parentDir = provider->parentPath(path);
    const QString baseName = provider->fileName(path);
    const int dot = baseName.lastIndexOf(QChar('.'));
    const QString base = (dot > 0) ? baseName.left(dot) : baseName;
    const QString suffix = (dot > 0) ? baseName.mid(dot) : QString();

    for (int i = 1; i < 10000; ++i) {
        const QString name = suffix.isEmpty()
            ? QStringLiteral("%1 copy %2").arg(base).arg(i)
            : QStringLiteral("%1 copy %2%3").arg(base).arg(i).arg(suffix);
        const QString candidate = provider->childPath(parentDir, name);
        if (!pathExists(candidate)) {
            return candidate;
        }
    }

    return path;
}

QString OperationQueue::duplicateDestinationPath(const QString &path) const
{
    FileProvider *provider = getProviderForPath(path);
    const QString parentDir = provider->parentPath(path);
    const QString baseName = provider->fileName(path);
    const int dot = baseName.lastIndexOf(QChar('.'));
    const QString base = (dot > 0) ? baseName.left(dot) : baseName;
    const QString suffix = (dot > 0) ? baseName.mid(dot) : QString();
    const QString effectiveBase = base.isEmpty() ? baseName : base;

    const QString firstName = suffix.isEmpty()
        ? QStringLiteral("%1(copy)").arg(effectiveBase)
        : QStringLiteral("%1(copy)%2").arg(effectiveBase, suffix);
    QString candidate = provider->childPath(parentDir, firstName);
    if (!pathExists(candidate)) {
        return candidate;
    }

    for (int i = 2; i < 10000; ++i) {
        const QString name = suffix.isEmpty()
            ? QStringLiteral("%1(copy %2)").arg(effectiveBase).arg(i)
            : QStringLiteral("%1(copy %2)%3").arg(effectiveBase).arg(i).arg(suffix);
        candidate = provider->childPath(parentDir, name);
        if (!pathExists(candidate)) {
            return candidate;
        }
    }

    return uniqueDestinationPath(provider->childPath(parentDir, baseName));
}
