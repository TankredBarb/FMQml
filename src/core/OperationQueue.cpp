#include "OperationQueue.h"
#include "FileProviderFactory.h"
#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#include "FileError.h"

#include <QtConcurrent>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QDebug>
#include <QSet>
#include <QVector>

#include <algorithm>
#include <limits>
#include <stdexcept>

// Windows-specific implementation
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602 // Windows 8+
#endif
#include <windows.h>
#endif

namespace {

constexpr qint64 SmallFileLimit = 10 * 1024 * 1024; // 10MB
constexpr qint64 DirectArchiveExtractThreshold = 64 * 1024 * 1024; // 64MB
constexpr qint64 MetricsUpdateIntervalMs = 500;

QString normalizedPath(const FileProvider &provider, const QString &path)
{
    QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(provider.absolutePath(path)));
#ifdef Q_OS_WIN
    normalized = normalized.toLower();
#endif
    return normalized;
}

bool samePath(const FileProvider &provider, const QString &lhs, const QString &rhs)
{
    return normalizedPath(provider, lhs) == normalizedPath(provider, rhs);
}

bool isDescendantPath(const FileProvider &provider, const QString &path, const QString &ancestor)
{
    const QString normalizedAncestor = normalizedPath(provider, ancestor);
    const QString normalizedPathValue = normalizedPath(provider, path);

    if (normalizedAncestor.isEmpty() || normalizedPathValue.isEmpty()) {
        return false;
    }

    if (normalizedPathValue.size() <= normalizedAncestor.size()) {
        return false;
    }

    if (!normalizedPathValue.startsWith(normalizedAncestor)) {
        return false;
    }

    if (normalizedAncestor.endsWith(QLatin1Char('/'))) {
        return true;
    }

    return normalizedPathValue.at(normalizedAncestor.size()) == QLatin1Char('/');
}

QString archiveContainerKey(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return {};
    }
    const QString normalized = ArchiveSupport::normalizeArchivePath(path);
    const int pipe = normalized.lastIndexOf(QLatin1Char('|'));
    return pipe >= 0 ? normalized.left(pipe) : normalized;
}

struct CopyFrame
{
    QString sourcePath;
    QString destinationPath;
};

QString formatSize(qint64 bytes) {
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024 * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
}

QString formatTime(qint64 seconds) {
    if (seconds < 60) return QString::number(seconds) + "s";
    if (seconds < 3600) return QString("%1m %2s").arg(seconds / 60).arg(seconds % 60);
    return QString("%1h %2m").arg(seconds / 3600).arg((seconds % 3600) / 60);
}

QString operationName(OperationQueue::Type type)
{
    switch (type) {
    case OperationQueue::Type::Copy:
        return QStringLiteral("copy");
    case OperationQueue::Type::Move:
        return QStringLiteral("move");
    case OperationQueue::Type::Delete:
        return QStringLiteral("delete");
    case OperationQueue::Type::Extract:
        return QStringLiteral("extract");
    }
    return QStringLiteral("operation");
}

QString primaryErrorPath(const OperationQueue::Request &request)
{
    switch (request.type) {
    case OperationQueue::Type::Copy:
    case OperationQueue::Type::Move:
    case OperationQueue::Type::Extract:
        return request.destination.isEmpty() ? request.sources.value(0) : request.destination;
    case OperationQueue::Type::Delete:
        return request.sources.value(0);
    }
    return request.sources.value(0);
}

QString providerFailureReason(FileProvider *provider, const QString &fallback)
{
    if (!provider) {
        return fallback;
    }
    const QString detail = provider->lastErrorString().trimmed();
    return detail.isEmpty() ? fallback : detail;
}

QString partialFailureSummary(int failedCount, int totalCount, const QString &firstError)
{
    if (failedCount <= 0) {
        return {};
    }
    if (totalCount <= 1) {
        return firstError;
    }
    if (firstError.trimmed().isEmpty()) {
        return QStringLiteral("%1 of %2 items failed").arg(failedCount).arg(totalCount);
    }
    return QStringLiteral("%1 of %2 items failed. First error: %3")
        .arg(failedCount)
        .arg(totalCount)
        .arg(firstError);
}

QString summarizedFailedItems(const QStringList &paths, int failedCount)
{
    if (failedCount <= 0 || paths.isEmpty()) {
        return {};
    }

    QStringList names;
    names.reserve((std::min)(paths.size(), qsizetype{2}));
    for (const QString &path : paths) {
        const QString name = QFileInfo(path).fileName().trimmed();
        names.append(name.isEmpty() ? QDir::toNativeSeparators(path) : name);
        if (names.size() >= 2) {
            break;
        }
    }

    if (names.isEmpty()) {
        return {};
    }

    QString summary = names.join(QStringLiteral(", "));
    const int remaining = failedCount - names.size();
    if (remaining > 0) {
        summary += QStringLiteral(" and %1 more").arg(remaining);
    }
    return summary;
}
}

thread_local std::function<bool()> g_threadAbortChecker;
thread_local std::function<void(qint64)> g_threadProgressReporter;

bool OperationQueue::isCurrentThreadAborted()
{
    if (g_threadAbortChecker) {
        return g_threadAbortChecker();
    }
    return false;
}

void OperationQueue::setCurrentThreadAbortChecker(std::function<bool()> checker)
{
    g_threadAbortChecker = std::move(checker);
}

void OperationQueue::reportCurrentThreadProgressBytes(qint64 bytes)
{
    if (g_threadProgressReporter) {
        g_threadProgressReporter(bytes);
    }
}

void OperationQueue::setCurrentThreadProgressReporter(std::function<void(qint64)> reporter)
{
    g_threadProgressReporter = std::move(reporter);
}

OperationQueue::OperationQueue(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<OperationResult>::finished, this, &OperationQueue::finishCurrent);
}

FileProvider* OperationQueue::getProviderForPath(const QString &path) const
{
    QMutexLocker locker(&m_providerMutex);
    QString key;
    if (ArchiveSupport::isArchivePath(path)) {
        key = ArchiveSupport::archiveRootPathForPath(path);
    } else {
        key = QStringLiteral("local");
    }

    auto it = m_providerCache.find(key);
    if (it != m_providerCache.end()) {
        return it.value().get();
    }

    std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
    FileProvider* ptr = provider.get();
    m_providerCache.insert(key, std::move(provider));
    return ptr;
}

OperationQueue::~OperationQueue()
{
    m_abort = true;
    QMutexLocker locker(&m_mutex);
    m_condition.wakeAll();
    locker.unlock();

    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

//TODO: move!
OperationQueue::DriveStorageType OperationQueue::getDriveTypeByPath(const QString &filePath)
{
#if defined(Q_OS_WIN)
    QString root = QFileInfo(filePath).absoluteDir().rootPath();
    if (root.isEmpty()) {
        return DriveStorageType::Unknown;
    }

    std::wstring stdRoot = root.toStdWString();
    LPCWSTR driveRoot = stdRoot.c_str();

    UINT winDriveType = GetDriveTypeW(driveRoot);
    if (winDriveType == DRIVE_REMOVABLE) {
        return DriveStorageType::USB_Flash;
    }
    if (winDriveType != DRIVE_FIXED) {
        return DriveStorageType::Unknown;
    }

    QString volumePath = QString(R"(\\.\)") + root.left(2);
    HANDLE hDevice = CreateFileW(
        volumePath.toStdWString().c_str(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        return DriveStorageType::Unknown;
    }

    DriveStorageType detectedType = DriveStorageType::HDD;

    STORAGE_PROPERTY_QUERY query;
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;

    DEVICE_SEEK_PENALTY_DESCRIPTOR seekPenaltyDesc = {0};
    DWORD bytesReturned = 0;

    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query),
        &seekPenaltyDesc, sizeof(seekPenaltyDesc),
        &bytesReturned, NULL
    );

    if (result && !seekPenaltyDesc.IncursSeekPenalty) {
        detectedType = DriveStorageType::SATA_SSD;

        query.PropertyId = StorageAdapterProperty;
        query.QueryType = PropertyStandardQuery;

        STORAGE_ADAPTER_DESCRIPTOR adapterDesc = {0};
        result = DeviceIoControl(
            hDevice,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query, sizeof(query),
            &adapterDesc, sizeof(adapterDesc),
            &bytesReturned, NULL
        );

        if (result) {
            if (adapterDesc.BusType == BusTypeNvme) {
                detectedType = DriveStorageType::NVME_SSD;
            } else if (adapterDesc.BusType == BusTypeUsb) {
                detectedType = DriveStorageType::USB_Flash;
            }
        }
    }

    CloseHandle(hDevice);
    return detectedType;
#else
    Q_UNUSED(filePath);
    return DriveStorageType::Unknown;
#endif
}

qint64 getBufferSizeByStorageType(OperationQueue::DriveStorageType type)
{
    switch (type) {
        case OperationQueue::DriveStorageType::HDD:
        case OperationQueue::DriveStorageType::USB_Flash:
            return 512 * 1024; // 512 КБ

        case OperationQueue::DriveStorageType::SATA_SSD:
            return 4 * 1024 * 1024; // 4 МБ

        case OperationQueue::DriveStorageType::NVME_SSD:
            return 8 * 1024 * 1024; // 8 МБ

        case OperationQueue::DriveStorageType::Unknown:
        default:
            return 1 * 1024 * 1024; // fallback
    }
}

bool OperationQueue::busy() const
{
    return m_busy;
}

double OperationQueue::progress() const
{
    return m_progress;
}

QString OperationQueue::currentLabel() const
{
    return m_currentLabel;
}

QString OperationQueue::error() const
{
    return m_error;
}

QVariantMap OperationQueue::lastError() const
{
    return m_lastError;
}

QString OperationQueue::statusMessage() const
{
    return m_statusMessage;
}

QString OperationQueue::speedText() const
{
    return m_speedText;
}

QString OperationQueue::remainingTimeText() const
{
    return m_remainingTimeText;
}

void OperationQueue::copyTo(const QStringList &sources, const QString &destination)
{
    if (sources.isEmpty() || destination.isEmpty()) {
        return;
    }
    if (ArchiveSupport::isArchivePath(destination)) {
        setStatusMessage(QStringLiteral("Archive contents are read-only"));
        return;
    }
    enqueue({Type::Copy, sources, destination});
}

void OperationQueue::moveTo(const QStringList &sources, const QString &destination)
{
    if (sources.isEmpty() || destination.isEmpty()) {
        return;
    }
    if (ArchiveSupport::isArchivePath(destination)) {
        setStatusMessage(QStringLiteral("Archive contents are read-only"));
        return;
    }
    for (const QString &source : sources) {
        if (ArchiveSupport::isArchivePath(source)) {
            setStatusMessage(QStringLiteral("Archive contents are read-only"));
            return;
        }
    }
    enqueue({Type::Move, sources, destination});
}

void OperationQueue::extractTo(const QStringList &sources, const QString &destination)
{
    if (sources.isEmpty() || destination.isEmpty()) {
        return;
    }

    QStringList normalizedSources;
    normalizedSources.reserve(sources.size());
    for (const QString &source : sources) {
        if (ArchiveSupport::archiveBackendAvailable() && ArchiveSupport::isArchiveFilePath(source)) {
            normalizedSources.append(ArchiveSupport::archiveRootPathForPath(source));
        } else {
            normalizedSources.append(source);
        }
    }

    enqueue({Type::Extract, normalizedSources, destination});
}

void OperationQueue::deletePaths(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return;
    }
    for (const QString &path : paths) {
        if (ArchiveSupport::isArchivePath(path)) {
            setStatusMessage(QStringLiteral("Archive contents are read-only"));
            return;
        }
    }
    enqueue({Type::Delete, paths, {}});
}

void OperationQueue::resolveConflict(ConflictResolution resolution, bool applyToAll)
{
    QMutexLocker locker(&m_mutex);
    m_resolution = resolution;
    m_applyToAll = applyToAll;
    m_lastResolution = resolution;
    m_condition.wakeAll();
}

void OperationQueue::cancel()
{
    m_abort = true;
    QMutexLocker locker(&m_mutex);
    m_condition.wakeAll();
}

void OperationQueue::clearError()
{
    setError({});
    setLastError({});
    if (!m_busy) {
        setCurrentLabel({});
    }
}

void OperationQueue::retryLastOperation()
{
    if (m_busy || !m_hasLastRequest) {
        return;
    }
    clearError();
    enqueue(m_lastRequest);
}

OperationQueue::ConflictResolution OperationQueue::waitForResolution(const QString &source, const QString &destination)
{
    if (m_abort) {
        return ConflictResolution::Cancel;
    }

    if (m_applyToAll && m_lastResolution != ConflictResolution::Pending) {
        return m_lastResolution;
    }

    FileProvider* srcProvider = getProviderForPath(source);
    FileProvider* destProvider = getProviderForPath(destination);

    const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(source);
    const std::optional<FileEntry> destInfo = destProvider->entryInfo(destination);

    QMutexLocker locker(&m_mutex);
    m_resolution = ConflictResolution::Pending;
    emit conflictDetected(source, destination, 
                          sourceInfo ? sourceInfo->size : 0,
                          sourceInfo ? sourceInfo->modified : QDateTime(),
                          destInfo ? destInfo->size : 0,
                          destInfo ? destInfo->modified : QDateTime());
    while (m_resolution == ConflictResolution::Pending && !m_abort) {
        m_condition.wait(&m_mutex);
    }

    if (m_abort) {
        return ConflictResolution::Cancel;
    }

    return m_resolution;
}

void OperationQueue::enqueue(Request request)
{
    m_pending.append(std::move(request));
    if (!m_busy) {
        runNext();
    }
}

void OperationQueue::runNext()
{
    if (m_pending.isEmpty()) {
        return;
    }

    const Request request = m_pending.takeFirst();
    m_lastRequest = request;
    m_hasLastRequest = true;
    m_abort = false;
    setBusy(true);
    setProgress(0.0);
    setCompletedItems(0);
    setTotalItems(0);
    setError({});
    setStatusMessage({});
    m_speedText = QString();
    m_remainingTimeText = QString();
    m_lastBytes = 0;
    m_lastTime = 0;
    m_currentSpeed = 0.0;
    m_applyToAll = false;
    m_lastResolution = ConflictResolution::Pending;
    
    QString label;
    switch (request.type) {
    case Type::Copy: label = QStringLiteral("Starting..."); break;
    case Type::Move: label = QStringLiteral("Moving..."); break;
    case Type::Delete: label = QStringLiteral("Deleting..."); break;
    case Type::Extract: label = QStringLiteral("Extracting..."); break;
    }
    setCurrentLabel(label);

    m_operationTimer.start();
    m_watcher.setFuture(QtConcurrent::run([this, request]() {
        return execute(request);
    }));
}

void OperationQueue::finishCurrent()
{
    const OperationResult result = m_watcher.future().result();
    const Request request = result.request;
    if (!result.error.isEmpty()) {
        if (!result.aborted) {
            setProgress(1.0);
        }
        setError(result.error);
        const QString errorPath = result.errorPath.isEmpty() ? primaryErrorPath(request) : result.errorPath;
        QVariantMap errorInfo = FileError::classify(result.error, errorPath, operationName(request.type));
        const QString itemSummary = summarizedFailedItems(result.failedPaths, result.failedCount);
        if (!itemSummary.isEmpty()) {
            errorInfo.insert(QStringLiteral("itemSummary"), itemSummary);
            errorInfo.insert(QStringLiteral("itemCount"), result.failedCount);
        }
        setLastError(errorInfo);
        setCurrentLabel(result.failedCount > 0 && result.succeededCount > 0
                            ? QStringLiteral("Completed with errors")
                            : QStringLiteral("Operation failed"));
    } else if (result.aborted) {
        setCurrentLabel(QStringLiteral("Cancelled"));
    } else {
        setProgress(1.0);
        setCurrentLabel(QStringLiteral("Done"));
    }
    setBusy(false);
    m_speedText = QString();
    m_remainingTimeText = QString();
    emit speedChanged();
    emit operationFinished(request.type, request.sources, request.destination);
    runNext();
}

void OperationQueue::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void OperationQueue::setProgress(double progress)
{
    const double bounded = std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_progress, bounded)) {
        return;
    }
    m_progress = bounded;
    emit progressChanged();
}

void OperationQueue::setCurrentLabel(const QString &label)
{
    if (m_currentLabel == label) {
        return;
    }
    m_currentLabel = label;
    emit currentLabelChanged();
}

void OperationQueue::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    if (m_error.isEmpty()) {
        setLastError({});
    }
    emit errorChanged();
}

void OperationQueue::setLastError(const QVariantMap &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

void OperationQueue::setStatusMessage(const QString &msg)
{
    m_statusMessage = msg;
    emit statusMessageChanged();
}

int OperationQueue::completedItems() const
{
    return m_completedItems;
}

int OperationQueue::totalItems() const
{
    return m_totalItems;
}

void OperationQueue::setCompletedItems(int completed)
{
    if (m_completedItems == completed) return;
    m_completedItems = completed;
    emit progressChanged();
}

void OperationQueue::setTotalItems(int total)
{
    if (m_totalItems == total) return;
    m_totalItems = total;
    emit progressChanged();
}

void OperationQueue::updateMetrics(qint64 currentBytes, qint64 totalBytes)
{
    const qint64 currentTime = m_operationTimer.elapsed();
    if (currentTime - m_lastTime < MetricsUpdateIntervalMs) return;

    const qint64 bytesSinceLast = currentBytes - m_lastBytes;
    const qint64 timeSinceLast = currentTime - m_lastTime;
    
    if (timeSinceLast > 0) {
        const double instantSpeed = (static_cast<double>(bytesSinceLast) / timeSinceLast) * 1000.0;
        
        if (m_currentSpeed <= 0) {
            m_currentSpeed = instantSpeed;
        } else {
            const double alpha = 0.25;
            m_currentSpeed = (alpha * instantSpeed) + (1.0 - alpha) * m_currentSpeed;
        }

        const QString speedTxt = formatSize(static_cast<qint64>(m_currentSpeed)) + "/s";
        
        const qint64 remainingBytes = totalBytes - currentBytes;
        QString remainingTxt;
        if (m_currentSpeed > 1024 && remainingBytes > 0) { 
            const qint64 remainingSec = static_cast<qint64>(remainingBytes / m_currentSpeed);
            remainingTxt = formatTime(remainingSec) + " remaining";
        }

        QMetaObject::invokeMethod(this, [this, speedTxt, remainingTxt]() {
            m_speedText = speedTxt;
            m_remainingTimeText = remainingTxt;
            emit speedChanged();
        }, Qt::QueuedConnection);
    }

    m_lastBytes = currentBytes;
    m_lastTime = currentTime;
}

OperationQueue::OperationResult OperationQueue::execute(const Request &request)
{
    OperationResult result;
    result.request = request;

    setCurrentThreadAbortChecker([this]() {
        return m_abort.load();
    });

    struct CacheCleaner {
        QHash<QString, std::shared_ptr<FileProvider>> &cache;
        ~CacheCleaner() {
            cache.clear();
            OperationQueue::setCurrentThreadAbortChecker(nullptr);
            OperationQueue::setCurrentThreadProgressReporter(nullptr);
            ArchiveFileProvider::setCurrentThreadTemporaryParent({});
        }
    } cleaner{m_providerCache};

    if (!request.destination.isEmpty()) {
        FileProvider *destProvider = getProviderForPath(request.destination);
        if (destProvider && destProvider->scheme() == QLatin1String("file")) {
            ArchiveFileProvider::setCurrentThreadTemporaryParent(request.destination);
        }
    }

    qint64 currentProgressBytes = 0;
    const int totalFileCount = request.sources.size();
    const bool isCountingItems = (request.type == Type::Delete);
    const qint64 totalBytes = isCountingItems
        ? static_cast<qint64>(totalFileCount)
        : std::max<qint64>(1, request.type == Type::Extract
            ? totalBytesForExtraction(request.sources)
            : totalBytesFor(request.sources));

    QMetaObject::invokeMethod(this, [this, totalFileCount]() {
        setTotalItems(totalFileCount);
        setCompletedItems(0);
    }, Qt::QueuedConnection);

    auto recordFailure = [&result, totalFileCount](const QString &path, const QString &message) {
        Q_UNUSED(totalFileCount)
        ++result.failedCount;
        if (result.error.isEmpty()) {
            result.error = message;
            result.errorPath = path;
        }
        if (!path.isEmpty()) {
            result.failedPaths.append(path);
        }
    };

    if (request.type == Type::Copy || request.type == Type::Move) {
        for (const QString &source : request.sources) {
            FileProvider* srcProvider = getProviderForPath(source);
            if (!isRealDirectory(source)) {
                continue;
            }

            const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(source);
            const QString sourceName = sourceInfo ? sourceInfo->name : srcProvider->fileName(source);
            FileProvider* destProvider = getProviderForPath(request.destination);
            const QString destinationPath = request.destination.isEmpty()
                ? QString()
                : destProvider->childPath(request.destination, sourceName);

            if (srcProvider == destProvider && isDescendantPath(*srcProvider, destinationPath, source)) {
                const QString message = QStringLiteral("Cannot %1 folder %2 into itself or one of its subfolders")
                    .arg(request.type == Type::Copy ? QStringLiteral("copy") : QStringLiteral("move"))
                    .arg(source);
                result.failedCount = totalFileCount;
                result.errorPath = source;
                result.error = message;
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
                    || ArchiveSupport::splitArchiveTokens(source).size() != 2
                    || ArchiveSupport::archiveBrowsePath(source) == QLatin1String("/")) {
                    canExtractArchiveSelection = false;
                    break;
                }

                FileProvider *srcProvider = getProviderForPath(source);
                const auto info = srcProvider->entryInfo(source);
                if (!info) {
                    canExtractArchiveSelection = false;
                    break;
                }

                QString finalPath = destProvider->childPath(request.destination, info->name);
                if (pathExists(finalPath)) {
                    ConflictResolution res = waitForResolution(source, finalPath);
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
                            QMetaObject::invokeMethod(this, [this]() {
                                setStatusMessage("Cannot replace the source archive. The item has been renamed.");
                            }, Qt::QueuedConnection);
                        } else if (!removePathIfExists(finalPath)) {
                            result.failedCount = totalFileCount;
                            result.errorPath = finalPath;
                            result.error = QStringLiteral("Cannot replace %1").arg(finalPath);
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
            QMetaObject::invokeMethod(this, [this]() {
                setProgress(1.0);
            }, Qt::QueuedConnection);
            return result;
        }

        if (canExtractArchiveSelection) {
            QString error;
            const qint64 baseBytes = currentProgressBytes;
            const qint64 remainingBytes = (std::max<qint64>)(1, totalBytes - baseBytes);
            const bool extracted = ArchiveFileProvider::extractArchiveItemsTo(
                archiveSources,
                finalPaths,
                &error,
                [this, baseBytes, remainingBytes, totalBytes](uint64_t processed) -> bool {
                    if (m_abort) {
                        return false;
                    }
                    const uint64_t maxBytes = static_cast<uint64_t>((std::numeric_limits<qint64>::max)());
                    const qint64 clampedBytes = std::clamp<qint64>(
                        static_cast<qint64>((std::min)(processed, maxBytes)),
                        0,
                        remainingBytes);
                    const qint64 progressBytes = baseBytes + clampedBytes;
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(progressBytes, totalBytes);
                    return true;
                });

            if (!extracted) {
                if (m_abort) {
                    result.aborted = true;
                    return result;
                }
                if (!error.contains(QStringLiteral("7-Zip"), Qt::CaseInsensitive)
                    && !error.contains(QStringLiteral("cached"), Qt::CaseInsensitive)) {
                    result.failedCount = archiveSources.size();
                    result.errorPath = archiveSources.value(0);
                    result.error = error.isEmpty()
                        ? QStringLiteral("Cannot extract selected archive items")
                        : error;
                    return result;
                }
            } else {
                currentProgressBytes = totalBytes;
                QMetaObject::invokeMethod(this, [this, totalFileCount]() {
                    setCompletedItems(totalFileCount);
                    setProgress(1.0);
                }, Qt::QueuedConnection);
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
                    ConflictResolution res = waitForResolution(source, finalPath);
                    if (res == ConflictResolution::Skip) {
                        continue;
                    }
                    if (res == ConflictResolution::KeepBoth) {
                        finalPath = uniqueDestinationPath(finalPath);
                    } else if (res == ConflictResolution::Replace) {
                        if (!removePathIfExists(finalPath)) {
                            result.failedCount = totalFileCount;
                            result.errorPath = finalPath;
                            result.error = QStringLiteral("Cannot replace %1").arg(finalPath);
                            return result;
                        }
                    } else if (res == ConflictResolution::Cancel) {
                        result.aborted = true;
                        return result;
                    }
                }

                const QString tempPath = finalPath + QStringLiteral(".part");
                if (pathExists(tempPath) && !removePathIfExists(tempPath)) {
                    result.failedCount = totalFileCount;
                    result.errorPath = tempPath;
                    result.error = QStringLiteral("Cannot replace temporary file %1").arg(tempPath);
                    return result;
                }

                batchSources.append(source);
                batchFinalPaths.append(finalPath);
                batchTempPaths.append(tempPath);
            }
        }

        if (canBatchArchiveFiles && batchSources.isEmpty()) {
            QMetaObject::invokeMethod(this, [this]() {
                setProgress(1.0);
            }, Qt::QueuedConnection);
            return result;
        }

        if (canBatchArchiveFiles) {
            QString error;
            const bool extracted = ArchiveFileProvider::extractArchiveEntriesTo(
                batchSources,
                batchTempPaths,
                &error,
                [this, totalBytes](uint64_t processed) -> bool {
                    if (m_abort) {
                        return false;
                    }
                    if (totalBytes > 0) {
                        const uint64_t maxBytes = static_cast<uint64_t>((std::numeric_limits<qint64>::max)());
                        const qint64 progressBytes = static_cast<qint64>((std::min)(processed, maxBytes));
                        const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                        QMetaObject::invokeMethod(this, [this, progress]() {
                            setProgress(progress);
                        }, Qt::QueuedConnection);
                        updateMetrics(progressBytes, totalBytes);
                    }
                    return true;
                });

            if (!extracted) {
                for (const QString &tempPath : std::as_const(batchTempPaths)) {
                    removePathIfExists(tempPath);
                }
                if (m_abort) {
                    result.aborted = true;
                    return result;
                }
                result.failedCount = batchSources.size();
                result.errorPath = batchSources.value(0);
                result.error = error.isEmpty() ? QStringLiteral("Cannot extract selected archive entries") : error;
                return result;
            }

            for (int i = 0; i < batchTempPaths.size(); ++i) {
                if (m_abort) {
                    for (const QString &tempPath : std::as_const(batchTempPaths)) {
                        removePathIfExists(tempPath);
                    }
                    result.aborted = true;
                    return result;
                }
                if (pathExists(batchFinalPaths.at(i)) && !removePathIfExists(batchFinalPaths.at(i))) {
                    removePathIfExists(batchTempPaths.at(i));
                    result.failedCount = batchSources.size();
                    result.errorPath = batchFinalPaths.at(i);
                    result.error = QStringLiteral("Cannot replace %1").arg(batchFinalPaths.at(i));
                    return result;
                }
                if (!destProvider->movePath(batchTempPaths.at(i), batchFinalPaths.at(i))) {
                    removePathIfExists(batchTempPaths.at(i));
                    result.failedCount = batchSources.size();
                    result.errorPath = batchFinalPaths.at(i);
                    result.error = QStringLiteral("Cannot finalize %1").arg(batchFinalPaths.at(i));
                    return result;
                }
            }

            currentProgressBytes = totalBytes;
            QMetaObject::invokeMethod(this, [this]() {
                setProgress(1.0);
            }, Qt::QueuedConnection);
            return result;
        }
    }

    for (int i = 0; i < totalFileCount; ++i) {
        if (m_abort) {
            result.aborted = true;
            return result;
        }
        const QString &source = request.sources.at(i);
        FileProvider* srcProvider = getProviderForPath(source);
        const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(source);
        const QString sourceName = sourceInfo ? sourceInfo->name : srcProvider->fileName(source);
        FileProvider* destProvider = getProviderForPath(request.destination);
        const QString destinationPath = request.destination.isEmpty() ? QString() : destProvider->childPath(request.destination, sourceName);
        const int failureCountBefore = result.failedCount;

        try {
            if (request.type == Type::Copy) {
                copyPath(source, destinationPath, totalBytes, currentProgressBytes);
            } else if (request.type == Type::Extract) {
                extractArchiveContents(source, request.destination, totalBytes, currentProgressBytes);
            } else if (request.type == Type::Move) {
                movePath(source, destinationPath, totalBytes, currentProgressBytes);
            } else if (request.type == Type::Delete) {
                QMetaObject::invokeMethod(this, [this, name = sourceName, i]() {
                    setCurrentLabel(name);
                }, Qt::QueuedConnection);

                if (isRealDirectory(source)) {
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
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
            }
        } catch (const std::exception &exception) {
            recordFailure(source, QString::fromUtf8(exception.what()));
        }

        QMetaObject::invokeMethod(this, [this, i]() {
            setCompletedItems(i + 1);
        }, Qt::QueuedConnection);

        if (result.failedCount == failureCountBefore) {
            ++result.succeededCount;
        }
    }

    if (m_abort) {
        result.aborted = true;
    } else if (result.failedCount > 0) {
        result.error = partialFailureSummary(result.failedCount, totalFileCount, result.error);
    }
    return result;
}

void OperationQueue::extractArchiveContents(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes)
{
    if (m_abort) return;

    FileProvider* srcProvider = getProviderForPath(sourcePath);
    FileProvider* destProvider = getProviderForPath(destinationPath);

    if (!ArchiveSupport::isArchivePath(sourcePath)) {
        const QString fallbackDestination = destProvider->childPath(destinationPath, srcProvider->fileName(sourcePath));
        copyPath(sourcePath, fallbackDestination, totalBytes, copiedBytes);
        return;
    }

    const QString physicalArchivePath = ArchiveSupport::physicalArchivePath(sourcePath);
    if (ArchiveSupport::archiveBrowsePath(sourcePath) == QLatin1String("/") && ArchiveSupport::isArchiveFilePath(physicalArchivePath)) {
        QString error;
        std::atomic<qint64> extractedEntries{0};
        std::atomic<qint64> lastProgressEntry{0};
        if (m_abort) {
            return;
        }
        const bool extracted = ArchiveFileProvider::extractArchiveFileTo(
            physicalArchivePath,
            destinationPath,
            &error,
            [this, totalBytes](uint64_t processed) -> bool {
                if (m_abort) {
                    return false;
                }
                if (totalBytes > 0 && processed <= static_cast<uint64_t>(totalBytes)) {
                    const double progress = static_cast<double>(processed) / static_cast<double>(totalBytes);
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(static_cast<qint64>(processed), totalBytes);
                }
                return true;
            },
            [this, &extractedEntries, &lastProgressEntry](const QString &filePath) {
                const qint64 current = extractedEntries.fetch_add(1) + 1;
                const qint64 minStep = 200;
                const qint64 previous = lastProgressEntry.load();
                if (current - previous < minStep) {
                    return;
                }
                lastProgressEntry.store(current);
                const double progress = (std::min)(0.95, static_cast<double>(current) / static_cast<double>(current + 2000));
                QMetaObject::invokeMethod(this, [this, progress, current, fileName = QFileInfo(filePath).fileName()]() {
                    if (!fileName.isEmpty()) {
                        setCurrentLabel(fileName);
                    }
                    setCompletedItems(static_cast<int>((std::min<qint64>)(current, (std::numeric_limits<int>::max)())));
                    if (m_totalItems < m_completedItems) {
                        setTotalItems(m_completedItems);
                    }
                    setProgress(progress);
                }, Qt::QueuedConnection);
            });

        if (!extracted) {
            throw std::runtime_error(error.isEmpty()
                ? QStringLiteral("Cannot extract archive %1").arg(physicalArchivePath).toStdString()
                : error.toStdString());
        }

        const qint64 finalEntryCount = extractedEntries.load();
        if (finalEntryCount > 0) {
            QMetaObject::invokeMethod(this, [this, finalEntryCount]() {
                const int boundedCount = static_cast<int>((std::min<qint64>)(finalEntryCount, (std::numeric_limits<int>::max)()));
                setCompletedItems(boundedCount);
                setTotalItems(boundedCount);
            }, Qt::QueuedConnection);
        }
        copiedBytes = (std::max)(copiedBytes, totalBytes);
        return;
    }

    if (!makePath(destinationPath)) {
        throw std::runtime_error(QStringLiteral("Cannot create folder %1").arg(destinationPath).toStdString());
    }

    const QStringList children = childPaths(sourcePath);
    for (const QString &child : children) {
        if (m_abort) return;

        const std::optional<FileEntry> childInfo = srcProvider->entryInfo(child);
        const QString childName = childInfo ? childInfo->name : srcProvider->fileName(child);
        const QString childDestination = destProvider->childPath(destinationPath, childName);
        copyPath(child, childDestination, totalBytes, copiedBytes);
    }
}

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

qint64 OperationQueue::totalBytesForExtraction(const QStringList &sources) const
{
    qint64 total = 0;
    for (const QString &source : sources) {
        if (m_abort) {
            break;
        }

        if (ArchiveSupport::isArchivePath(source)
            && ArchiveSupport::archiveBrowsePath(source) == QLatin1String("/")) {
            const QString physicalPath = ArchiveSupport::physicalArchivePath(source);
            if (ArchiveSupport::isArchiveFilePath(physicalPath)) {
                total += (std::max<qint64>)(1, QFileInfo(physicalPath).size());
                continue;
            }
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
            total += info->size;
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

qint64 OperationQueue::totalEntryCountForPath(const QString &path) const
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

        FileProvider *provider = getProviderForPath(currentPath);
        const std::optional<FileEntry> info = provider->entryInfo(currentPath);
        if (!info) {
            continue;
        }

        if (!info->isDirectory || provider->isSymLink(currentPath)) {
            ++total;
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

void OperationQueue::copyPath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes)
{
    if (m_abort) return;

    QVector<CopyFrame> stack;
    stack.push_back({sourcePath, destinationPath});

    while (!stack.isEmpty()) {
        if (m_abort) return;

        const CopyFrame frame = stack.back();
        stack.pop_back();

        FileProvider* srcProvider = getProviderForPath(frame.sourcePath);
        FileProvider* destProvider = getProviderForPath(frame.destinationPath);

        const std::optional<FileEntry> sourceInfo = srcProvider->entryInfo(frame.sourcePath);
        const QString fileName = sourceInfo ? sourceInfo->name : srcProvider->fileName(frame.sourcePath);

        QMetaObject::invokeMethod(this, [this, fileName]() {
            setCurrentLabel(fileName);
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
                const QString childDestination = destProvider->childPath(targetPath, srcProvider->fileName(*it));
                stack.push_back({*it, childDestination});
            }
            continue;
        }

        if (!ensureParentDirectory(targetPath)) {
            throw std::runtime_error(providerFailureReason(
                destProvider,
                QStringLiteral("Cannot create parent directory for %1").arg(targetPath)).toStdString());
        }
        const QString tempPath = targetPath + QStringLiteral(".part");
        if (pathExists(tempPath) && !removePathIfExists(tempPath)) {
            throw std::runtime_error(providerFailureReason(
                destProvider,
                QStringLiteral("Cannot replace temporary file %1").arg(tempPath)).toStdString());
        }

        const qint64 fileSize = sourceInfo ? sourceInfo->size : 0;
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
            continue;
        }

        std::unique_ptr<QIODevice> source = srcProvider->openRead(frame.sourcePath);
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
            qint64 bufferSize = getBufferSizeByStorageType(getDriveTypeByPath(targetPath));

            buffer.resize(static_cast<int>(bufferSize));

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

                const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
                updateMetrics(copiedBytes, totalBytes);
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
    }
}

void OperationQueue::movePath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes)
{
    if (m_abort) return;

    FileProvider* srcProvider = getProviderForPath(sourcePath);
    FileProvider* destProvider = getProviderForPath(destinationPath);

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

    copyPath(sourcePath, targetPath, totalBytes, copiedBytes);

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
