#include "OperationQueue.h"
#include "OperationQueuePrivate.h"

#include "CleanupSubsystem.h"
#include "LinuxAdminBroker.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QUuid>
#include <QVector>

#include <algorithm>
#include <stdexcept>

void OperationQueue::copyPathAsAdministrator(const QString &sourcePath,
                                             const QString &destinationPath,
                                             qint64 totalBytes,
                                             qint64 &copiedBytes,
                                             bool destinationConflictResolved)
{
    struct AdminCopyFrame {
        QString sourcePath;
        QString destinationPath;
        bool conflictResolved = false;
    };

    auto submitAdminRequest = [this](LinuxAdminBroker::Request request,
                                     const LinuxAdminBroker::ProgressCallback &progress = {}) {
        LinuxAdminBroker broker;
        request.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        request.sessionNonce = OperationQueuePrivate::requireLinuxAdminSessionNonce();
        const LinuxAdminBroker::Result result = broker.submitBlocking(request, progress);
        if (!result.success) {
            if (result.errorCode == QLatin1String("operation-canceled")) {
                m_abort = true;
                return;
            }
            throw std::runtime_error((result.errorMessage.isEmpty() ? result.errorCode : result.errorMessage).toStdString());
        }
    };

    auto reportAdminCopyProgress = [this, totalBytes, &copiedBytes](qint64 bytes) {
        copiedBytes = std::min(totalBytes, copiedBytes + std::max<qint64>(1, bytes));
        const double progress = static_cast<double>(copiedBytes) / static_cast<double>(std::max<qint64>(1, totalBytes));
        QMetaObject::invokeMethod(this, [this, progress]() {
            setProgress(progress);
        }, Qt::QueuedConnection);
        updateMetrics(copiedBytes, totalBytes);
    };

    struct AdminPartCleanup {
        QString leaseId;
        bool finalized = false;
        ~AdminPartCleanup()
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
    };

    QVector<AdminCopyFrame> stack;
    stack.push_back({sourcePath, destinationPath, destinationConflictResolved});

    while (!stack.isEmpty()) {
        if (m_abort) {
            return;
        }

        const AdminCopyFrame frame = stack.back();
        stack.pop_back();

        FileProvider *srcProvider = getProviderForPath(frame.sourcePath);
        FileProvider *destProvider = getProviderForPath(frame.destinationPath);
        if (!srcProvider || !destProvider
            || srcProvider->scheme() != QLatin1String("file")
            || destProvider->scheme() != QLatin1String("file")) {
            throw std::runtime_error("Administrator copy is available for local files and folders only");
        }

        const QString fileName = destinationNameForCopy(srcProvider, frame.sourcePath);
        QMetaObject::invokeMethod(this, [this, fileName]() {
            setCurrentLabel(OperationQueuePrivate::operationItemLabel(Type::Copy, fileName));
        }, Qt::QueuedConnection);

        if (srcProvider == destProvider
            && OperationQueuePrivate::samePath(*srcProvider, frame.sourcePath, frame.destinationPath)) {
            reportAdminCopyProgress(totalBytesForPath(frame.sourcePath));
            QMetaObject::invokeMethod(this, [this]() {
                setStatusMessage(QStringLiteral("Some files skipped (source is same as destination)"));
            }, Qt::QueuedConnection);
            continue;
        }

        QString targetPath = frame.destinationPath;
        const bool sourceIsDirectory = isRealDirectory(frame.sourcePath);
        const qint64 frameBaseBytes = copiedBytes;
        const auto fileProgress = [this, totalBytes, frameBaseBytes](qint64 processedBytes, qint64) {
            const qint64 progressBytes = std::clamp<qint64>(
                frameBaseBytes + std::max<qint64>(0, processedBytes),
                0,
                totalBytes);
            const double progress = static_cast<double>(progressBytes) / static_cast<double>(std::max<qint64>(1, totalBytes));
            QMetaObject::invokeMethod(this, [this, progress]() {
                setProgress(progress);
            }, Qt::QueuedConnection);
            updateMetrics(progressBytes, totalBytes);
        };
        if (pathExists(targetPath) && !frame.conflictResolved) {
            ConflictResolution res = waitForResolution(frame.sourcePath, targetPath);
            if (res == ConflictResolution::Skip) {
                reportAdminCopyProgress(totalBytesForPath(frame.sourcePath));
                continue;
            }
            if (res == ConflictResolution::KeepBoth) {
                targetPath = uniqueDestinationPath(targetPath);
            } else if (res == ConflictResolution::Replace && !sourceIsDirectory) {
                LinuxAdminBroker::Request request;
                request.operation = LinuxAdminBroker::Operation::AtomicReplace;
                request.sourcePath = frame.sourcePath;
                request.destinationPath = targetPath;
                request.overwrite = true;
                submitAdminRequest(request, fileProgress);
                if (m_abort) {
                    return;
                }
                reportAdminCopyProgress(totalBytesForPath(frame.sourcePath));
                continue;
            } else if (res == ConflictResolution::Cancel) {
                m_abort = true;
                return;
            }
        }

        if (sourceIsDirectory) {
            if (srcProvider == destProvider
                && OperationQueuePrivate::isDescendantPath(*srcProvider, targetPath, frame.sourcePath)) {
                throw std::runtime_error(
                    QStringLiteral("Cannot copy folder %1 into itself or one of its subfolders")
                        .arg(frame.sourcePath)
                        .toStdString());
            }

            LinuxAdminBroker::Request request;
            request.operation = LinuxAdminBroker::Operation::MakeDirectory;
            request.destinationPath = targetPath;
            submitAdminRequest(request);
            if (m_abort) {
                return;
            }

            const QStringList children = childPaths(frame.sourcePath);
            for (auto it = children.crbegin(); it != children.crend(); ++it) {
                const QString childDestination = destProvider->childPath(targetPath, destinationNameForCopy(srcProvider, *it));
                stack.push_back({*it, childDestination, false});
            }
            continue;
        }

        LinuxAdminBroker::Request request;
        request.operation = LinuxAdminBroker::Operation::CopyFile;
        request.sourcePath = frame.sourcePath;
        const QString tempPath = targetPath + QStringLiteral(".part");
        AdminPartCleanup partCleanup;
        CleanupSubsystem::instance().registerArtifact(
            CleanupArtifactKind::PartFile,
            tempPath,
            QFileInfo(tempPath).absolutePath(),
            false,
            &partCleanup.leaseId);
        if (pathExists(tempPath)) {
            LinuxAdminBroker::Request deleteRequest;
            deleteRequest.operation = LinuxAdminBroker::Operation::DeletePath;
            deleteRequest.sourcePath = tempPath;
            submitAdminRequest(deleteRequest);
            if (m_abort) {
                return;
            }
        }
        request.destinationPath = tempPath;
        submitAdminRequest(request, fileProgress);
        if (m_abort) {
            return;
        }
        if (pathExists(targetPath)) {
            LinuxAdminBroker::Request deleteRequest;
            deleteRequest.operation = LinuxAdminBroker::Operation::DeletePath;
            deleteRequest.sourcePath = tempPath;
            submitAdminRequest(deleteRequest);
            throw std::runtime_error(QStringLiteral("Cannot finalize %1: destination already exists")
                                         .arg(targetPath)
                                         .toStdString());
        }
        LinuxAdminBroker::Request renameRequest;
        renameRequest.operation = LinuxAdminBroker::Operation::RenamePath;
        renameRequest.sourcePath = tempPath;
        renameRequest.destinationPath = targetPath;
        submitAdminRequest(renameRequest);
        if (m_abort) {
            return;
        }
        partCleanup.finalized = true;
        reportAdminCopyProgress(totalBytesForPath(frame.sourcePath));
    }
}

void OperationQueue::createFolderAsAdministratorPath(const QString &path)
{
    LinuxAdminBroker broker;

    LinuxAdminBroker::Request request;
    request.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.sessionNonce = OperationQueuePrivate::requireLinuxAdminSessionNonce();
    request.operation = LinuxAdminBroker::Operation::MakeDirectory;
    request.destinationPath = path;

    const LinuxAdminBroker::Result result = broker.submitBlocking(request);
    if (!result.success) {
        throw std::runtime_error((result.errorMessage.isEmpty() ? result.errorCode : result.errorMessage).toStdString());
    }
}

void OperationQueue::deletePathAsAdministrator(const QString &path)
{
    LinuxAdminBroker broker;

    LinuxAdminBroker::Request request;
    request.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.sessionNonce = OperationQueuePrivate::requireLinuxAdminSessionNonce();
    request.operation = LinuxAdminBroker::Operation::DeletePath;
    request.sourcePath = path;

    const LinuxAdminBroker::Result result = broker.submitBlocking(request, [this](qint64 processedEntries, qint64 totalEntries) {
        Q_UNUSED(totalEntries)
        updateMetrics(processedEntries, std::max<qint64>(processedEntries, 1));
    });
    if (!result.success) {
        if (result.errorCode == QLatin1String("operation-canceled")) {
            m_abort = true;
            return;
        }
        throw std::runtime_error((result.errorMessage.isEmpty() ? result.errorCode : result.errorMessage).toStdString());
    }
}

