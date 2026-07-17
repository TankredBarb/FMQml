#include "OperationQueue.h"
#include "OperationQueuePrivate.h"

#include "CleanupSubsystem.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QScopeGuard>
#include <QUuid>
#include <QVector>

#include <algorithm>
#include <utility>

using OperationQueuePrivate::CopyFrame;
using OperationQueuePrivate::keepExistingLocalUploadItems;
using OperationQueuePrivate::mibPerSecond;
using OperationQueuePrivate::operationFolderLabel;
using OperationQueuePrivate::pathLogName;
using OperationQueuePrivate::providerBatchLabel;
using OperationQueuePrivate::providerBatchLoggingEnabled;
using OperationQueuePrivate::providerFailureReason;
using OperationQueuePrivate::providerMaterializeLoggingEnabled;
using OperationQueuePrivate::providerStagedWaveCount;
using OperationQueuePrivate::providerTransferTimingEnabled;
using OperationQueuePrivate::ProviderLocalBatchFileLimit;
using OperationQueuePrivate::ProviderStagedBatchMaxBytes;
using OperationQueuePrivate::ProviderStagedBatchMaxFiles;

bool OperationQueue::copySmallLocalFilesToProviderBatch(const QStringList &sources,
                                                        const QString &destination,
                                                        qint64 totalBytes,
                                                        qint64 &copiedBytes)
{
    if (sources.size() < 2 || destination.isEmpty()) {
        return false;
    }

    FileProvider *destProvider = getProviderForPath(destination);
    if (!destProvider || destProvider->scheme() == QLatin1String("file")
        || !destProvider->supportsLocalFileBatchCopy()) {
        return false;
    }

    QVector<LocalFileCopyItem> items;
    items.reserve(sources.size());
    qint64 batchBytes = 0;

    for (const QString &source : sources) {
        FileProvider *srcProvider = getProviderForPath(source);
        if (!srcProvider || srcProvider->scheme() != QLatin1String("file") || isRealDirectory(source)) {
            return false;
        }
        const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(source);
        if (!sourceInfo || sourceInfo->size > ProviderLocalBatchFileLimit) {
            return false;
        }
        const QString targetPath = destProvider->childPath(destination, destinationNameForCopy(srcProvider, source));
        if (pathExists(targetPath)) {
            return false;
        }
        batchBytes += sourceInfo->size;
        items.push_back(LocalFileCopyItem{source, targetPath, sourceInfo->size});
    }

    const qint64 baseBytes = copiedBytes;
    QString batchError;
    const QString uploadLabel = providerBatchLabel(QLatin1StringView("Uploading"), 0);
    QMetaObject::invokeMethod(this, [this, uploadLabel]() {
        setCurrentLabel(uploadLabel);
    }, Qt::QueuedConnection);
    const bool copied = destProvider->copyFromLocalFiles(
        items,
        [this, baseBytes, totalBytes](const QString &currentFilePath, qint64 processed, qint64 total) -> bool {
            Q_UNUSED(total)
            Q_UNUSED(currentFilePath)
            if (m_abort) {
                return false;
            }
            const qint64 progressBytes = std::clamp<qint64>(baseBytes + processed, 0, totalBytes);
            const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
            QMetaObject::invokeMethod(this, [this, progress]() {
                setProgress(progress);
            }, Qt::QueuedConnection);
            updateMetrics(progressBytes, totalBytes);
            return true;
        },
        &batchError);

    if (!copied) {
        if (m_abort) {
            return true;
        }
        if (!batchError.trimmed().isEmpty()) {
            throw std::runtime_error(batchError.toStdString());
        }
        return false;
    }

    copiedBytes = (std::min)(totalBytes, copiedBytes + batchBytes);
    const double progress = static_cast<double>(copiedBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
    QMetaObject::invokeMethod(this, [this, progress]() {
        setProgress(progress);
    }, Qt::QueuedConnection);
    updateMetrics(copiedBytes, totalBytes);
    for (const LocalFileCopyItem &item : std::as_const(items)) {
        m_committedBatchFinalPaths.insert(item.sourceFilePath, item.destinationPath);
    }
    return true;
}

int OperationQueue::copyNextSmallLocalFilesToProviderBatch(const QStringList &sources,
                                                           int startIndex,
                                                           const QString &destination,
                                                           qint64 totalBytes,
                                                           qint64 &copiedBytes)
{
    if (startIndex < 0 || startIndex >= sources.size() || destination.isEmpty()) {
        return 0;
    }

    FileProvider *destProvider = getProviderForPath(destination);
    if (!destProvider || destProvider->scheme() == QLatin1String("file")
        || !destProvider->supportsLocalFileBatchCopy()) {
        return 0;
    }

    QVector<LocalFileCopyItem> items;
    qint64 batchBytes = 0;

    for (int i = startIndex; i < sources.size(); ++i) {
        const QString &source = sources.at(i);
        FileProvider *srcProvider = getProviderForPath(source);
        if (!srcProvider || srcProvider->scheme() != QLatin1String("file") || isRealDirectory(source)) {
            break;
        }

        const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(source);
        if (!sourceInfo || sourceInfo->size > ProviderLocalBatchFileLimit) {
            break;
        }

        const QString targetPath = destProvider->childPath(destination, destinationNameForCopy(srcProvider, source));
        if (targetPath.isEmpty() || pathExists(targetPath)) {
            break;
        }

        batchBytes += sourceInfo->size;
        items.push_back(LocalFileCopyItem{source, targetPath, sourceInfo->size});
    }

    if (items.size() < 2) {
        return 0;
    }

    if (providerBatchLoggingEnabled()) {
        qInfo() << "Provider mixed file upload scheduler"
                << "startIndex" << startIndex
                << "files" << items.size()
                << "bytes" << batchBytes;
    }

    const qint64 baseBytes = copiedBytes;
    QString batchError;
    const QString uploadLabel = providerBatchLabel(QLatin1StringView("Uploading"), 0);
    QMetaObject::invokeMethod(this, [this, uploadLabel]() {
        setCurrentLabel(uploadLabel);
    }, Qt::QueuedConnection);
    const bool copied = destProvider->copyFromLocalFiles(
        items,
        [this, baseBytes, totalBytes](const QString &currentFilePath, qint64 processed, qint64 total) -> bool {
            Q_UNUSED(total)
            Q_UNUSED(currentFilePath)
            if (m_abort) {
                return false;
            }
            const qint64 progressBytes = std::clamp<qint64>(baseBytes + processed, 0, totalBytes);
            const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
            QMetaObject::invokeMethod(this, [this, progress]() {
                setProgress(progress);
            }, Qt::QueuedConnection);
            updateMetrics(progressBytes, totalBytes);
            return true;
        },
        &batchError);

    if (!copied) {
        if (m_abort) {
            return items.size();
        }
        if (!batchError.trimmed().isEmpty()) {
            throw std::runtime_error(batchError.toStdString());
        }
        return 0;
    }

    copiedBytes = (std::min)(totalBytes, copiedBytes + batchBytes);
    const double progress = static_cast<double>(copiedBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
    QMetaObject::invokeMethod(this, [this, progress]() {
        setProgress(progress);
    }, Qt::QueuedConnection);
    updateMetrics(copiedBytes, totalBytes);
    for (const LocalFileCopyItem &item : std::as_const(items)) {
        m_committedBatchFinalPaths.insert(item.sourceFilePath, item.destinationPath);
    }
    return items.size();
}

bool OperationQueue::copyLocalDirectoryToProviderBatch(const QString &sourcePath,
                                                       const QString &destinationPath,
                                                       qint64 totalBytes,
                                                       qint64 &copiedBytes)
{
    FileProvider *srcProvider = getProviderForPath(sourcePath);
    FileProvider *destProvider = getProviderForPath(destinationPath);
    if (!srcProvider || !destProvider
        || srcProvider->scheme() != QLatin1String("file")
        || destProvider->scheme() == QLatin1String("file")
        || !destProvider->supportsLocalFileBatchCopy()
        || !isRealDirectory(sourcePath)) {
        return false;
    }

    const QString initialScanLabel = operationFolderLabel(QLatin1StringView("Scanning"), sourcePath);
    QMetaObject::invokeMethod(this, [this, initialScanLabel]() {
        setCurrentLabel(initialScanLabel);
    }, Qt::QueuedConnection);

    struct DirectoryFrame {
        QString source;
        QString destination;
    };

    QVector<CopyFrame> largeFiles;
    QVector<LocalFileCopyItem> items;
    qint64 smallFileCount = 0;
    QVector<QString> checkStack;
    checkStack.push_back(sourcePath);
    while (!checkStack.isEmpty()) {
        if (m_abort) {
            return true;
        }

        const QString current = checkStack.back();
        checkStack.pop_back();

        const QStringList children = srcProvider->childPaths(current);
        for (const QString &child : children) {
            const std::optional<FileEntry> childInfo = srcProvider->entryInfo(child);
            if (srcProvider->isDirectory(child)) {
                checkStack.push_back(child);
                continue;
            }
            if (!childInfo) {
                return false;
            }
            if (childInfo->size <= ProviderLocalBatchFileLimit) {
                ++smallFileCount;
            }
        }
    }

    if (smallFileCount < 2) {
        return false;
    }

    QVector<DirectoryFrame> stack;
    stack.push_back({sourcePath, destinationPath});
    int uploadBatchIndex = 0;

    while (!stack.isEmpty()) {
        if (m_abort) {
            return true;
        }

        const DirectoryFrame frame = stack.back();
        stack.pop_back();
        if (pathExists(frame.destination)) {
            return false;
        }

        const QString createLabel = operationFolderLabel(QLatin1StringView("Creating"), frame.source);
        QMetaObject::invokeMethod(this, [this, createLabel]() {
            setCurrentLabel(createLabel);
        }, Qt::QueuedConnection);
        if (!destProvider->makePath(frame.destination)) {
            throw std::runtime_error(providerFailureReason(
                destProvider,
                QStringLiteral("Cannot create folder %1").arg(frame.destination)).toStdString());
        }

        QVector<LocalFileCopyItem> directoryItems;
        QVector<CopyFrame> directoryLargeFiles;
        qint64 directoryBatchBytes = 0;

        const QStringList children = srcProvider->childPaths(frame.source);
        for (const QString &child : children) {
            const QString childDestination = destProvider->childPath(frame.destination, destinationNameForCopy(srcProvider, child));
            const std::optional<FileEntry> childInfo = srcProvider->entryInfo(child);
            if (childDestination.isEmpty() || pathExists(childDestination)) {
                return false;
            }
            if (srcProvider->isDirectory(child)) {
                stack.push_back({child, childDestination});
                continue;
            }
            if (!childInfo) {
                return false;
            }
            if (childInfo->size > ProviderLocalBatchFileLimit) {
                directoryLargeFiles.push_back({child, childDestination});
            } else {
                directoryBatchBytes += childInfo->size;
                directoryItems.push_back(LocalFileCopyItem{child, childDestination, childInfo->size});
            }
        }

        if (providerBatchLoggingEnabled() && !directoryItems.isEmpty()) {
            qInfo() << "Provider directory upload scheduler wave"
                    << "source" << frame.source
                    << "destination" << frame.destination
                    << "files" << directoryItems.size()
                    << "bytes" << directoryBatchBytes
                    << "largeFiles" << directoryLargeFiles.size();
        }

        if (directoryItems.size() == 1) {
            copyPath(directoryItems.constFirst().sourceFilePath,
                     directoryItems.constFirst().destinationPath,
                     totalBytes,
                     copiedBytes);
        } else if (directoryItems.size() > 1) {
            ++uploadBatchIndex;
            const qint64 baseBytes = copiedBytes;
            QString batchError;
            const QString uploadLabel = providerBatchLabel(QLatin1StringView("Uploading"), uploadBatchIndex);
            QMetaObject::invokeMethod(this, [this, uploadLabel]() {
                setCurrentLabel(uploadLabel);
            }, Qt::QueuedConnection);
            const bool copied = destProvider->copyFromLocalFiles(
                directoryItems,
                [this, baseBytes, totalBytes](const QString &currentFilePath, qint64 processed, qint64 total) -> bool {
                    Q_UNUSED(total)
                    Q_UNUSED(currentFilePath)
                    if (m_abort) {
                        return false;
                    }
                    const qint64 progressBytes = std::clamp<qint64>(baseBytes + processed, 0, totalBytes);
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(progressBytes, totalBytes);
                    return true;
                },
                &batchError);

            if (!copied) {
                if (m_abort) {
                    return true;
                }
                if (!batchError.trimmed().isEmpty()) {
                    throw std::runtime_error(batchError.toStdString());
                }
                return false;
            }

            copiedBytes = (std::min)(totalBytes, copiedBytes + directoryBatchBytes);
            const double progress = static_cast<double>(copiedBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
            QMetaObject::invokeMethod(this, [this, progress]() {
                setProgress(progress);
            }, Qt::QueuedConnection);
            updateMetrics(copiedBytes, totalBytes);
        }

        for (const CopyFrame &largeFile : std::as_const(directoryLargeFiles)) {
            if (m_abort) {
                return true;
            }
            copyPath(largeFile.sourcePath, largeFile.destinationPath, totalBytes, copiedBytes);
        }
    }

    m_committedBatchFinalPaths.insert(sourcePath, destinationPath);
    return true;
}

bool OperationQueue::copyProviderDirectoryToProviderStagedBatch(const QString &sourcePath,
                                                                const QString &destinationPath,
                                                                qint64 totalBytes,
                                                                qint64 &copiedBytes)
{
    FileProvider *srcProvider = getProviderForPath(sourcePath);
    FileProvider *destProvider = getProviderForPath(destinationPath);
    if (!srcProvider || !destProvider
        || srcProvider->scheme() == QLatin1String("file")
        || destProvider->scheme() == QLatin1String("file")
        || !destProvider->supportsLocalFileBatchCopy()
        || !isRealDirectory(sourcePath)) {
        return false;
    }

    struct DirectoryFrame {
        QString source;
        QString destination;
    };

    QVector<CopyFrame> batchFiles;
    qint64 batchBytes = 0;
    const bool skipFreshDestinationChildConflictChecks = destProvider->scheme() == QLatin1String("gdrive")
        || destProvider->scheme() == QLatin1String("mega");

    QVector<DirectoryFrame> stack;
    stack.push_back({sourcePath, destinationPath});
    QElapsedTimer preflightTimer;
    if (providerTransferTimingEnabled()) {
        preflightTimer.start();
        qInfo().noquote()
            << "[ProviderTransferPhase]"
            << "phase=stagedBatchPreflightStart"
            << "sourceScheme=" << srcProvider->scheme()
            << "destinationScheme=" << destProvider->scheme()
            << "source=" << pathLogName(sourcePath)
            << "destination=" << pathLogName(destinationPath);
    }
    while (!stack.isEmpty()) {
        if (m_abort) {
            return true;
        }

        const DirectoryFrame frame = stack.back();
        stack.pop_back();
        const QString prepareLabel = operationFolderLabel(QLatin1StringView("Preparing"), frame.source);
        QMetaObject::invokeMethod(this, [this, prepareLabel]() {
            setCurrentLabel(prepareLabel);
        }, Qt::QueuedConnection);
        if (pathExists(frame.destination)) {
            return false;
        }

        QElapsedTimer makePathTimer;
        if (providerTransferTimingEnabled()) {
            makePathTimer.start();
            qInfo().noquote()
                << "[ProviderTransferPhase]"
                << "phase=stagedBatchMakePathStart"
                << "destinationScheme=" << destProvider->scheme()
                << "destination=" << pathLogName(frame.destination);
        }
        if (!destProvider->makePath(frame.destination)) {
            throw std::runtime_error(providerFailureReason(
                destProvider,
                QStringLiteral("Cannot create folder %1").arg(frame.destination)).toStdString());
        }
        if (providerTransferTimingEnabled()) {
            qInfo().noquote()
                << "[ProviderTransferPhase]"
                << "phase=stagedBatchMakePathFinish"
                << "destinationScheme=" << destProvider->scheme()
                << "elapsedMs=" << makePathTimer.elapsed();
        }

        const QString scanLabel = operationFolderLabel(QLatin1StringView("Scanning"), frame.source);
        QMetaObject::invokeMethod(this, [this, scanLabel]() {
            setCurrentLabel(scanLabel);
        }, Qt::QueuedConnection);
        const QStringList children = srcProvider->childPaths(frame.source);
        for (const QString &child : children) {
            if (m_abort) {
                return true;
            }

            const QString childDestination = destProvider->childPath(frame.destination, destinationNameForCopy(srcProvider, child));
            if (childDestination.isEmpty()) {
                return false;
            }
            if (!skipFreshDestinationChildConflictChecks && pathExists(childDestination)) {
                return false;
            }

            if (srcProvider->isDirectory(child)) {
                stack.push_back({child, childDestination});
                continue;
            }

            const std::optional<FileEntry> childInfo = srcProvider->entryInfo(child);
            if (!childInfo) {
                return false;
            }

            batchBytes += childInfo->size;
            batchFiles.push_back({child, childDestination, childInfo->size});
        }
    }
    if (providerTransferTimingEnabled()) {
        qInfo().noquote()
            << "[ProviderTransferPhase]"
            << "phase=stagedBatchPreflightFinish"
            << "destinationScheme=" << destProvider->scheme()
            << "batchFiles=" << batchFiles.size()
            << "bytes=" << batchBytes
            << "elapsedMs=" << preflightTimer.elapsed();
    }

    if (batchFiles.isEmpty()) {
        return true;
    }

    const int waveCount = providerStagedWaveCount(batchFiles);
    if (providerBatchLoggingEnabled()) {
        qInfo() << "Provider staged directory batch upload"
                << "source" << sourcePath
                << "destination" << destinationPath
                << "files" << batchFiles.size()
                << "bytes" << batchBytes
                << "waves" << waveCount
                << "maxFilesPerWave" << ProviderStagedBatchMaxFiles
                << "maxBytesPerWave" << ProviderStagedBatchMaxBytes;
    }

    qsizetype index = 0;
    int waveIndex = 0;
    while (index < batchFiles.size()) {
        if (m_abort) {
            return true;
        }

        QVector<CopyFrame> waveFiles;
        qint64 waveBytes = 0;
        while (index < batchFiles.size() && waveFiles.size() < ProviderStagedBatchMaxFiles) {
            const CopyFrame &file = batchFiles.at(index);
            const qint64 fileSize = (std::max<qint64>)(0, file.size);
            if (!waveFiles.isEmpty() && waveBytes + fileSize > ProviderStagedBatchMaxBytes) {
                break;
            }
            waveBytes += fileSize;
            waveFiles.push_back(file);
            ++index;
        }

        if (waveFiles.isEmpty()) {
            break;
        }
        ++waveIndex;

        const bool timingActive = m_providerTransferTiming.active;
        QElapsedTimer waveTimer;
        QElapsedTimer allocationTimer;
        if (timingActive) {
            waveTimer.start();
            allocationTimer.start();
        }
        qint64 allocationMs = 0;
        qint64 stagingMs = 0;
        qint64 uploadMs = 0;
        qint64 cleanupMs = 0;

        QString leaseId;
        const QString stagingParent = StagingLocationPolicy::resolveStagingParent(destinationPath, {}, {}, true);
        const QString stagingDir = CleanupSubsystem::instance().allocateStagingDirectory(
            CleanupArtifactKind::ProviderTransfer,
            stagingParent,
            QStringLiteral("provider-transfer-batch-") + QUuid::createUuid().toString(QUuid::WithoutBraces),
            &leaseId);
        if (timingActive) {
            allocationMs = allocationTimer.elapsed();
        }
        if (stagingDir.isEmpty()) {
            throw std::runtime_error("Cannot allocate provider transfer staging location");
        }

        const auto cleanup = qScopeGuard([&]() {
            if (!leaseId.isEmpty()) {
                CleanupSubsystem::instance().scheduleDeleteOnFailure(leaseId);
            }
        });

        if (providerBatchLoggingEnabled()) {
            qInfo() << "Provider staged batch wave"
                    << "files" << waveFiles.size()
                    << "bytes" << waveBytes
                    << "stagingDir" << stagingDir;
        }

        const bool materializeLoggingActive = providerMaterializeLoggingEnabled();
        QVector<LocalFileCopyItem> uploadItems;
        uploadItems.reserve(waveFiles.size());
        qint64 stagedWaveBytes = 0;
        const qint64 baseBytes = copiedBytes;

        QElapsedTimer stagingTimer;
        if (timingActive) {
            stagingTimer.start();
        }
        if (srcProvider->supportsLocalFileBatchMaterialize()) {
            const QString materializeLabel = providerBatchLabel(QLatin1StringView("Downloading"), waveIndex, waveCount);
            QMetaObject::invokeMethod(this, [this, materializeLabel]() {
                setCurrentLabel(materializeLabel);
            }, Qt::QueuedConnection);

            QVector<LocalFileMaterializeItem> materializeItems;
            materializeItems.reserve(waveFiles.size());
            for (qsizetype i = 0; i < waveFiles.size(); ++i) {
                if (m_abort) {
                    return true;
                }

                const CopyFrame &file = waveFiles.at(i);
                const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(file.sourcePath);
                if (!sourceInfo) {
                    return false;
                }

                const QString sourceName = destinationNameForCopy(srcProvider, file.sourcePath);
                QString suffix = QFileInfo(sourceName).suffix().toLower();
                if (suffix.size() > 16 || suffix.contains(QLatin1Char('/')) || suffix.contains(QLatin1Char('\\'))) {
                    suffix.clear();
                }
                const QString stagedPath = QDir(stagingDir).filePath(
                    QStringLiteral("transfer-%1").arg(i, 5, 10, QLatin1Char('0'))
                    + (suffix.isEmpty() ? QString{} : QLatin1Char('.') + suffix));

                const qint64 fileSize = file.size > 0 ? file.size : sourceInfo->size;
                materializeItems.push_back(LocalFileMaterializeItem{file.sourcePath, stagedPath, fileSize});
                uploadItems.push_back(LocalFileCopyItem{stagedPath, file.destinationPath, fileSize});
            }

            qint64 stagedProcessed = 0;
            QString stagingError;
            QElapsedTimer materializeTimer;
            if (materializeLoggingActive) {
                materializeTimer.start();
            }
            const bool staged = srcProvider->copyToLocalFiles(
                materializeItems,
                [this, baseBytes, waveBytes, totalBytes, &stagedProcessed](const QString &currentSourcePath, qint64 processed, qint64 total) -> bool {
                    Q_UNUSED(total)
                    Q_UNUSED(currentSourcePath)
                    if (m_abort) {
                        return false;
                    }
                    stagedProcessed = (std::max<qint64>)(0, processed);
                    const qint64 stagedBytes = std::clamp<qint64>(stagedProcessed, 0, waveBytes);
                    const qint64 progressBytes = std::clamp<qint64>(baseBytes + stagedBytes / 2, 0, totalBytes);
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(progressBytes, totalBytes);
                    return true;
                },
                &stagingError);
            if (!staged) {
                if (m_abort) {
                    return true;
                }
                throw std::runtime_error(stagingError.trimmed().isEmpty()
                                             ? "Provider staged batch download failed"
                                             : stagingError.toStdString());
            }
            waveBytes = keepExistingLocalUploadItems(uploadItems);
            if (uploadItems.isEmpty()) {
                throw std::runtime_error(stagingError.trimmed().isEmpty()
                                             ? "Provider staged batch download failed"
                                             : stagingError.toStdString());
            }
            stagedWaveBytes = waveBytes;
            if (materializeLoggingActive) {
                const qint64 elapsedMs = materializeTimer.isValid() ? materializeTimer.elapsed() : 0;
                qInfo().noquote()
                    << "[ProviderMaterializeWave]"
                    << "operationId=" << m_providerTransferTiming.operationId
                    << "sourceScheme=" << srcProvider->scheme()
                    << "destinationScheme=" << destProvider->scheme()
                    << "files=" << waveFiles.size()
                    << "bytes=" << waveBytes
                    << "stagedBytes=" << stagedWaveBytes
                    << "elapsedMs=" << elapsedMs
                    << "throughputMiBs=" << mibPerSecond(stagedWaveBytes, elapsedMs);
            }
        } else {
            const QString materializeLabel = providerBatchLabel(QLatin1StringView("Reading"), waveIndex, waveCount);
            QMetaObject::invokeMethod(this, [this, materializeLabel]() {
                setCurrentLabel(materializeLabel);
            }, Qt::QueuedConnection);

            for (qsizetype i = 0; i < waveFiles.size(); ++i) {
                if (m_abort) {
                    return true;
                }

                const CopyFrame &file = waveFiles.at(i);
                const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(file.sourcePath);
                if (!sourceInfo) {
                    return false;
                }

                const QString sourceName = destinationNameForCopy(srcProvider, file.sourcePath);
                QString suffix = QFileInfo(sourceName).suffix().toLower();
                if (suffix.size() > 16 || suffix.contains(QLatin1Char('/')) || suffix.contains(QLatin1Char('\\'))) {
                    suffix.clear();
                }
                const QString stagedPath = QDir(stagingDir).filePath(
                    QStringLiteral("transfer-%1").arg(i, 5, 10, QLatin1Char('0'))
                    + (suffix.isEmpty() ? QString{} : QLatin1Char('.') + suffix));

                qint64 stagedProcessed = 0;
                QString stagingError;
                QElapsedTimer fileMaterializeTimer;
                if (materializeLoggingActive) {
                    fileMaterializeTimer.start();
                }
                const bool staged = srcProvider->copyToLocalFile(
                    file.sourcePath,
                    stagedPath,
                    [this, baseBytes, stagedWaveBytes, waveBytes, totalBytes, &stagedProcessed](qint64 processed, qint64 total) -> bool {
                        Q_UNUSED(total)
                        if (m_abort) {
                            return false;
                        }
                        stagedProcessed = (std::max<qint64>)(0, processed);
                        const qint64 stagedBytes = std::clamp<qint64>(stagedWaveBytes + stagedProcessed, 0, waveBytes);
                        const qint64 progressBytes = std::clamp<qint64>(baseBytes + stagedBytes / 2, 0, totalBytes);
                        const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
                        QMetaObject::invokeMethod(this, [this, progress]() {
                            setProgress(progress);
                        }, Qt::QueuedConnection);
                        return true;
                    },
                    &stagingError);

                if (!staged) {
                    if (m_abort) {
                        return true;
                    }
                    throw std::runtime_error(stagingError.trimmed().isEmpty()
                                                 ? "Provider staged batch download failed"
                                                 : stagingError.toStdString());
                }

                stagedWaveBytes += sourceInfo->size;
                if (materializeLoggingActive) {
                    const qint64 elapsedMs = fileMaterializeTimer.isValid() ? fileMaterializeTimer.elapsed() : 0;
                    const qint64 stagedBytesForLog = stagedProcessed > 0
                        ? std::clamp<qint64>(stagedProcessed, 0, sourceInfo->size)
                        : sourceInfo->size;
                    qInfo().noquote()
                        << "[ProviderMaterializeFile]"
                        << "operationId=" << m_providerTransferTiming.operationId
                        << "sourceScheme=" << srcProvider->scheme()
                        << "destinationScheme=" << destProvider->scheme()
                        << "index=" << (i + 1)
                        << "waveFiles=" << waveFiles.size()
                        << "source=" << pathLogName(file.sourcePath)
                        << "destination=" << pathLogName(file.destinationPath)
                        << "bytes=" << sourceInfo->size
                        << "stagedBytes=" << stagedBytesForLog
                        << "elapsedMs=" << elapsedMs
                        << "throughputMiBs=" << mibPerSecond(stagedBytesForLog, elapsedMs);
                }
                uploadItems.push_back(LocalFileCopyItem{stagedPath, file.destinationPath, file.size > 0 ? file.size : sourceInfo->size});
            }
        }
        if (timingActive) {
            stagingMs = stagingTimer.elapsed();
        }

        qint64 uploadedProcessed = 0;
        QString uploadError;
        QElapsedTimer uploadTimer;
        if (timingActive) {
            uploadTimer.start();
        }
        const QString uploadLabel = providerBatchLabel(QLatin1StringView("Uploading"), waveIndex, waveCount);
        QMetaObject::invokeMethod(this, [this, uploadLabel]() {
            setCurrentLabel(uploadLabel);
        }, Qt::QueuedConnection);
        const bool uploaded = destProvider->copyFromLocalFiles(
            uploadItems,
            [this, baseBytes, waveBytes, totalBytes, &uploadedProcessed](const QString &currentFilePath, qint64 processed, qint64 total) -> bool {
                Q_UNUSED(total)
                Q_UNUSED(currentFilePath)
                if (m_abort) {
                    return false;
                }
                uploadedProcessed = (std::max<qint64>)(0, processed);
                const qint64 uploadedBytes = std::clamp<qint64>(uploadedProcessed, 0, waveBytes);
                const qint64 progressBytes = std::clamp<qint64>(baseBytes + waveBytes / 2 + (uploadedBytes + 1) / 2, 0, totalBytes);
                const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
                updateMetrics(progressBytes, totalBytes);
                return true;
            },
            &uploadError);
        if (timingActive) {
            uploadMs = uploadTimer.elapsed();
        }

        if (!uploaded) {
            if (m_abort) {
                return true;
            }
            throw std::runtime_error(uploadError.trimmed().isEmpty()
                                         ? "Provider staged batch upload failed"
                                         : uploadError.toStdString());
        }

        copiedBytes = (std::min)(totalBytes, copiedBytes + waveBytes);
        const double progress = static_cast<double>(copiedBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
        QMetaObject::invokeMethod(this, [this, progress]() {
            setProgress(progress);
        }, Qt::QueuedConnection);
        updateMetrics(copiedBytes, totalBytes);
        QElapsedTimer cleanupTimer;
        if (timingActive) {
            cleanupTimer.start();
        }
        CleanupSubsystem::instance().scheduleDelete(leaseId);
        if (timingActive) {
            cleanupMs = cleanupTimer.elapsed();
        }
        leaseId.clear();

        if (timingActive) {
            m_providerTransferTiming.fileCount += waveFiles.size();
            m_providerTransferTiming.successfulFiles += waveFiles.size();
            m_providerTransferTiming.totalBytes += waveBytes;
            m_providerTransferTiming.stagedBytes += stagedWaveBytes;
            m_providerTransferTiming.uploadedBytes += waveBytes;
            m_providerTransferTiming.allocationMs += allocationMs;
            m_providerTransferTiming.stagingMs += stagingMs;
            m_providerTransferTiming.uploadMs += uploadMs;
            m_providerTransferTiming.cleanupMs += cleanupMs;

            const qint64 waveMs = waveTimer.isValid() ? waveTimer.elapsed() : 0;
            qInfo().noquote()
                << "[ProviderStagedBatchWave]"
                << "operationId=" << m_providerTransferTiming.operationId
                << "sourceScheme=" << srcProvider->scheme()
                << "destinationScheme=" << destProvider->scheme()
                << "files=" << waveFiles.size()
                << "bytes=" << waveBytes
                << "stagedBytes=" << stagedWaveBytes
                << "uploadedBytes=" << waveBytes
                << "allocationMs=" << allocationMs
                << "stagingMs=" << stagingMs
                << "uploadMs=" << uploadMs
                << "cleanupMs=" << cleanupMs
                << "totalMs=" << waveMs
                << "stagingMiBs=" << mibPerSecond(stagedWaveBytes, stagingMs)
                << "uploadMiBs=" << mibPerSecond(waveBytes, uploadMs);
        }
    }

    m_committedBatchFinalPaths.insert(sourcePath, destinationPath);
    return true;
}

bool OperationQueue::copyProviderDirectoryToLocalBatch(const QString &sourcePath,
                                                       const QString &destinationPath,
                                                       qint64 totalBytes,
                                                       qint64 &copiedBytes)
{
    FileProvider *srcProvider = getProviderForPath(sourcePath);
    FileProvider *destProvider = getProviderForPath(destinationPath);
    if (!srcProvider || !destProvider
        || srcProvider->scheme() == QLatin1String("file")
        || destProvider->scheme() != QLatin1String("file")
        || !srcProvider->supportsLocalFileBatchMaterialize()
        || !isRealDirectory(sourcePath)
        || pathExists(destinationPath)) {
        return false;
    }

    struct DirectoryFrame {
        QString source;
        QString destination;
    };

    QVector<CopyFrame> batchFiles;
    QVector<QString> directories;
    QSet<QString> plannedDestinations;
    QVector<DirectoryFrame> stack;
    stack.push_back({sourcePath, destinationPath});
    plannedDestinations.insert(destinationPath);
    auto plannedUniqueDestination = [&](const QString &requestedPath) {
        if (requestedPath.isEmpty() || (!pathExists(requestedPath) && !plannedDestinations.contains(requestedPath))) {
            return requestedPath;
        }

        const QString parentDir = destProvider->parentPath(requestedPath);
        const QString baseName = destProvider->fileName(requestedPath);
        const int dot = baseName.lastIndexOf(QChar('.'));
        const QString base = (dot > 0) ? baseName.left(dot) : baseName;
        const QString suffix = (dot > 0) ? baseName.mid(dot) : QString();
        for (int i = 1; i < 10000; ++i) {
            const QString name = suffix.isEmpty()
                ? QStringLiteral("%1 copy %2").arg(base).arg(i)
                : QStringLiteral("%1 copy %2%3").arg(base).arg(i).arg(suffix);
            const QString candidate = destProvider->childPath(parentDir, name);
            if (!pathExists(candidate) && !plannedDestinations.contains(candidate)) {
                return candidate;
            }
        }
        return QString{};
    };
    while (!stack.isEmpty()) {
        if (m_abort) {
            return true;
        }

        const DirectoryFrame frame = stack.back();
        stack.pop_back();
        if (pathExists(frame.destination)) {
            return false;
        }
        directories.push_back(frame.destination);

        const QStringList children = srcProvider->childPaths(frame.source);
        for (const QString &child : children) {
            if (m_abort) {
                return true;
            }

            const QString requestedChildDestination = destProvider->childPath(frame.destination, destinationNameForCopy(srcProvider, child));
            const QString childDestination = plannedUniqueDestination(requestedChildDestination);
            if (childDestination.isEmpty()) {
                return false;
            }
            plannedDestinations.insert(childDestination);

            if (srcProvider->isDirectory(child)) {
                stack.push_back({child, childDestination});
                continue;
            }

            const std::optional<FileEntry> childInfo = srcProvider->entryInfo(child);
            if (!childInfo) {
                return false;
            }

            batchFiles.push_back({child, childDestination, childInfo->size});
        }
    }

    if (batchFiles.size() < 2) {
        return false;
    }

    for (const QString &directory : std::as_const(directories)) {
        if (!makePath(directory)) {
            throw std::runtime_error(QStringLiteral("Cannot create folder %1").arg(directory).toStdString());
        }
    }

    const int waveCount = providerStagedWaveCount(batchFiles);
    qsizetype index = 0;
    int waveIndex = 0;
    while (index < batchFiles.size()) {
        if (m_abort) {
            return true;
        }

        QVector<CopyFrame> waveFiles;
        qint64 waveBytes = 0;
        while (index < batchFiles.size() && waveFiles.size() < ProviderStagedBatchMaxFiles) {
            const CopyFrame &file = batchFiles.at(index);
            const qint64 fileSize = (std::max<qint64>)(0, file.size);
            if (!waveFiles.isEmpty() && waveBytes + fileSize > ProviderStagedBatchMaxBytes) {
                break;
            }
            waveBytes += fileSize;
            waveFiles.push_back(file);
            ++index;
        }

        if (waveFiles.isEmpty()) {
            break;
        }
        ++waveIndex;

        QVector<LocalFileMaterializeItem> materializeItems;
        materializeItems.reserve(waveFiles.size());
        for (const CopyFrame &file : std::as_const(waveFiles)) {
            const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(file.sourcePath);
            if (!sourceInfo) {
                return false;
            }
            const QString partialPath = file.destinationPath + QStringLiteral(".part");
            if (pathExists(partialPath) && !removePathIfExists(partialPath)) {
                return false;
            }
            const qint64 fileSize = file.size > 0 ? file.size : sourceInfo->size;
            materializeItems.push_back(LocalFileMaterializeItem{file.sourcePath, file.destinationPath, fileSize});
        }

        qint64 stagedProcessed = 0;
        const qint64 baseBytes = copiedBytes;
        QString materializeError;
        QElapsedTimer materializeTimer;
        const bool materializeLoggingActive = providerMaterializeLoggingEnabled();
        if (materializeLoggingActive) {
            materializeTimer.start();
        }
        const QString downloadLabel = providerBatchLabel(QLatin1StringView("Downloading"), waveIndex, waveCount);
        QMetaObject::invokeMethod(this, [this, downloadLabel]() {
            setCurrentLabel(downloadLabel);
        }, Qt::QueuedConnection);
        const bool materialized = srcProvider->copyToLocalFiles(
            materializeItems,
            [this, baseBytes, waveBytes, totalBytes, &stagedProcessed](const QString &currentSourcePath, qint64 processed, qint64 total) -> bool {
                Q_UNUSED(total)
                Q_UNUSED(currentSourcePath)
                if (m_abort) {
                    return false;
                }
                stagedProcessed = (std::max<qint64>)(0, processed);
                const qint64 stagedBytes = std::clamp<qint64>(stagedProcessed, 0, waveBytes);
                const qint64 progressBytes = std::clamp<qint64>(baseBytes + stagedBytes, 0, totalBytes);
                const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
                updateMetrics(progressBytes, totalBytes);
                return true;
            },
            &materializeError);
        if (!materialized) {
            for (const CopyFrame &file : std::as_const(waveFiles)) {
                removePathIfExists(file.destinationPath + QStringLiteral(".part"));
            }
            if (m_abort) {
                return true;
            }
            throw std::runtime_error(materializeError.trimmed().isEmpty()
                                         ? "Provider local batch download failed"
                                         : materializeError.toStdString());
        }

        if (materializeLoggingActive) {
            const qint64 elapsedMs = materializeTimer.isValid() ? materializeTimer.elapsed() : 0;
            qInfo().noquote()
                << "[ProviderMaterializeWave]"
                << "operationId=" << m_providerTransferTiming.operationId
                << "sourceScheme=" << srcProvider->scheme()
                << "destinationScheme=" << destProvider->scheme()
                << "files=" << waveFiles.size()
                << "bytes=" << waveBytes
                << "stagedBytes=" << waveBytes
                << "elapsedMs=" << elapsedMs
                << "throughputMiBs=" << mibPerSecond(waveBytes, elapsedMs);
        }

        copiedBytes = (std::min)(totalBytes, copiedBytes + waveBytes);
        const double progress = static_cast<double>(copiedBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
        QMetaObject::invokeMethod(this, [this, progress]() {
            setProgress(progress);
        }, Qt::QueuedConnection);
        updateMetrics(copiedBytes, totalBytes);
    }

    m_committedBatchFinalPaths.insert(sourcePath, destinationPath);
    return true;
}

bool OperationQueue::copyProviderFilesToProviderStagedBatch(const QStringList &sources,
                                                            const QString &destination,
                                                            qint64 totalBytes,
                                                            qint64 &copiedBytes)
{
    if (sources.size() < 2 || destination.isEmpty()) {
        return false;
    }

    FileProvider *destProvider = getProviderForPath(destination);
    if (!destProvider
        || destProvider->scheme() == QLatin1String("file")
        || !destProvider->supportsLocalFileBatchCopy()) {
        return false;
    }

    struct DirectoryFrame {
        QString source;
        QString destination;
    };

    QVector<CopyFrame> batchFiles;
    qint64 batchBytes = 0;
    const bool skipFreshDestinationChildConflictChecks = destProvider->scheme() == QLatin1String("gdrive")
        || destProvider->scheme() == QLatin1String("mega");

    for (const QString &source : sources) {
        FileProvider *srcProvider = getProviderForPath(source);
        if (!srcProvider
            || srcProvider->scheme() == QLatin1String("file")) {
            return false;
        }

        const QString targetPath = destProvider->childPath(destination, destinationNameForCopy(srcProvider, source));
        if (targetPath.isEmpty() || pathExists(targetPath)) {
            return false;
        }

        if (!isRealDirectory(source)) {
            const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(source);
            if (!sourceInfo) {
                return false;
            }

            batchBytes += sourceInfo->size;
            batchFiles.push_back({source, targetPath, sourceInfo->size});
            continue;
        }

        QVector<DirectoryFrame> stack;
        stack.push_back({source, targetPath});
        while (!stack.isEmpty()) {
            if (m_abort) {
                return true;
            }

            const DirectoryFrame frame = stack.back();
            stack.pop_back();
            FileProvider *frameSourceProvider = getProviderForPath(frame.source);
            if (!frameSourceProvider || frameSourceProvider->scheme() == QLatin1String("file")) {
                return false;
            }

            const QString prepareLabel = operationFolderLabel(QLatin1StringView("Preparing"), frame.source);
            QMetaObject::invokeMethod(this, [this, prepareLabel]() {
                setCurrentLabel(prepareLabel);
            }, Qt::QueuedConnection);
            if (pathExists(frame.destination)) {
                return false;
            }
            if (!destProvider->makePath(frame.destination)) {
                throw std::runtime_error(providerFailureReason(
                    destProvider,
                    QStringLiteral("Cannot create folder %1").arg(frame.destination)).toStdString());
            }

            const QString scanLabel = operationFolderLabel(QLatin1StringView("Scanning"), frame.source);
            QMetaObject::invokeMethod(this, [this, scanLabel]() {
                setCurrentLabel(scanLabel);
            }, Qt::QueuedConnection);
            const QStringList children = frameSourceProvider->childPaths(frame.source);
            for (const QString &child : children) {
                if (m_abort) {
                    return true;
                }

                const QString childDestination = destProvider->childPath(frame.destination, destinationNameForCopy(frameSourceProvider, child));
                if (childDestination.isEmpty()) {
                    return false;
                }
                if (!skipFreshDestinationChildConflictChecks && pathExists(childDestination)) {
                    return false;
                }

                if (frameSourceProvider->isDirectory(child)) {
                    stack.push_back({child, childDestination});
                    continue;
                }

                const std::optional<FileEntry> childInfo = frameSourceProvider->entryInfo(child);
                if (!childInfo) {
                    return false;
                }

                batchBytes += childInfo->size;
                batchFiles.push_back({child, childDestination, childInfo->size});
            }
        }
    }

    if (batchFiles.isEmpty()) {
        return true;
    }

    FileProvider *firstSourceProvider = getProviderForPath(batchFiles.constFirst().sourcePath);
    if (!firstSourceProvider) {
        return false;
    }

    const int waveCount = providerStagedWaveCount(batchFiles);
    if (providerBatchLoggingEnabled()) {
        qInfo() << "Provider staged selection batch upload"
                << "files" << batchFiles.size()
                << "bytes" << batchBytes
                << "destination" << destination
                << "waves" << waveCount
                << "maxFilesPerWave" << ProviderStagedBatchMaxFiles
                << "maxBytesPerWave" << ProviderStagedBatchMaxBytes;
    }

    qsizetype index = 0;
    int waveIndex = 0;
    while (index < batchFiles.size()) {
        if (m_abort) {
            return true;
        }

        QVector<CopyFrame> waveFiles;
        qint64 waveBytes = 0;
        while (index < batchFiles.size() && waveFiles.size() < ProviderStagedBatchMaxFiles) {
            FileProvider *srcProvider = getProviderForPath(batchFiles.at(index).sourcePath);
            if (!srcProvider) {
                return false;
            }
            const qint64 fileSize = (std::max<qint64>)(0, batchFiles.at(index).size);
            if (!waveFiles.isEmpty() && waveBytes + fileSize > ProviderStagedBatchMaxBytes) {
                break;
            }
            waveBytes += fileSize;
            waveFiles.push_back(batchFiles.at(index));
            ++index;
        }

        if (waveFiles.isEmpty()) {
            break;
        }
        ++waveIndex;

        const bool timingActive = m_providerTransferTiming.active;
        QElapsedTimer waveTimer;
        QElapsedTimer allocationTimer;
        if (timingActive) {
            waveTimer.start();
            allocationTimer.start();
        }
        qint64 allocationMs = 0;
        qint64 stagingMs = 0;
        qint64 uploadMs = 0;
        qint64 cleanupMs = 0;

        QString leaseId;
        const QString stagingParent = StagingLocationPolicy::resolveStagingParent(destination, {}, {}, true);
        const QString stagingDir = CleanupSubsystem::instance().allocateStagingDirectory(
            CleanupArtifactKind::ProviderTransfer,
            stagingParent,
            QStringLiteral("provider-transfer-batch-") + QUuid::createUuid().toString(QUuid::WithoutBraces),
            &leaseId);
        if (timingActive) {
            allocationMs = allocationTimer.elapsed();
        }
        if (stagingDir.isEmpty()) {
            throw std::runtime_error("Cannot allocate provider transfer staging location");
        }

        const auto cleanup = qScopeGuard([&]() {
            if (!leaseId.isEmpty()) {
                CleanupSubsystem::instance().scheduleDeleteOnFailure(leaseId);
            }
        });

        if (providerBatchLoggingEnabled()) {
            qInfo() << "Provider staged file batch wave"
                    << "files" << waveFiles.size()
                    << "bytes" << waveBytes
                    << "stagingDir" << stagingDir;
        }

        const bool materializeLoggingActive = providerMaterializeLoggingEnabled();
        QVector<LocalFileCopyItem> uploadItems;
        uploadItems.reserve(waveFiles.size());
        qint64 stagedWaveBytes = 0;
        const qint64 baseBytes = copiedBytes;

        QElapsedTimer stagingTimer;
        if (timingActive) {
            stagingTimer.start();
        }
        FileProvider *waveSourceProvider = getProviderForPath(waveFiles.constFirst().sourcePath);
        bool canBatchMaterialize = waveSourceProvider && waveSourceProvider->supportsLocalFileBatchMaterialize();
        for (const CopyFrame &file : std::as_const(waveFiles)) {
            if (getProviderForPath(file.sourcePath) != waveSourceProvider) {
                canBatchMaterialize = false;
                break;
            }
        }

        if (canBatchMaterialize) {
            const QString materializeLabel = providerBatchLabel(QLatin1StringView("Downloading"), waveIndex, waveCount);
            QMetaObject::invokeMethod(this, [this, materializeLabel]() {
                setCurrentLabel(materializeLabel);
            }, Qt::QueuedConnection);

            QVector<LocalFileMaterializeItem> materializeItems;
            materializeItems.reserve(waveFiles.size());
            for (qsizetype i = 0; i < waveFiles.size(); ++i) {
                if (m_abort) {
                    return true;
                }

                const std::optional<FileEntry> sourceInfo = waveSourceProvider->entryInfo(waveFiles.at(i).sourcePath);
                if (!sourceInfo) {
                    return false;
                }

                const QString sourceName = destinationNameForCopy(waveSourceProvider, waveFiles.at(i).sourcePath);
                QString suffix = QFileInfo(sourceName).suffix().toLower();
                if (suffix.size() > 16 || suffix.contains(QLatin1Char('/')) || suffix.contains(QLatin1Char('\\'))) {
                    suffix.clear();
                }
                const QString stagedPath = QDir(stagingDir).filePath(
                    QStringLiteral("transfer-%1").arg(i, 5, 10, QLatin1Char('0'))
                    + (suffix.isEmpty() ? QString{} : QLatin1Char('.') + suffix));

                const qint64 fileSize = waveFiles.at(i).size > 0 ? waveFiles.at(i).size : sourceInfo->size;
                materializeItems.push_back(LocalFileMaterializeItem{waveFiles.at(i).sourcePath, stagedPath, fileSize});
                uploadItems.push_back(LocalFileCopyItem{stagedPath, waveFiles.at(i).destinationPath, fileSize});
            }

            qint64 stagedProcessed = 0;
            QString stagingError;
            QElapsedTimer materializeTimer;
            if (materializeLoggingActive) {
                materializeTimer.start();
            }
            const bool staged = waveSourceProvider->copyToLocalFiles(
                materializeItems,
                [this, baseBytes, waveBytes, totalBytes, &stagedProcessed](const QString &currentSourcePath, qint64 processed, qint64 total) -> bool {
                    Q_UNUSED(total)
                    Q_UNUSED(currentSourcePath)
                    if (m_abort) {
                        return false;
                    }
                    stagedProcessed = (std::max<qint64>)(0, processed);
                    const qint64 stagedBytes = std::clamp<qint64>(stagedProcessed, 0, waveBytes);
                    const qint64 progressBytes = std::clamp<qint64>(baseBytes + stagedBytes / 2, 0, totalBytes);
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(progressBytes, totalBytes);
                    return true;
                },
                &stagingError);
            if (!staged) {
                if (m_abort) {
                    return true;
                }
                throw std::runtime_error(stagingError.trimmed().isEmpty()
                                             ? "Provider staged file batch download failed"
                                             : stagingError.toStdString());
            }
            waveBytes = keepExistingLocalUploadItems(uploadItems);
            if (uploadItems.isEmpty()) {
                throw std::runtime_error(stagingError.trimmed().isEmpty()
                                             ? "Provider staged file batch download failed"
                                             : stagingError.toStdString());
            }
            stagedWaveBytes = waveBytes;
            if (materializeLoggingActive) {
                const qint64 elapsedMs = materializeTimer.isValid() ? materializeTimer.elapsed() : 0;
                qInfo().noquote()
                    << "[ProviderMaterializeWave]"
                    << "operationId=" << m_providerTransferTiming.operationId
                    << "sourceScheme=" << waveSourceProvider->scheme()
                    << "destinationScheme=" << destProvider->scheme()
                    << "files=" << waveFiles.size()
                    << "bytes=" << waveBytes
                    << "stagedBytes=" << stagedWaveBytes
                    << "elapsedMs=" << elapsedMs
                    << "throughputMiBs=" << mibPerSecond(stagedWaveBytes, elapsedMs);
            }
        } else {
            const QString materializeLabel = providerBatchLabel(QLatin1StringView("Reading"), waveIndex, waveCount);
            QMetaObject::invokeMethod(this, [this, materializeLabel]() {
                setCurrentLabel(materializeLabel);
            }, Qt::QueuedConnection);

            for (qsizetype i = 0; i < waveFiles.size(); ++i) {
                if (m_abort) {
                    return true;
                }

                FileProvider *srcProvider = getProviderForPath(waveFiles.at(i).sourcePath);
                if (!srcProvider) {
                    return false;
                }
                const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(waveFiles.at(i).sourcePath);
                if (!sourceInfo) {
                    return false;
                }

                const QString sourceName = destinationNameForCopy(srcProvider, waveFiles.at(i).sourcePath);
                QString suffix = QFileInfo(sourceName).suffix().toLower();
                if (suffix.size() > 16 || suffix.contains(QLatin1Char('/')) || suffix.contains(QLatin1Char('\\'))) {
                    suffix.clear();
                }
                const QString stagedPath = QDir(stagingDir).filePath(
                    QStringLiteral("transfer-%1").arg(i, 5, 10, QLatin1Char('0'))
                    + (suffix.isEmpty() ? QString{} : QLatin1Char('.') + suffix));

                qint64 stagedProcessed = 0;
                QString stagingError;
                QElapsedTimer fileMaterializeTimer;
                if (materializeLoggingActive) {
                    fileMaterializeTimer.start();
                }
                const bool staged = srcProvider->copyToLocalFile(
                    waveFiles.at(i).sourcePath,
                    stagedPath,
                    [this, baseBytes, stagedWaveBytes, waveBytes, totalBytes, &stagedProcessed](qint64 processed, qint64 total) -> bool {
                        Q_UNUSED(total)
                        if (m_abort) {
                            return false;
                        }
                        stagedProcessed = (std::max<qint64>)(0, processed);
                        const qint64 stagedBytes = std::clamp<qint64>(stagedWaveBytes + stagedProcessed, 0, waveBytes);
                        const qint64 progressBytes = std::clamp<qint64>(baseBytes + stagedBytes / 2, 0, totalBytes);
                        const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
                        QMetaObject::invokeMethod(this, [this, progress]() {
                            setProgress(progress);
                        }, Qt::QueuedConnection);
                        updateMetrics(progressBytes, totalBytes);
                        return true;
                    },
                    &stagingError);

                if (!staged) {
                    if (m_abort) {
                        return true;
                    }
                    throw std::runtime_error(stagingError.trimmed().isEmpty()
                                                 ? "Provider staged file batch download failed"
                                                 : stagingError.toStdString());
                }

                stagedWaveBytes += sourceInfo->size;
                if (materializeLoggingActive) {
                    const qint64 elapsedMs = fileMaterializeTimer.isValid() ? fileMaterializeTimer.elapsed() : 0;
                    const qint64 stagedBytesForLog = stagedProcessed > 0
                        ? std::clamp<qint64>(stagedProcessed, 0, sourceInfo->size)
                        : sourceInfo->size;
                    qInfo().noquote()
                        << "[ProviderMaterializeFile]"
                        << "operationId=" << m_providerTransferTiming.operationId
                        << "sourceScheme=" << srcProvider->scheme()
                        << "destinationScheme=" << destProvider->scheme()
                        << "index=" << (i + 1)
                        << "waveFiles=" << waveFiles.size()
                        << "source=" << pathLogName(waveFiles.at(i).sourcePath)
                        << "destination=" << pathLogName(waveFiles.at(i).destinationPath)
                        << "bytes=" << sourceInfo->size
                        << "stagedBytes=" << stagedBytesForLog
                        << "elapsedMs=" << elapsedMs
                        << "throughputMiBs=" << mibPerSecond(stagedBytesForLog, elapsedMs);
                }
                uploadItems.push_back(LocalFileCopyItem{stagedPath, waveFiles.at(i).destinationPath, waveFiles.at(i).size > 0 ? waveFiles.at(i).size : sourceInfo->size});
            }
        }
        if (timingActive) {
            stagingMs = stagingTimer.elapsed();
        }

        qint64 uploadedProcessed = 0;
        QString uploadError;
        QElapsedTimer uploadTimer;
        if (timingActive) {
            uploadTimer.start();
        }
        const QString uploadLabel = providerBatchLabel(QLatin1StringView("Uploading"), waveIndex, waveCount);
        QMetaObject::invokeMethod(this, [this, uploadLabel]() {
            setCurrentLabel(uploadLabel);
        }, Qt::QueuedConnection);
        const bool uploaded = destProvider->copyFromLocalFiles(
            uploadItems,
            [this, baseBytes, waveBytes, totalBytes, &uploadedProcessed](const QString &currentFilePath, qint64 processed, qint64 total) -> bool {
                Q_UNUSED(total)
                Q_UNUSED(currentFilePath)
                if (m_abort) {
                    return false;
                }
                uploadedProcessed = (std::max<qint64>)(0, processed);
                const qint64 uploadedBytes = std::clamp<qint64>(uploadedProcessed, 0, waveBytes);
                const qint64 progressBytes = std::clamp<qint64>(baseBytes + waveBytes / 2 + (uploadedBytes + 1) / 2, 0, totalBytes);
                const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
                updateMetrics(progressBytes, totalBytes);
                return true;
            },
            &uploadError);
        if (timingActive) {
            uploadMs = uploadTimer.elapsed();
        }

        if (!uploaded) {
            if (m_abort) {
                return true;
            }
            throw std::runtime_error(uploadError.trimmed().isEmpty()
                                         ? "Provider staged file batch upload failed"
                                         : uploadError.toStdString());
        }

        copiedBytes = (std::min)(totalBytes, copiedBytes + waveBytes);
        const double progress = static_cast<double>(copiedBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
        QMetaObject::invokeMethod(this, [this, progress]() {
            setProgress(progress);
        }, Qt::QueuedConnection);
        updateMetrics(copiedBytes, totalBytes);

        QElapsedTimer cleanupTimer;
        if (timingActive) {
            cleanupTimer.start();
        }
        CleanupSubsystem::instance().scheduleDelete(leaseId);
        if (timingActive) {
            cleanupMs = cleanupTimer.elapsed();
        }
        leaseId.clear();

        if (timingActive) {
            m_providerTransferTiming.fileCount += waveFiles.size();
            m_providerTransferTiming.successfulFiles += waveFiles.size();
            m_providerTransferTiming.totalBytes += waveBytes;
            m_providerTransferTiming.stagedBytes += stagedWaveBytes;
            m_providerTransferTiming.uploadedBytes += waveBytes;
            m_providerTransferTiming.allocationMs += allocationMs;
            m_providerTransferTiming.stagingMs += stagingMs;
            m_providerTransferTiming.uploadMs += uploadMs;
            m_providerTransferTiming.cleanupMs += cleanupMs;

            const qint64 waveMs = waveTimer.isValid() ? waveTimer.elapsed() : 0;
            qInfo().noquote()
                << "[ProviderStagedBatchWave]"
                << "operationId=" << m_providerTransferTiming.operationId
                << "sourceScheme=" << firstSourceProvider->scheme()
                << "destinationScheme=" << destProvider->scheme()
                << "files=" << waveFiles.size()
                << "bytes=" << waveBytes
                << "stagedBytes=" << stagedWaveBytes
                << "uploadedBytes=" << waveBytes
                << "allocationMs=" << allocationMs
                << "stagingMs=" << stagingMs
                << "uploadMs=" << uploadMs
                << "cleanupMs=" << cleanupMs
                << "totalMs=" << waveMs
                << "stagingMiBs=" << mibPerSecond(stagedWaveBytes, stagingMs)
                << "uploadMiBs=" << mibPerSecond(waveBytes, uploadMs);
        }
    }

    for (const CopyFrame &file : std::as_const(batchFiles)) {
        m_committedBatchFinalPaths.insert(file.sourcePath, file.destinationPath);
    }
    return true;
}

bool OperationQueue::copyProviderFilesToLocalBatch(const QStringList &sources,
                                                   const QString &destination,
                                                   qint64 totalBytes,
                                                   qint64 &copiedBytes)
{
    if (sources.size() < 2 || destination.isEmpty()) {
        return false;
    }

    FileProvider *destProvider = getProviderForPath(destination);
    FileProvider *srcProvider = getProviderForPath(sources.constFirst());
    if (!srcProvider || !destProvider
        || srcProvider->scheme() == QLatin1String("file")
        || destProvider->scheme() != QLatin1String("file")
        || !srcProvider->supportsLocalFileBatchMaterialize()) {
        return false;
    }

    QVector<CopyFrame> batchFiles;
    batchFiles.reserve(sources.size());
    QSet<QString> plannedDestinations;
    auto plannedUniqueDestination = [&](const QString &requestedPath) {
        if (requestedPath.isEmpty() || (!pathExists(requestedPath) && !plannedDestinations.contains(requestedPath))) {
            return requestedPath;
        }

        const QString parentDir = destProvider->parentPath(requestedPath);
        const QString baseName = destProvider->fileName(requestedPath);
        const int dot = baseName.lastIndexOf(QChar('.'));
        const QString base = (dot > 0) ? baseName.left(dot) : baseName;
        const QString suffix = (dot > 0) ? baseName.mid(dot) : QString();
        for (int i = 1; i < 10000; ++i) {
            const QString name = suffix.isEmpty()
                ? QStringLiteral("%1 copy %2").arg(base).arg(i)
                : QStringLiteral("%1 copy %2%3").arg(base).arg(i).arg(suffix);
            const QString candidate = destProvider->childPath(parentDir, name);
            if (!pathExists(candidate) && !plannedDestinations.contains(candidate)) {
                return candidate;
            }
        }
        return QString{};
    };
    for (const QString &source : sources) {
        if (getProviderForPath(source) != srcProvider || isRealDirectory(source)) {
            return false;
        }

        const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(source);
        if (!sourceInfo) {
            return false;
        }

        const QString requestedTargetPath = destProvider->childPath(destination, destinationNameForCopy(srcProvider, source));
        const QString targetPath = plannedUniqueDestination(requestedTargetPath);
        if (targetPath.isEmpty() || pathExists(targetPath + QStringLiteral(".part"))) {
            return false;
        }
        plannedDestinations.insert(targetPath);

        batchFiles.push_back({source, targetPath, sourceInfo->size});
    }

    if (batchFiles.size() < 2) {
        return false;
    }

    const int waveCount = providerStagedWaveCount(batchFiles);
    qsizetype index = 0;
    int waveIndex = 0;
    int completedTopLevelFiles = 0;
    while (index < batchFiles.size()) {
        if (m_abort) {
            return true;
        }

        QVector<CopyFrame> waveFiles;
        qint64 waveBytes = 0;
        while (index < batchFiles.size() && waveFiles.size() < ProviderStagedBatchMaxFiles) {
            const CopyFrame &file = batchFiles.at(index);
            const qint64 fileSize = (std::max<qint64>)(0, file.size);
            if (!waveFiles.isEmpty() && waveBytes + fileSize > ProviderStagedBatchMaxBytes) {
                break;
            }
            waveBytes += fileSize;
            waveFiles.push_back(file);
            ++index;
        }

        if (waveFiles.isEmpty()) {
            break;
        }
        ++waveIndex;

        QVector<LocalFileMaterializeItem> materializeItems;
        materializeItems.reserve(waveFiles.size());
        for (const CopyFrame &file : std::as_const(waveFiles)) {
            const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(file.sourcePath);
            if (!sourceInfo) {
                return false;
            }
            const QString partialPath = file.destinationPath + QStringLiteral(".part");
            if (pathExists(partialPath) && !removePathIfExists(partialPath)) {
                return false;
            }
            const qint64 fileSize = file.size > 0 ? file.size : sourceInfo->size;
            materializeItems.push_back(LocalFileMaterializeItem{file.sourcePath, file.destinationPath, fileSize});
        }

        qint64 stagedProcessed = 0;
        const qint64 baseBytes = copiedBytes;
        QString materializeError;
        QElapsedTimer materializeTimer;
        const bool materializeLoggingActive = providerMaterializeLoggingEnabled();
        if (materializeLoggingActive) {
            materializeTimer.start();
        }
        const QString downloadLabel = providerBatchLabel(QLatin1StringView("Downloading"), waveIndex, waveCount);
        QMetaObject::invokeMethod(this, [this, downloadLabel]() {
            setCurrentLabel(downloadLabel);
        }, Qt::QueuedConnection);
        const bool materialized = srcProvider->copyToLocalFiles(
            materializeItems,
            [this, baseBytes, waveBytes, totalBytes, &stagedProcessed](const QString &currentSourcePath, qint64 processed, qint64 total) -> bool {
                Q_UNUSED(total)
                Q_UNUSED(currentSourcePath)
                if (m_abort) {
                    return false;
                }
                stagedProcessed = (std::max<qint64>)(0, processed);
                const qint64 stagedBytes = std::clamp<qint64>(stagedProcessed, 0, waveBytes);
                const qint64 progressBytes = std::clamp<qint64>(baseBytes + stagedBytes, 0, totalBytes);
                const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
                updateMetrics(progressBytes, totalBytes);
                return true;
            },
            &materializeError);
        if (!materialized) {
            for (const CopyFrame &file : std::as_const(waveFiles)) {
                removePathIfExists(file.destinationPath + QStringLiteral(".part"));
            }
            if (m_abort) {
                return true;
            }
            throw std::runtime_error(materializeError.trimmed().isEmpty()
                                         ? "Provider local file batch download failed"
                                         : materializeError.toStdString());
        }

        if (materializeLoggingActive) {
            const qint64 elapsedMs = materializeTimer.isValid() ? materializeTimer.elapsed() : 0;
            qInfo().noquote()
                << "[ProviderMaterializeWave]"
                << "operationId=" << m_providerTransferTiming.operationId
                << "sourceScheme=" << srcProvider->scheme()
                << "destinationScheme=" << destProvider->scheme()
                << "files=" << waveFiles.size()
                << "bytes=" << waveBytes
                << "stagedBytes=" << waveBytes
                << "elapsedMs=" << elapsedMs
                << "throughputMiBs=" << mibPerSecond(waveBytes, elapsedMs);
        }

        copiedBytes = (std::min)(totalBytes, copiedBytes + waveBytes);
        completedTopLevelFiles += waveFiles.size();
        const double progress = static_cast<double>(copiedBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
        QMetaObject::invokeMethod(this, [this, progress]() {
            setProgress(progress);
        }, Qt::QueuedConnection);
        QMetaObject::invokeMethod(this, [this, completedTopLevelFiles]() {
            setCompletedItems(completedTopLevelFiles);
        }, Qt::QueuedConnection);
        updateMetrics(copiedBytes, totalBytes);
    }

    for (const CopyFrame &file : std::as_const(batchFiles)) {
        m_committedBatchFinalPaths.insert(file.sourcePath, file.destinationPath);
    }
    return true;
}
