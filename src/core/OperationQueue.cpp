#include "OperationQueue.h"
#include "OperationQueuePrivate.h"
#include "FileProviderFactory.h"
#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#include "FileError.h"
#include "CleanupSubsystem.h"
#include "LinuxAdminPolicy.h"

#include <QtConcurrent>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QFileInfo>
#include <QMetaObject>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStorageInfo>
#include <QScopeGuard>
#include <QUuid>
#include <QThread>
#include <QVector>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

using OperationQueuePrivate::isDescendantPath;
using OperationQueuePrivate::normalizedPath;
using OperationQueuePrivate::samePath;

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

// Windows-specific implementation
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602 // Windows 8+
#endif
#include <windows.h>
#include <winioctl.h>
#endif

namespace OperationQueuePrivate {

bool providerBatchLoggingEnabled()
{
    return qEnvironmentVariableIntValue("FMQML_PROVIDER_BATCH_LOG") > 0;
}

bool providerTransferTimingEnabled()
{
    return qEnvironmentVariableIntValue("FMQML_PROVIDER_TRANSFER_TIMING") > 0;
}

bool providerMaterializeLoggingEnabled()
{
    return qEnvironmentVariableIntValue("FMQML_PROVIDER_MATERIALIZE_LOG") > 0;
}

double mibPerSecond(qint64 bytes, qint64 elapsedMs)
{
    if (bytes <= 0 || elapsedMs <= 0) {
        return 0.0;
    }
    return (static_cast<double>(bytes) / 1024.0 / 1024.0) / (static_cast<double>(elapsedMs) / 1000.0);
}

QString pathLogName(const QString &path)
{
    const QString name = QFileInfo(path).fileName();
    return name.isEmpty() ? path.left(96) : name;
}

QString providerBatchLabel(QLatin1StringView phase, int waveIndex, int waveCount)
{
    if (waveIndex > 0) {
        if (waveCount > 0) {
            return QStringLiteral("%1 batch %2/%3").arg(QString(phase)).arg(waveIndex).arg(waveCount);
        }
        return QStringLiteral("%1 batch %2").arg(QString(phase)).arg(waveIndex);
    }
    return QStringLiteral("%1 batch").arg(QString(phase));
}

QString operationFolderLabel(QLatin1StringView action, const QString &path)
{
    QString name = QFileInfo(path).fileName();
    if (name.isEmpty()) {
        const QString cleaned = QDir::cleanPath(path);
        const int separatorIndex = cleaned.lastIndexOf(QLatin1Char('/'));
        name = separatorIndex >= 0 ? cleaned.mid(separatorIndex + 1) : cleaned;
    }
    name = name.trimmed();
    return name.isEmpty()
        ? QStringLiteral("%1 upload folder...").arg(QString(action))
        : QStringLiteral("%1 upload folder: %2").arg(QString(action), name);
}

int providerStagedWaveCount(const QVector<CopyFrame> &batchFiles)
{
    qsizetype index = 0;
    int waveCount = 0;
    while (index < batchFiles.size()) {
        qsizetype waveFiles = 0;
        qint64 waveBytes = 0;
        while (index < batchFiles.size() && waveFiles < ProviderStagedBatchMaxFiles) {
            const qint64 fileSize = (std::max<qint64>)(0, batchFiles.at(index).size);
            if (waveFiles > 0 && waveBytes + fileSize > ProviderStagedBatchMaxBytes) {
                break;
            }
            waveBytes += fileSize;
            ++waveFiles;
            ++index;
        }
        if (waveFiles == 0) {
            break;
        }
        ++waveCount;
    }
    return waveCount;
}

qint64 keepExistingLocalUploadItems(QVector<LocalFileCopyItem> &items)
{
    qint64 bytes = 0;
    qsizetype writeIndex = 0;
    for (qsizetype readIndex = 0; readIndex < items.size(); ++readIndex) {
        const LocalFileCopyItem &item = items.at(readIndex);
        if (!QFileInfo::exists(item.sourceFilePath)) {
            continue;
        }
        if (writeIndex != readIndex) {
            items[writeIndex] = item;
        }
        bytes += (std::max<qint64>)(0, item.size);
        ++writeIndex;
    }
    items.resize(writeIndex);
    return bytes;
}

QString archiveContainerKey(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return {};
    }
    const QString normalized = ArchiveSupport::normalizeArchivePath(path);
    const QStringList tokens = ArchiveSupport::splitArchiveTokens(normalized);
    if (tokens.isEmpty()) {
        return {};
    }

    const int containerTokenCount = qMax(1, tokens.size() - 1);
    QStringList parts;
    parts.reserve(containerTokenCount);
    parts.append(QDir::fromNativeSeparators(QFileInfo(tokens.first()).absoluteFilePath()));
    for (int i = 1; i < containerTokenCount; ++i) {
        QString segment = QDir::fromNativeSeparators(tokens.at(i).trimmed());
        if (segment == QLatin1String("/")) {
            segment.clear();
        }
        if (segment.startsWith(QLatin1Char('/'))) {
            segment.remove(0, 1);
        }
        while (segment.endsWith(QLatin1Char('/'))) {
            segment.chop(1);
        }
        parts.append(segment);
    }
    return QStringLiteral("archive://") + parts.join(QLatin1Char('|'));
}

QString explicitProviderScheme(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return {};
    }

    static const QRegularExpression schemePattern(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*$"));
    const QString scheme = trimmed.left(separatorIndex).toLower();
    if (!schemePattern.match(scheme).hasMatch()) {
        return {};
    }
    return scheme;
}

bool quotaManagedRemotePath(const QString &path)
{
    const QString scheme = explicitProviderScheme(path);
    return scheme == QLatin1String("mega")
        || scheme == QLatin1String("gdrive")
        || scheme == QLatin1String("ftp");
}

bool requestUsesQuotaManagedRemoteProvider(const OperationQueue::Request &request)
{
    if (quotaManagedRemotePath(request.destination)) {
        return true;
    }
    for (const QString &source : request.sources) {
        if (quotaManagedRemotePath(source)) {
            return true;
        }
    }
    return false;
}

QString providerCacheKeyForPath(const QString &path)
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveRootPathForPath(path);
    }

    if (FileProviderFactory::hasPluginProviderForPath(path)) {
        const QString scheme = explicitProviderScheme(path);
        if (!scheme.isEmpty()) {
            return QStringLiteral("plugin:%1").arg(scheme);
        }
    }

    return QStringLiteral("local");
}

QString allocateProviderTransferFile(const QString &destinationPath,
                                     const QString &fileName,
                                     QString *leaseId)
{
    const QString stagingParent = StagingLocationPolicy::resolveStagingParent(
        destinationPath, {}, {}, true);
    if (stagingParent.isEmpty()) {
        return {};
    }

    const QString operationId = QStringLiteral("provider-transfer-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stagingDir = CleanupSubsystem::instance().allocateStagingDirectory(
        CleanupArtifactKind::ProviderTransfer,
        stagingParent,
        operationId,
        leaseId);
    if (stagingDir.isEmpty()) {
        return {};
    }

    QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix.size() > 16 || suffix.contains(QLatin1Char('/')) || suffix.contains(QLatin1Char('\\'))) {
        suffix.clear();
    }

    return QDir(stagingDir).filePath(
        QStringLiteral("transfer") + (suffix.isEmpty() ? QString{} : QLatin1Char('.') + suffix));
}

QString allocateNeutralProviderTransferFile(const QString &fileName, QString *leaseId)
{
    const QString stagingParent = StagingLocationPolicy::defaultCleanupRoot();
    if (stagingParent.isEmpty()) {
        return {};
    }

    const QString operationId = QStringLiteral("portable-transfer-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stagingDir = CleanupSubsystem::instance().allocateStagingDirectory(
        CleanupArtifactKind::ProviderTransfer,
        stagingParent,
        operationId,
        leaseId);
    if (stagingDir.isEmpty()) {
        return {};
    }

    QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix.size() > 16 || suffix.contains(QLatin1Char('/')) || suffix.contains(QLatin1Char('\\'))) {
        suffix.clear();
    }

    return QDir(stagingDir).filePath(
        QStringLiteral("transfer") + (suffix.isEmpty() ? QString{} : QLatin1Char('.') + suffix));
}

qint64 cheapArchiveSelectionBytes(const QStringList &sources)
{
    if (sources.isEmpty()) {
        return -1;
    }

    const QString firstContainer = archiveContainerKey(sources.constFirst());
    if (firstContainer.isEmpty()) {
        return -1;
    }

    qint64 total = 0;
    for (const QString &source : sources) {
        if (!ArchiveSupport::isArchivePath(source)
            || archiveContainerKey(source) != firstContainer
            || ArchiveSupport::archiveBrowsePath(source) == QLatin1String("/")) {
            return -1;
        }

        const auto entry = ArchiveFileProvider::entryInfoForPath(source);
        total += (std::max<qint64>)(1, entry ? entry->size : 1);
    }
    return (std::max<qint64>)(1, total);
}

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
    case OperationQueue::Type::Duplicate:
        return QStringLiteral("duplicate");
    case OperationQueue::Type::Move:
        return QStringLiteral("move");
    case OperationQueue::Type::Delete:
        return QStringLiteral("delete");
    case OperationQueue::Type::Extract:
        return QStringLiteral("extract");
    case OperationQueue::Type::Compress:
        return QStringLiteral("compress");
    case OperationQueue::Type::CreateFolder:
        return QStringLiteral("createFolder");
    }
    return QStringLiteral("operation");
}

QString primaryErrorPath(const OperationQueue::Request &request)
{
    switch (request.type) {
    case OperationQueue::Type::Copy:
    case OperationQueue::Type::Duplicate:
    case OperationQueue::Type::Move:
    case OperationQueue::Type::Extract:
    case OperationQueue::Type::Compress:
        return request.destination.isEmpty() ? request.sources.value(0) : request.destination;
    case OperationQueue::Type::Delete:
        return request.sources.value(0);
    case OperationQueue::Type::CreateFolder:
        return request.destination;
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

using OperationQueuePrivate::formatSize;
using OperationQueuePrivate::formatTime;
using OperationQueuePrivate::MetricsUpdateIntervalMs;
using OperationQueuePrivate::mibPerSecond;
using OperationQueuePrivate::operationName;
using OperationQueuePrivate::primaryErrorPath;
using OperationQueuePrivate::providerCacheKeyForPath;
using OperationQueuePrivate::providerTransferTimingEnabled;
using OperationQueuePrivate::requestUsesQuotaManagedRemoteProvider;
using OperationQueuePrivate::summarizedFailedItems;

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
    m_elapsedTimer.setInterval(1000);
    connect(&m_elapsedTimer, &QTimer::timeout, this, &OperationQueue::updateElapsedTimeText);
}

FileProvider* OperationQueue::getProviderForPath(const QString &path) const
{
    QMutexLocker locker(&m_providerMutex);
    const QString key = providerCacheKeyForPath(path);

    auto it = m_providerCache.find(key);
    if (it != m_providerCache.end()) {
        return it.value().get();
    }

    std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
    if (!provider) {
        provider = std::make_unique<LocalFileProvider>();
    }
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

QString OperationQueue::elapsedTimeText() const
{
    return m_elapsedTimeText;
}

bool OperationQueue::remoteQuotaNoticeVisible() const
{
    return m_remoteQuotaNoticeVisible;
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
    enqueue({Type::Copy, sources, destination, false, {}});
}

void OperationQueue::copyToExactDestinations(const QStringList &sources, const QStringList &destinations)
{
    if (sources.isEmpty() || sources.size() != destinations.size()) return;
    for (const QString &destination : destinations) {
        if (destination.isEmpty() || ArchiveSupport::isArchivePath(destination)) return;
    }
    Request request;
    request.type = Type::Copy;
    request.sources = sources;
    request.explicitDestinations = destinations;
    enqueue(std::move(request));
}

void OperationQueue::copyToAsAdministrator(const QStringList &sources, const QString &destination)
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
            setStatusMessage(QStringLiteral("Administrator copy is unavailable for archive contents"));
            return;
        }
    }
    enqueue({Type::Copy, sources, destination, true, {}});
}

void OperationQueue::createFolderAsAdministrator(const QString &destination, const QString &name)
{
    if (destination.isEmpty() || name.trimmed().isEmpty()) {
        return;
    }
    if (ArchiveSupport::isArchivePath(destination)) {
        setStatusMessage(QStringLiteral("Archive contents are read-only"));
        return;
    }
    enqueue({Type::CreateFolder, {name.trimmed()}, destination, true, {}});
}

void OperationQueue::duplicateInPlace(const QStringList &sources, const QString &destinationHint)
{
    if (sources.size() != 1) {
        return;
    }
    const QString source = sources.constFirst();
    if (ArchiveSupport::isArchivePath(source)) {
        setStatusMessage(QStringLiteral("Archive contents are read-only"));
        return;
    }
    if (!QFileInfo(source).isFile()) {
        setStatusMessage(QStringLiteral("Only files can be duplicated"));
        return;
    }
    enqueue({Type::Duplicate, sources, destinationHint, false, {}});
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
    enqueue({Type::Move, sources, destination, false, {}});
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

    enqueue({Type::Extract, normalizedSources, destination, false, {}});
}

void OperationQueue::compressToArchive(const QStringList &sources, const QString &archivePath)
{
    if (sources.isEmpty() || archivePath.isEmpty()) {
        return;
    }
    if (ArchiveSupport::sevenZipExecutablePath().isEmpty()) {
        setStatusMessage(QStringLiteral("7-Zip executable was not found"));
        return;
    }
    if (ArchiveSupport::isArchivePath(archivePath)) {
        setStatusMessage(QStringLiteral("Archive contents are read-only"));
        return;
    }
    for (const QString &source : sources) {
        if (ArchiveSupport::isArchivePath(source)) {
            setStatusMessage(QStringLiteral("Archive contents are read-only"));
            return;
        }
    }
    enqueue({Type::Compress, sources, archivePath, false, {}});
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
    enqueue({Type::Delete, paths, {}, false, {}});
}

void OperationQueue::deletePathsAsAdministrator(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return;
    }
    for (const QString &path : paths) {
        if (ArchiveSupport::isArchivePath(path)) {
            setStatusMessage(QStringLiteral("Administrator delete is unavailable for archive contents"));
            return;
        }
    }
    enqueue({Type::Delete, paths, {}, true, {}});
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
    LinuxAdminBroker::cancelActiveSessionOperation();
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

void OperationQueue::reportError(const QString &message,
                                 const QString &path,
                                 const QString &operation,
                                 bool retryable)
{
    if (message.trimmed().isEmpty()) {
        return;
    }

    QVariantMap errorInfo = FileError::classify(message, path, operation);
    if (!retryable) {
        QStringList actions = errorInfo.value(QStringLiteral("actions")).toStringList();
        actions.removeAll(QStringLiteral("retry"));
        errorInfo.insert(QStringLiteral("actions"), actions);
        errorInfo.insert(QStringLiteral("recoverable"), !actions.isEmpty());
    }

    setLastError(errorInfo);
    setError(message);
    setCurrentLabel(QStringLiteral("Operation failed"));
    setStatusMessage(message);
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
    setProgress(0.0);
    setCompletedItems(0);
    setTotalItems(0);
    setError({});
    setStatusMessage({});
    setRemoteQuotaNoticeVisible(requestUsesQuotaManagedRemoteProvider(request));
    m_speedText = QStringLiteral("0 B/s");
    m_remainingTimeText = QString();
    m_elapsedTimeText = QStringLiteral("Elapsed 0s");
    emit speedChanged();
    m_lastBytes = 0;
    m_lastTime = 0;
    m_currentSpeed = 0.0;
    m_applyToAll = false;
    m_lastResolution = ConflictResolution::Pending;
    
    QString label;
    switch (request.type) {
    case Type::Copy: label = QStringLiteral("Starting..."); break;
    case Type::Duplicate: label = QStringLiteral("Duplicating..."); break;
    case Type::Move: label = QStringLiteral("Moving..."); break;
    case Type::Delete: label = request.administrator
        ? QStringLiteral("Deleting as Administrator...")
        : QStringLiteral("Deleting..."); break;
    case Type::Extract: label = QStringLiteral("Extracting..."); break;
    case Type::Compress: label = QStringLiteral("Compressing..."); break;
    case Type::CreateFolder: label = request.administrator
        ? QStringLiteral("Creating as Administrator...")
        : QStringLiteral("Creating..."); break;
    }
    setCurrentLabel(label);
    setBusy(true);
    emit operationStarted(request.type, request.sources, request.destination);

    m_operationTimer.start();
    m_elapsedTimer.start();
    m_watcher.setFuture(QtConcurrent::run([this, request]() {
        try {
            return execute(request);
        } catch (const std::exception &exception) {
            OperationResult result;
            result.request = request;
            result.error = QString::fromUtf8(exception.what());
            result.errorPath = primaryErrorPath(request);
            result.failedCount = std::max(1, static_cast<int>(request.sources.size()));
            result.failedPaths = request.sources;
            result.aborted = m_abort.load();
            return result;
        } catch (...) {
            OperationResult result;
            result.request = request;
            result.error = QStringLiteral("Operation failed");
            result.errorPath = primaryErrorPath(request);
            result.failedCount = std::max(1, static_cast<int>(request.sources.size()));
            result.failedPaths = request.sources;
            result.aborted = m_abort.load();
            return result;
        }
    }));
}

void OperationQueue::finishCurrent()
{
    m_elapsedTimer.stop();
    updateElapsedTimeText();
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
    m_elapsedTimeText = QString();
    emit speedChanged();
    if (request.administrator && result.succeededCount > 0) {
        emit administratorOperationSucceeded();
    }
    emit operationFinishedDetailed(request.type, request.sources, request.destination,
                                   result.succeededCount, result.failedCount, result.failedPaths, result.aborted);
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

void OperationQueue::setRemoteQuotaNoticeVisible(bool visible)
{
    if (m_remoteQuotaNoticeVisible == visible) {
        return;
    }
    m_remoteQuotaNoticeVisible = visible;
    emit remoteQuotaNoticeVisibleChanged();
}

void OperationQueue::updateElapsedTimeText()
{
    if (!m_operationTimer.isValid()) {
        return;
    }

    const QString elapsedTxt = QStringLiteral("Elapsed %1").arg(formatTime(m_operationTimer.elapsed() / 1000));
    if (m_elapsedTimeText == elapsedTxt) {
        return;
    }
    m_elapsedTimeText = elapsedTxt;
    emit speedChanged();
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
        const QString elapsedTxt = QStringLiteral("Elapsed %1").arg(formatTime(currentTime / 1000));
        
        const qint64 remainingBytes = (std::max<qint64>)(0, totalBytes - currentBytes);
        QString remainingTxt;
        if (m_currentSpeed > 1024 && remainingBytes > 0) { 
            const qint64 remainingSec = static_cast<qint64>(remainingBytes / m_currentSpeed);
            remainingTxt = formatTime(remainingSec) + " estimated";
        }

        QMetaObject::invokeMethod(this, [this, speedTxt, remainingTxt, elapsedTxt]() {
            m_speedText = speedTxt;
            m_remainingTimeText = remainingTxt;
            m_elapsedTimeText = elapsedTxt;
            emit speedChanged();
        }, Qt::QueuedConnection);
    }

    m_lastBytes = currentBytes;
    m_lastTime = currentTime;
}

void OperationQueue::resetTransferMetricsBaseline()
{
    m_lastBytes = 0;
    m_lastTime = m_operationTimer.elapsed();
    m_currentSpeed = 0.0;
    QMetaObject::invokeMethod(this, [this]() {
        m_speedText = QStringLiteral("0 B/s");
        m_remainingTimeText.clear();
        emit speedChanged();
    }, Qt::QueuedConnection);
}

void OperationQueue::resetProviderTransferTiming(const Request &request)
{
    m_providerTransferTiming = {};
    if (!providerTransferTimingEnabled()) {
        return;
    }

    m_providerTransferTiming.active = true;
    m_providerTransferTiming.type = request.type;
    m_providerTransferTiming.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!request.destination.isEmpty()) {
        if (FileProvider *destProvider = getProviderForPath(request.destination)) {
            m_providerTransferTiming.destinationScheme = destProvider->scheme();
        }
    }
    m_providerTransferTiming.wallTimer.start();
}

void OperationQueue::logProviderTransferTimingSummary()
{
    if (!m_providerTransferTiming.active
        || m_providerTransferTiming.logged
        || m_providerTransferTiming.fileCount <= 0) {
        return;
    }

    m_providerTransferTiming.logged = true;
    const qint64 wallMs = m_providerTransferTiming.wallTimer.isValid()
        ? m_providerTransferTiming.wallTimer.elapsed()
        : 0;
    const QString result = m_abort.load()
        ? QStringLiteral("canceled")
        : (m_providerTransferTiming.failedFiles > 0 ? QStringLiteral("failed") : QStringLiteral("success"));

    qInfo().noquote()
        << "[ProviderTransferSummary]"
        << "operationId=" << m_providerTransferTiming.operationId
        << "result=" << result
        << "destinationScheme=" << m_providerTransferTiming.destinationScheme
        << "files=" << m_providerTransferTiming.fileCount
        << "success=" << m_providerTransferTiming.successfulFiles
        << "failed=" << m_providerTransferTiming.failedFiles
        << "canceled=" << m_providerTransferTiming.canceledFiles
        << "bytes=" << m_providerTransferTiming.totalBytes
        << "stagedBytes=" << m_providerTransferTiming.stagedBytes
        << "uploadedBytes=" << m_providerTransferTiming.uploadedBytes
        << "allocationMs=" << m_providerTransferTiming.allocationMs
        << "stagingMs=" << m_providerTransferTiming.stagingMs
        << "uploadMs=" << m_providerTransferTiming.uploadMs
        << "cleanupMs=" << m_providerTransferTiming.cleanupMs
        << "wallMs=" << wallMs
        << "effectiveMiBs=" << mibPerSecond(m_providerTransferTiming.totalBytes, wallMs)
        << "stagingMiBs=" << mibPerSecond(m_providerTransferTiming.stagedBytes, m_providerTransferTiming.stagingMs)
        << "uploadMiBs=" << mibPerSecond(m_providerTransferTiming.uploadedBytes, m_providerTransferTiming.uploadMs);
}
