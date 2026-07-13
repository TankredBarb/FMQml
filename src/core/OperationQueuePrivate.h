#pragma once

#include "OperationQueue.h"

#include "LinuxAdminBroker.h"

#include <QDir>
#include <QVector>

#include <stdexcept>
#include <utility>

namespace OperationQueuePrivate {

inline constexpr qint64 SmallFileLimit = 10 * 1024 * 1024;
inline constexpr qint64 MetricsUpdateIntervalMs = 500;
inline constexpr qint64 ProviderLocalBatchFileLimit = 16 * 1024 * 1024;
inline constexpr qsizetype ProviderStagedBatchMaxFiles = 64;
inline constexpr qint64 ProviderStagedBatchMaxBytes = 128 * 1024 * 1024;
inline constexpr qint64 ProviderUnknownSizeProgressBytes = 16 * 1024 * 1024;

struct CopyFrame
{
    QString sourcePath;
    QString destinationPath;
    qint64 size = 0;
};

bool providerBatchLoggingEnabled();
bool providerTransferTimingEnabled();
bool providerMaterializeLoggingEnabled();
double mibPerSecond(qint64 bytes, qint64 elapsedMs);
QString pathLogName(const QString &path);
QString providerBatchLabel(QLatin1StringView phase, int waveIndex, int waveCount = 0);
QString operationFolderLabel(QLatin1StringView action, const QString &path);
int providerStagedWaveCount(const QVector<CopyFrame> &batchFiles);
qint64 keepExistingLocalUploadItems(QVector<LocalFileCopyItem> &items);
QString archiveContainerKey(const QString &path);
QString explicitProviderScheme(const QString &path);
bool quotaManagedRemotePath(const QString &path);
bool requestUsesQuotaManagedRemoteProvider(const OperationQueue::Request &request);
QString providerCacheKeyForPath(const QString &path);
QString allocateProviderTransferFile(const QString &destinationPath,
                                     const QString &fileName,
                                     QString *leaseId);
QString allocateNeutralProviderTransferFile(const QString &fileName, QString *leaseId);
qint64 cheapArchiveSelectionBytes(const QStringList &sources);
QString formatSize(qint64 bytes);
QString formatTime(qint64 seconds);
QString operationName(OperationQueue::Type type);
QString primaryErrorPath(const OperationQueue::Request &request);
QString providerFailureReason(FileProvider *provider, const QString &fallback);
QString summarizedFailedItems(const QStringList &paths, int failedCount);

class ExecutionContext
{
public:
    using AbortCheck = std::function<bool()>;
    using ProgressReporter = std::function<void(double)>;
    using CompletedItemsReporter = std::function<void(int)>;
    using TextReporter = std::function<void(const QString &)>;
    using ConflictResolver = std::function<OperationQueue::ConflictResolution(
        const QString &, const QString &)>;

    const OperationQueue::Request &request;
    AbortCheck isAborted;
    ProgressReporter reportProgress;
    CompletedItemsReporter reportCompletedItems;
    TextReporter reportLabel;
    TextReporter reportStatus;
    ConflictResolver resolveConflict;
};

class ProviderTransferEngine
{
public:
    enum class BatchResult {
        NotHandled,
        Succeeded,
        Aborted
    };

    using WholeBatchTransfer = std::function<bool(
        const QStringList &, const QString &, qint64, qint64 &)>;
    using IncrementalBatchTransfer = std::function<int(
        const QStringList &, int, const QString &, qint64, qint64 &)>;

    ProviderTransferEngine(const ExecutionContext &context,
                           WholeBatchTransfer uploadBatch,
                           WholeBatchTransfer downloadBatch,
                           WholeBatchTransfer stagedProviderBatch,
                           IncrementalBatchTransfer incrementalUploadBatch)
        : m_context(context)
        , m_uploadBatch(std::move(uploadBatch))
        , m_downloadBatch(std::move(downloadBatch))
        , m_stagedProviderBatch(std::move(stagedProviderBatch))
        , m_incrementalUploadBatch(std::move(incrementalUploadBatch))
    {
    }

    BatchResult copyWholeRequest(qint64 totalBytes, qint64 &copiedBytes) const
    {
        const OperationQueue::Request &request = m_context.request;
        if (request.type != OperationQueue::Type::Copy
            || !request.explicitDestinations.isEmpty()) {
            return BatchResult::NotHandled;
        }

        const bool handled = m_uploadBatch(request.sources, request.destination, totalBytes, copiedBytes)
            || m_downloadBatch(request.sources, request.destination, totalBytes, copiedBytes)
            || m_stagedProviderBatch(request.sources, request.destination, totalBytes, copiedBytes);
        if (!handled) {
            return BatchResult::NotHandled;
        }
        return m_context.isAborted() ? BatchResult::Aborted : BatchResult::Succeeded;
    }

    int copyNextUploadBatch(int sourceIndex, qint64 totalBytes, qint64 &copiedBytes) const
    {
        const OperationQueue::Request &request = m_context.request;
        if (request.type != OperationQueue::Type::Copy
            || !request.explicitDestinations.isEmpty()) {
            return 0;
        }
        return m_incrementalUploadBatch(
            request.sources, sourceIndex, request.destination, totalBytes, copiedBytes);
    }

private:
    const ExecutionContext &m_context;
    WholeBatchTransfer m_uploadBatch;
    WholeBatchTransfer m_downloadBatch;
    WholeBatchTransfer m_stagedProviderBatch;
    IncrementalBatchTransfer m_incrementalUploadBatch;
};

class ResultAccumulator
{
public:
    ResultAccumulator(OperationQueue::OperationResult &result, int totalCount)
        : m_result(result)
        , m_totalCount(totalCount)
    {
    }

    void recordFailure(const QString &path, const QString &message, int count = 1)
    {
        m_result.failedCount += count;
        if (m_result.error.isEmpty()) {
            m_result.error = message;
            m_result.errorPath = path;
        }
        if (!path.isEmpty()) {
            m_result.failedPaths.append(path);
        }
    }

    void recordBatchFailure(int count, const QString &path, const QString &message)
    {
        m_result.failedCount = count;
        m_result.errorPath = path;
        m_result.error = message;
    }

    void addSuccess(int count = 1) { m_result.succeededCount += count; }
    void setSucceededCount(int count) { m_result.succeededCount = count; }
    void abort() { m_result.aborted = true; }

    void summarizePartialFailure()
    {
        if (m_result.failedCount <= 0 || m_totalCount <= 1) {
            return;
        }
        m_result.error = m_result.error.trimmed().isEmpty()
            ? QStringLiteral("%1 of %2 items failed").arg(m_result.failedCount).arg(m_totalCount)
            : QStringLiteral("%1 of %2 items failed. First error: %3")
                  .arg(m_result.failedCount)
                  .arg(m_totalCount)
                  .arg(m_result.error);
    }

private:
    OperationQueue::OperationResult &m_result;
    int m_totalCount = 0;
};

inline QString normalizedPath(const FileProvider &provider, const QString &path)
{
    QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(provider.absolutePath(path)));
#ifdef Q_OS_WIN
    normalized = normalized.toLower();
#endif
    return normalized;
}

inline bool samePath(const FileProvider &provider, const QString &lhs, const QString &rhs)
{
    return normalizedPath(provider, lhs) == normalizedPath(provider, rhs);
}

inline bool isDescendantPath(const FileProvider &provider, const QString &path, const QString &ancestor)
{
    const QString normalizedAncestor = normalizedPath(provider, ancestor);
    const QString normalizedPathValue = normalizedPath(provider, path);

    if (normalizedAncestor.isEmpty() || normalizedPathValue.size() <= normalizedAncestor.size()
        || !normalizedPathValue.startsWith(normalizedAncestor)) {
        return false;
    }
    return normalizedAncestor.endsWith(QLatin1Char('/'))
        || normalizedPathValue.at(normalizedAncestor.size()) == QLatin1Char('/');
}

inline QString requireLinuxAdminSessionNonce()
{
    const QString nonce = LinuxAdminBroker::activeSessionNonce();
    if (nonce.isEmpty()) {
        throw std::runtime_error("Linux administrator mode is not active");
    }
    return nonce;
}

qint64 bufferSizeForStorageType(OperationQueue::DriveStorageType type);

inline QString operationItemLabel(OperationQueue::Type type, const QString &name)
{
    QString action;
    switch (type) {
    case OperationQueue::Type::Copy:
        action = QStringLiteral("Copying");
        break;
    case OperationQueue::Type::Duplicate:
        action = QStringLiteral("Cloning");
        break;
    case OperationQueue::Type::Move:
        action = QStringLiteral("Moving");
        break;
    case OperationQueue::Type::Delete:
        action = QStringLiteral("Deleting");
        break;
    case OperationQueue::Type::Extract:
        action = QStringLiteral("Extracting");
        break;
    case OperationQueue::Type::Compress:
        action = QStringLiteral("Compressing");
        break;
    case OperationQueue::Type::CreateFolder:
        action = QStringLiteral("Creating");
        break;
    }

    return name.trimmed().isEmpty()
        ? action + QStringLiteral("...")
        : QStringLiteral("%1: %2").arg(action, name);
}

} // namespace OperationQueuePrivate
