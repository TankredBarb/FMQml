#include "OperationQueue.h"
#include "OperationQueuePrivate.h"

#include "ArchiveFileProvider.h"
#include "ArchiveOperationCallbacks.h"
#include "ArchiveSupport.h"
#include "CleanupSubsystem.h"
#include "LinuxAdminBroker.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QMetaObject>
#include <QScopeGuard>
#include <QUuid>
#include <QVector>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

using OperationQueuePrivate::archiveContainerKey;
using OperationQueuePrivate::cheapArchiveSelectionBytes;
using OperationQueuePrivate::isDescendantPath;
using OperationQueuePrivate::pathLogName;
using OperationQueuePrivate::providerFailureReason;
using OperationQueuePrivate::providerTransferTimingEnabled;
using OperationQueuePrivate::samePath;

OperationQueue::OperationResult OperationQueue::execute(const Request &request)
{
    OperationResult result;
    result.request = request;
    m_committedBatchFinalPaths.clear();

    const OperationQueuePrivate::ExecutionContext context{
        request,
        [this]() { return m_abort.load(); },
        [this](double progress) {
            QMetaObject::invokeMethod(this, [this, progress]() {
                setProgress(progress);
            }, Qt::QueuedConnection);
        },
        [this](int completedItems) {
            QMetaObject::invokeMethod(this, [this, completedItems]() {
                setCompletedItems(completedItems);
            }, Qt::QueuedConnection);
        },
        [this](const QString &label) {
            QMetaObject::invokeMethod(this, [this, label]() {
                setCurrentLabel(label);
            }, Qt::QueuedConnection);
        },
        [this](const QString &status) {
            QMetaObject::invokeMethod(this, [this, status]() {
                setStatusMessage(status);
            }, Qt::QueuedConnection);
        },
        [this](const QString &source, const QString &destination) {
            return waitForResolution(source, destination);
        }
    };
    const OperationQueuePrivate::ProviderTransferEngine providerTransfers{
        context,
        [this](const QStringList &sources, const QString &destination,
               qint64 totalBytes, qint64 &copiedBytes) {
            return copySmallLocalFilesToProviderBatch(
                sources, destination, totalBytes, copiedBytes);
        },
        [this](const QStringList &sources, const QString &destination,
               qint64 totalBytes, qint64 &copiedBytes) {
            return copyProviderFilesToLocalBatch(
                sources, destination, totalBytes, copiedBytes);
        },
        [this](const QStringList &sources, const QString &destination,
               qint64 totalBytes, qint64 &copiedBytes) {
            return copyProviderFilesToProviderStagedBatch(
                sources, destination, totalBytes, copiedBytes);
        },
        [this](const QStringList &sources, int sourceIndex, const QString &destination,
               qint64 totalBytes, qint64 &copiedBytes) {
            return copyNextSmallLocalFilesToProviderBatch(
                sources, sourceIndex, destination, totalBytes, copiedBytes);
        }
    };

    resetProviderTransferTiming(request);

    setCurrentThreadAbortChecker([&context]() {
        return context.isAborted();
    });
    ArchiveOperationCallbacks::setCurrent({
        [&context]() { return context.isAborted(); },
        [](qint64 processedBytes) {
            OperationQueue::reportCurrentThreadProgressBytes(processedBytes);
        }
    });

    struct CacheCleaner {
        OperationQueue *owner = nullptr;
        QHash<QString, std::shared_ptr<FileProvider>> &cache;
        ~CacheCleaner() {
            if (owner) {
                owner->logProviderTransferTimingSummary();
            }
            for (const std::shared_ptr<FileProvider> &provider : std::as_const(cache)) {
                if (provider) {
                    provider->flushPendingStorageInfoRefresh();
                }
            }
            cache.clear();
            OperationQueue::setCurrentThreadAbortChecker(nullptr);
            OperationQueue::setCurrentThreadProgressReporter(nullptr);
            ArchiveOperationCallbacks::clearCurrent();
            ArchiveFileProvider::setCurrentThreadTemporaryParent({});
        }
    } cleaner{this, m_providerCache};

    if (!request.destination.isEmpty()) {
        FileProvider *destProvider = getProviderForPath(request.destination);
        if (destProvider && destProvider->scheme() == QLatin1String("file")) {
            ArchiveFileProvider::setCurrentThreadTemporaryParent(request.destination);
        }
    }

    qint64 currentProgressBytes = 0;
    const int totalFileCount = request.type == Type::CreateFolder ? 1 : request.sources.size();
    OperationQueuePrivate::ResultAccumulator accumulator(result, totalFileCount);
    const bool isCountingItems = (request.type == Type::Delete);
    QElapsedTimer totalBytesTimer;
    if (providerTransferTimingEnabled()) {
        totalBytesTimer.start();
        qInfo().noquote()
            << "[ProviderTransferPhase]"
            << "phase=totalBytesStart"
            << "sources=" << request.sources.size()
            << "destination=" << pathLogName(request.destination);
    }
    const bool extractsArchiveItems = (request.type == Type::Copy || request.type == Type::Move)
        && std::any_of(request.sources.cbegin(), request.sources.cend(), [](const QString &source) {
            return ArchiveSupport::isArchivePath(source);
        });
    if (request.type == Type::Extract || extractsArchiveItems) {
        context.reportLabel(QStringLiteral("Preparing extraction..."));
    } else if (request.type == Type::Compress) {
        context.reportLabel(QStringLiteral("Preparing archive..."));
    } else {
        context.reportLabel(QStringLiteral("Scanning transfer..."));
    }
    const qint64 archiveSelectionBytes =
        (request.type == Type::Copy || request.type == Type::Move)
            ? cheapArchiveSelectionBytes(request.sources)
            : -1;
    const qint64 totalBytes = isCountingItems
        ? static_cast<qint64>(totalFileCount)
        : std::max<qint64>(1, request.type == Type::Extract
            ? totalBytesForExtraction(request.sources)
            : (archiveSelectionBytes >= 0
                ? archiveSelectionBytes
                : totalBytesFor(request.sources)));
    if (providerTransferTimingEnabled()) {
        qInfo().noquote()
            << "[ProviderTransferPhase]"
            << "phase=totalBytesFinish"
            << "bytes=" << totalBytes
            << "elapsedMs=" << totalBytesTimer.elapsed();
    }

    QMetaObject::invokeMethod(this, [this, totalFileCount]() {
        setTotalItems(totalFileCount);
    }, Qt::QueuedConnection);
    context.reportCompletedItems(0);

    if (request.type == Type::CreateFolder) {
        const QString folderName = request.sources.value(0).trimmed();
        FileProvider *destProvider = getProviderForPath(request.destination);
        const QString folderPath = destProvider
            ? destProvider->childPath(request.destination, folderName)
            : QString();
        try {
            if (folderPath.isEmpty()) {
                throw std::runtime_error("Cannot create folder: destination is invalid");
            }
            if (request.administrator) {
                QString adminFolderPath = folderPath;
                if (pathExists(adminFolderPath)) {
                    for (int i = 1; i < 1000; ++i) {
                        const QString candidate = destProvider->childPath(
                            request.destination,
                            QStringLiteral("%1 (%2)").arg(folderName).arg(i));
                        if (!pathExists(candidate)) {
                            adminFolderPath = candidate;
                            break;
                        }
                    }
                }
                createFolderAsAdministratorPath(adminFolderPath);
            } else if (!makePath(folderPath)) {
                throw std::runtime_error(QStringLiteral("Cannot create folder %1").arg(folderPath).toStdString());
            }
            accumulator.setSucceededCount(1);
            context.reportCompletedItems(1);
            context.reportProgress(1.0);
        } catch (const std::exception &exception) {
            accumulator.recordFailure(folderPath.isEmpty() ? request.destination : folderPath, QString::fromUtf8(exception.what()));
        }
        return result;
    }

    if (request.type == Type::Compress) {
        try {
            context.reportLabel(QStringLiteral("Compressing..."));
            resetTransferMetricsBaseline();
            compressPathsToSevenZip(request.sources, request.destination, totalBytes);
            if (context.isAborted()) {
                result.aborted = true;
                return result;
            }
            accumulator.setSucceededPaths(request.sources);
            result.resultPaths.append(request.destination);
            context.reportCompletedItems(totalFileCount);
            context.reportProgress(1.0);
        } catch (const std::exception &exception) {
            accumulator.recordFailure(request.destination, QString::fromUtf8(exception.what()));
        }
        return result;
    }

    if (request.type == Type::Extract) {
        resetTransferMetricsBaseline();
    }

    if (request.type == Type::Copy && request.administrator) {
        if (request.destination.isEmpty()) {
            accumulator.recordFailure({}, QStringLiteral("Administrator copy destination is empty"));
            return result;
        }
        FileProvider *destProvider = getProviderForPath(request.destination);
        if (!destProvider || destProvider->scheme() != QLatin1String("file")) {
            accumulator.recordFailure(request.destination, QStringLiteral("Administrator copy is available for local folders only"));
            return result;
        }

        for (int i = 0; i < totalFileCount; ++i) {
            if (context.isAborted()) {
                result.aborted = true;
                return result;
            }

            const QString &source = request.sources.at(i);
            FileProvider *srcProvider = getProviderForPath(source);
            if (!srcProvider || srcProvider->scheme() != QLatin1String("file")) {
                accumulator.recordFailure(source, QStringLiteral("Administrator copy is available for local files and folders only"));
                continue;
            }
            const bool sourceIsDirectory = isRealDirectory(source);
            const QString destinationPath = destProvider->childPath(
                request.destination,
                destinationNameForCopy(srcProvider, source));

            try {
                QString finalPath = destinationPath;
                bool overwrite = false;
                bool destinationConflictResolved = false;
                if (pathExists(finalPath)) {
                    const ConflictResolution res = context.resolveConflict(source, finalPath);
                    if (res == ConflictResolution::Skip) {
                        context.reportCompletedItems(i + 1);
                        continue;
                    }
                    if (res == ConflictResolution::KeepBoth) {
                        finalPath = uniqueDestinationPath(finalPath);
                        destinationConflictResolved = true;
                    } else if (res == ConflictResolution::Replace && !sourceIsDirectory) {
                        overwrite = true;
                        destinationConflictResolved = true;
                    } else if (res == ConflictResolution::Replace) {
                        if (!isRealDirectory(finalPath)) {
                            throw std::runtime_error(
                                QStringLiteral("Cannot replace %1 with a folder as Administrator")
                                    .arg(finalPath)
                                    .toStdString());
                        }
                        destinationConflictResolved = true;
                    } else if (res == ConflictResolution::Cancel) {
                        result.aborted = true;
                        return result;
                    }
                }

                if (overwrite) {
                    LinuxAdminBroker broker;
                    LinuxAdminBroker::Request adminRequest;
                    adminRequest.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                    adminRequest.sessionNonce = OperationQueuePrivate::requireLinuxAdminSessionNonce();
                    adminRequest.operation = LinuxAdminBroker::Operation::AtomicReplace;
                    adminRequest.sourcePath = source;
                    adminRequest.destinationPath = finalPath;
                    adminRequest.overwrite = true;
                    struct AdminReplacePartCleanup {
                        QString leaseId;
                        bool finalized = false;
                        ~AdminReplacePartCleanup()
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
                    const QString partPath = finalPath + QStringLiteral(".fm-admin-replace-part");
                    CleanupSubsystem::instance().registerArtifact(
                        CleanupArtifactKind::PartFile,
                        partPath,
                        QFileInfo(partPath).absolutePath(),
                        false,
                        &partCleanup.leaseId);
                    const LinuxAdminBroker::Result adminResult = broker.submitBlocking(adminRequest);
                    if (!adminResult.success) {
                        if (adminResult.errorCode == QLatin1String("operation-canceled")) {
                            result.aborted = true;
                            return result;
                        }
                        throw std::runtime_error(adminResult.errorMessage.toStdString());
                    }
                    partCleanup.finalized = true;
                    currentProgressBytes += std::max<qint64>(1, totalBytesForPath(source));
                } else {
                    copyPathAsAdministrator(source, finalPath, totalBytes, currentProgressBytes, destinationConflictResolved);
                }
                if (context.isAborted()) {
                    result.aborted = true;
                    return result;
                }

                accumulator.addSuccess(source, finalPath);
                const double progress = static_cast<double>(i + 1) / static_cast<double>(totalFileCount);
                context.reportCompletedItems(i + 1);
                context.reportProgress(progress);
            } catch (const std::exception &exception) {
                accumulator.recordFailure(source, QString::fromUtf8(exception.what()));
            }
        }

        if (context.isAborted()) {
            result.aborted = true;
        } else if (result.failedCount > 0) {
            accumulator.summarizePartialFailure();
        }
        return result;
    }

    if (request.type == Type::Copy || request.type == Type::Move || request.type == Type::Duplicate) {
        for (int sourceIndex = 0; sourceIndex < request.sources.size(); ++sourceIndex) {
            const QString &source = request.sources.at(sourceIndex);
            if (ArchiveSupport::isArchivePath(source)) {
                continue;
            }

            FileProvider* srcProvider = getProviderForPath(source);
            if (!isRealDirectory(source)) {
                continue;
            }

            const QString sourceName = destinationNameForCopy(srcProvider, source);
            const QString explicitDestination = request.explicitDestinations.size() == request.sources.size()
                ? request.explicitDestinations.at(sourceIndex) : QString();
            FileProvider* destProvider = request.type == Type::Duplicate
                ? srcProvider
                : getProviderForPath(explicitDestination.isEmpty() ? request.destination : explicitDestination);
            const QString destinationPath = request.type == Type::Duplicate
                ? duplicateDestinationPath(source)
                : (!explicitDestination.isEmpty() ? explicitDestination
                   : (request.destination.isEmpty()
                    ? QString()
                    : destProvider->childPath(request.destination, sourceName)));

            if (srcProvider == destProvider && isDescendantPath(*srcProvider, destinationPath, source)) {
                const QString message = QStringLiteral("Cannot %1 folder %2 into itself or one of its subfolders")
                    .arg(request.type == Type::Copy ? QStringLiteral("copy") : QStringLiteral("move"))
                    .arg(source);
                accumulator.recordBatchFailure(totalFileCount, source, message);
                return result;
            }
        }
    }

    if ((request.type == Type::Copy || request.type == Type::Move)
        && totalFileCount > 0
        && !request.destination.isEmpty()) {
        FileProvider *destProvider = getProviderForPath(request.destination);
        const QString firstContainer = archiveContainerKey(request.sources.constFirst());
        bool canExtractArchiveSelection = destProvider
            && destProvider->scheme() == QLatin1String("file")
            && !firstContainer.isEmpty();
        QStringList archiveSources;
        QStringList finalPaths;

        if (canExtractArchiveSelection) {
            for (const QString &source : request.sources) {
                if (!ArchiveSupport::isArchivePath(source)
                    || archiveContainerKey(source) != firstContainer
                    || ArchiveSupport::splitArchiveTokens(source).size() < 2
                    || ArchiveSupport::archiveBrowsePath(source) == QLatin1String("/")) {
                    canExtractArchiveSelection = false;
                    break;
                }

                FileProvider *srcProvider = getProviderForPath(source);
                const QString sourceName = srcProvider->fileName(source);
                if (sourceName.isEmpty()) {
                    canExtractArchiveSelection = false;
                    break;
                }

                QString finalPath = destProvider->childPath(request.destination, sourceName);
                if (pathExists(finalPath)) {
                    ConflictResolution res = context.resolveConflict(source, finalPath);
                    if (res == ConflictResolution::Skip) {
                        currentProgressBytes += (std::max<qint64>)(1, totalBytesForPath(source));
                        continue;
                    }
                    if (res == ConflictResolution::KeepBoth) {
                        finalPath = uniqueDestinationPath(finalPath);
                    } else if (res == ConflictResolution::Replace) {
                        const QString physicalPath = ArchiveSupport::physicalArchivePath(source);
                        FileProvider *localProvider = getProviderForPath(physicalPath);
                        if (samePath(*localProvider, finalPath, physicalPath)
                            || isDescendantPath(*localProvider, physicalPath, finalPath)) {
                            finalPath = uniqueDestinationPath(finalPath);
                            context.reportStatus(
                                QStringLiteral("Cannot replace the source archive. The item has been renamed."));
                        } else if (!removePathIfExists(finalPath)) {
                            accumulator.recordBatchFailure(
                                totalFileCount, finalPath,
                                QStringLiteral("Cannot replace %1").arg(finalPath));
                            return result;
                        }
                    } else if (res == ConflictResolution::Cancel) {
                        result.aborted = true;
                        return result;
                    }
                }

                archiveSources.append(source);
                finalPaths.append(finalPath);
            }
        }

        if (canExtractArchiveSelection && archiveSources.isEmpty()) {
            context.reportProgress(1.0);
            return result;
        }

        if (canExtractArchiveSelection) {
            QString error;
            context.reportLabel(QStringLiteral("Extracting archive items..."));
            resetTransferMetricsBaseline();
            const bool extracted = ArchiveFileProvider::extractArchiveItemsTo(
                archiveSources,
                finalPaths,
                &error,
                [this, &context](uint64_t processed, uint64_t backendTotal) -> bool {
                    if (context.isAborted()) {
                        return false;
                    }
                    const double fraction = backendTotal > 0
                        ? std::clamp(static_cast<double>(processed) / static_cast<double>(backendTotal), 0.0, 1.0)
                        : 0.0;
                    context.reportProgress(fraction);
                    updateMetrics(
                        static_cast<qint64>((std::min<uint64_t>)(processed, std::numeric_limits<qint64>::max())),
                        static_cast<qint64>((std::min<uint64_t>)(backendTotal, std::numeric_limits<qint64>::max())));
                    return true;
                });

            if (!extracted) {
                if (context.isAborted()) {
                    result.aborted = true;
                    return result;
                }
                if (!error.contains(QStringLiteral("7-Zip"), Qt::CaseInsensitive)
                    && !error.contains(QStringLiteral("cached"), Qt::CaseInsensitive)) {
                    accumulator.recordBatchFailure(
                        archiveSources.size(), archiveSources.value(0),
                        error.isEmpty() ? QStringLiteral("Cannot extract selected archive items") : error);
                    return result;
                }
            } else {
                currentProgressBytes = totalBytes;
                for (int i = 0; i < archiveSources.size(); ++i) {
                    accumulator.addSuccess(archiveSources.at(i), finalPaths.at(i));
                }
                context.reportCompletedItems(totalFileCount);
                context.reportProgress(1.0);
                return result;
            }
        }
    }

    constexpr bool kEnableArchiveBatchCopy = false;
    if (kEnableArchiveBatchCopy && request.type == Type::Copy && totalFileCount > 1 && !request.destination.isEmpty()) {
        FileProvider *destProvider = getProviderForPath(request.destination);
        const QString firstContainer = archiveContainerKey(request.sources.constFirst());
        bool canBatchArchiveFiles = destProvider && destProvider->scheme() == QLatin1String("file") && !firstContainer.isEmpty();
        QStringList batchSources;
        QStringList batchFinalPaths;
        QStringList batchTempPaths;
        QStringList batchTempLeaseIds;
        QVector<bool> batchTempFinalized;

        if (canBatchArchiveFiles) {
            for (const QString &source : request.sources) {
                FileProvider *srcProvider = getProviderForPath(source);
                const auto info = srcProvider->entryInfo(source);
                if (!info || info->isDirectory || archiveContainerKey(source) != firstContainer) {
                    canBatchArchiveFiles = false;
                    break;
                }

                QString finalPath = destProvider->childPath(request.destination, info->name);
                if (pathExists(finalPath)) {
                    ConflictResolution res = context.resolveConflict(source, finalPath);
                    if (res == ConflictResolution::Skip) {
                        continue;
                    }
                    if (res == ConflictResolution::KeepBoth) {
                        finalPath = uniqueDestinationPath(finalPath);
                    } else if (res == ConflictResolution::Replace) {
                        if (!removePathIfExists(finalPath)) {
                            accumulator.recordBatchFailure(
                                totalFileCount, finalPath,
                                QStringLiteral("Cannot replace %1").arg(finalPath));
                            return result;
                        }
                    } else if (res == ConflictResolution::Cancel) {
                        result.aborted = true;
                        return result;
                    }
                }

                const QString tempPath = finalPath + QStringLiteral(".part");
                if (pathExists(tempPath) && !removePathIfExists(tempPath)) {
                    accumulator.recordBatchFailure(
                        totalFileCount, tempPath,
                        QStringLiteral("Cannot replace temporary file %1").arg(tempPath));
                    return result;
                }

                QString tempLeaseId;
                CleanupSubsystem::instance().registerArtifact(
                    CleanupArtifactKind::PartFile,
                    tempPath,
                    QFileInfo(tempPath).absolutePath(),
                    false,
                    &tempLeaseId);

                batchSources.append(source);
                batchFinalPaths.append(finalPath);
                batchTempPaths.append(tempPath);
                batchTempLeaseIds.append(tempLeaseId);
                batchTempFinalized.append(false);
            }
        }

        if (canBatchArchiveFiles && batchSources.isEmpty()) {
            context.reportProgress(1.0);
            return result;
        }

        if (canBatchArchiveFiles) {
            const auto batchTempCleanup = qScopeGuard([&]() {
                for (int i = 0; i < batchTempLeaseIds.size(); ++i) {
                    const QString &leaseId = batchTempLeaseIds.at(i);
                    if (leaseId.isEmpty()) {
                        continue;
                    }
                    if (batchTempFinalized.value(i)) {
                        CleanupSubsystem::instance().completeWithoutDelete(leaseId);
                    } else {
                        CleanupSubsystem::instance().scheduleDeleteOnFailure(leaseId);
                    }
                }
            });

            QString error;
            const bool extracted = ArchiveFileProvider::extractArchiveEntriesTo(
                batchSources,
                batchTempPaths,
                &error,
                [this, &context, totalBytes](uint64_t processed) -> bool {
                    if (context.isAborted()) {
                        return false;
                    }
                    if (totalBytes > 0) {
                        const uint64_t maxBytes = static_cast<uint64_t>((std::numeric_limits<qint64>::max)());
                        const qint64 progressBytes = static_cast<qint64>((std::min)(processed, maxBytes));
                        const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                        context.reportProgress(progress);
                        updateMetrics(progressBytes, totalBytes);
                    }
                    return true;
                });

            if (!extracted) {
                for (const QString &tempPath : std::as_const(batchTempPaths)) {
                    removePathIfExists(tempPath);
                }
                if (context.isAborted()) {
                    result.aborted = true;
                    return result;
                }
                accumulator.recordBatchFailure(
                    batchSources.size(), batchSources.value(0),
                    error.isEmpty() ? QStringLiteral("Cannot extract selected archive entries") : error);
                return result;
            }

            for (int i = 0; i < batchTempPaths.size(); ++i) {
                if (context.isAborted()) {
                    for (const QString &tempPath : std::as_const(batchTempPaths)) {
                        removePathIfExists(tempPath);
                    }
                    result.aborted = true;
                    return result;
                }
                if (pathExists(batchFinalPaths.at(i)) && !removePathIfExists(batchFinalPaths.at(i))) {
                    removePathIfExists(batchTempPaths.at(i));
                    accumulator.recordBatchFailure(
                        batchSources.size(), batchFinalPaths.at(i),
                        QStringLiteral("Cannot replace %1").arg(batchFinalPaths.at(i)));
                    return result;
                }
                if (!destProvider->movePath(batchTempPaths.at(i), batchFinalPaths.at(i))) {
                    removePathIfExists(batchTempPaths.at(i));
                    accumulator.recordBatchFailure(
                        batchSources.size(), batchFinalPaths.at(i),
                        QStringLiteral("Cannot finalize %1").arg(batchFinalPaths.at(i)));
                    return result;
                }
                batchTempFinalized[i] = true;
            }

            currentProgressBytes = totalBytes;
            context.reportProgress(1.0);
            return result;
        }
    }

    const auto providerBatchResult = providerTransfers.copyWholeRequest(
        totalBytes, currentProgressBytes);
    if (providerBatchResult == OperationQueuePrivate::ProviderTransferEngine::BatchResult::Aborted) {
        result.aborted = true;
        return result;
    }
    if (providerBatchResult == OperationQueuePrivate::ProviderTransferEngine::BatchResult::Succeeded) {
        accumulator.setSucceededPaths(request.sources);
        result.finalPathsBySource = m_committedBatchFinalPaths;
        for (const QString &source : request.sources) {
            QString finalPath = result.finalPathsBySource.value(source);
            if (!finalPath.isEmpty()) {
                finalPath = getProviderForPath(finalPath)->committedPath(finalPath);
                result.finalPathsBySource.insert(source, finalPath);
            }
            if (!finalPath.isEmpty()) result.resultPaths.append(finalPath);
        }
        context.reportCompletedItems(totalFileCount);
        return result;
    }

    for (int i = 0; i < totalFileCount; ++i) {
        if (context.isAborted()) {
            result.aborted = true;
            return result;
        }
        const QString &source = request.sources.at(i);
        FileProvider* srcProvider = getProviderForPath(source);
        const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(source);
        const QString sourceName = sourceInfo ? sourceInfo->name : srcProvider->fileName(source);
        const QString destinationName = destinationNameForCopy(srcProvider, source);
        const QString explicitDestination = request.explicitDestinations.size() == request.sources.size()
            ? request.explicitDestinations.at(i) : QString();
        FileProvider* destProvider = request.type == Type::Duplicate
            ? srcProvider
            : getProviderForPath(explicitDestination.isEmpty() ? request.destination : explicitDestination);
        const QString destinationPath = request.type == Type::Duplicate
            ? duplicateDestinationPath(source)
            : (!explicitDestination.isEmpty() ? explicitDestination
               : (request.destination.isEmpty() ? QString() : destProvider->childPath(request.destination, destinationName)));
        if (request.type == Type::Copy && request.explicitDestinations.isEmpty()) {
            const int batchCount = providerTransfers.copyNextUploadBatch(
                i, totalBytes, currentProgressBytes);
            if (context.isAborted()) {
                result.aborted = true;
                return result;
            }
            if (batchCount > 0) {
                accumulator.addSuccesses(request.sources.mid(i, batchCount));
                for (int batchIndex = i; batchIndex < i + batchCount; ++batchIndex) {
                    const QString &batchSource = request.sources.at(batchIndex);
                    QString batchFinalPath = m_committedBatchFinalPaths.value(batchSource);
                    if (!batchFinalPath.isEmpty()) {
                        batchFinalPath = getProviderForPath(batchFinalPath)->committedPath(batchFinalPath);
                    }
                    if (!batchFinalPath.isEmpty()) {
                        result.finalPathsBySource.insert(batchSource, batchFinalPath);
                        result.resultPaths.append(batchFinalPath);
                    }
                }
                i += batchCount - 1;
                context.reportCompletedItems(i + 1);
                continue;
            }
        }
        const int failureCountBefore = result.failedCount;
        QString committedPath;
        bool itemSkipped = false;

        try {
            if (request.type == Type::Copy) {
                committedPath = copyPath(source, destinationPath, totalBytes, currentProgressBytes, Type::Copy,
                                         !explicitDestination.isEmpty());
                if (!committedPath.isEmpty()) {
                    committedPath = destProvider->committedPath(committedPath);
                }
                itemSkipped = committedPath.isEmpty() && !context.isAborted();
            } else if (request.type == Type::Duplicate) {
                committedPath = copyPath(source, destinationPath, totalBytes, currentProgressBytes, Type::Duplicate);
                itemSkipped = committedPath.isEmpty() && !context.isAborted();
            } else if (request.type == Type::Extract) {
                context.reportLabel(
                    OperationQueuePrivate::operationItemLabel(Type::Extract, sourceName));
                const QStringList extractedPaths = extractArchiveContents(
                    source, request.destination, totalBytes, currentProgressBytes);
                for (const QString &extractedPath : extractedPaths) {
                    if (!result.resultPaths.contains(extractedPath)) {
                        result.resultPaths.append(extractedPath);
                    }
                }
            } else if (request.type == Type::Move) {
                committedPath = movePath(source, destinationPath, totalBytes, currentProgressBytes);
                itemSkipped = committedPath.isEmpty() && !context.isAborted();
            } else if (request.type == Type::Delete) {
                context.reportLabel(
                    OperationQueuePrivate::operationItemLabel(Type::Delete, sourceName));

                const bool sourceIsDirectory = isRealDirectory(source);
                if (request.administrator) {
                    deletePathAsAdministrator(source);
                } else if (sourceIsDirectory) {
                    if (!removePathIfExists(source)) {
                        const QString message = providerFailureReason(
                            srcProvider,
                            QStringLiteral("Cannot delete folder: it may be in use or protected"));
                        throw std::runtime_error(message.toStdString());
                    }
                } else {
                    if (!removePathIfExists(source)) {
                        const QString message = providerFailureReason(
                            srcProvider,
                            QStringLiteral("Cannot delete file: it may be in use or protected"));
                        throw std::runtime_error(message.toStdString());
                    }
                }

                currentProgressBytes += 1;
                const double progress = static_cast<double>(i + 1) / static_cast<double>(totalFileCount);
                context.reportProgress(progress);
            }
        } catch (const std::exception &exception) {
            accumulator.recordFailure(source, QString::fromUtf8(exception.what()));
        }

        if (context.isAborted()) {
            result.aborted = true;
            return result;
        }

        context.reportCompletedItems(i + 1);

        if (result.failedCount == failureCountBefore && !itemSkipped) {
            accumulator.addSuccess(source, request.type == Type::Delete ? QString() : committedPath);
        }
    }

    if (context.isAborted()) {
        result.aborted = true;
    } else if (result.failedCount > 0) {
        accumulator.summarizePartialFailure();
    }
    return result;
}
