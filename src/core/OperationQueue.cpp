#include "OperationQueue.h"
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

namespace {

constexpr qint64 SmallFileLimit = 10 * 1024 * 1024; // 10MB
constexpr qint64 DirectArchiveExtractThreshold = 64 * 1024 * 1024; // 64MB
constexpr qint64 MetricsUpdateIntervalMs = 500;
constexpr qint64 CopyProgressUpdateIntervalMs = 100;
constexpr qint64 LinuxCrossFilesystemCopyBufferSize = 1 * 1024 * 1024;
constexpr qint64 LinuxCrossFilesystemCopyCacheWindow = 32 * 1024 * 1024;
constexpr qint64 ProviderLocalBatchFileLimit = 16 * 1024 * 1024;
constexpr qsizetype ProviderStagedBatchMaxFiles = 64;
constexpr qint64 ProviderStagedBatchMaxBytes = 128 * 1024 * 1024;

struct CopyFrame
{
    QString sourcePath;
    QString destinationPath;
    qint64 size = 0;
};

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

QString providerBatchLabel(QLatin1StringView phase, int waveIndex, int waveCount = 0)
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

QString requireLinuxAdminSessionNonce()
{
    const QString nonce = LinuxAdminBroker::activeSessionNonce();
    if (nonce.isEmpty()) {
        throw std::runtime_error("Linux administrator mode is not active");
    }
    return nonce;
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

QString sevenZipArchiveTypeForPath(const QString &archivePath)
{
    const QString lower = archivePath.toLower();
    if (lower.endsWith(QStringLiteral(".zip"))) {
        return QStringLiteral("zip");
    }
    if (lower.endsWith(QStringLiteral(".gz")) || lower.endsWith(QStringLiteral(".gzip"))) {
        return QStringLiteral("gzip");
    }
    if (lower.endsWith(QStringLiteral(".bz2")) || lower.endsWith(QStringLiteral(".bzip2"))) {
        return QStringLiteral("bzip2");
    }
    if (lower.endsWith(QStringLiteral(".xz"))) {
        return QStringLiteral("xz");
    }
    return QStringLiteral("7z");
}

bool isSingleFileCompressionType(const QString &type)
{
    return type == QLatin1String("gzip")
        || type == QLatin1String("bzip2")
        || type == QLatin1String("xz");
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
    enqueue({Type::Copy, sources, destination});
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
    enqueue({Type::Copy, sources, destination, true});
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
    enqueue({Type::CreateFolder, {name.trimmed()}, destination, true});
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
    enqueue({Type::Duplicate, sources, destinationHint});
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
    enqueue({Type::Compress, sources, archivePath});
}

void OperationQueue::compressToSevenZip(const QStringList &sources, const QString &archivePath)
{
    compressToArchive(sources, archivePath);
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
    enqueue({Type::Delete, paths, {}, true});
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

OperationQueue::OperationResult OperationQueue::execute(const Request &request)
{
    OperationResult result;
    result.request = request;

    resetProviderTransferTiming(request);

    setCurrentThreadAbortChecker([this]() {
        return m_abort.load();
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
    QMetaObject::invokeMethod(this, [this]() {
        setCurrentLabel(QStringLiteral("Scanning transfer..."));
    }, Qt::QueuedConnection);
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
            result.succeededCount = 1;
            QMetaObject::invokeMethod(this, [this]() {
                setCompletedItems(1);
                setProgress(1.0);
            }, Qt::QueuedConnection);
        } catch (const std::exception &exception) {
            recordFailure(folderPath.isEmpty() ? request.destination : folderPath, QString::fromUtf8(exception.what()));
        }
        return result;
    }

    if (request.type == Type::Compress) {
        try {
            compressPathsToSevenZip(request.sources, request.destination, totalBytes);
            if (m_abort) {
                result.aborted = true;
                return result;
            }
            result.succeededCount = totalFileCount;
            QMetaObject::invokeMethod(this, [this, totalFileCount]() {
                setCompletedItems(totalFileCount);
                setProgress(1.0);
            }, Qt::QueuedConnection);
        } catch (const std::exception &exception) {
            recordFailure(request.destination, QString::fromUtf8(exception.what()));
        }
        return result;
    }

    if (request.type == Type::Copy && request.administrator) {
        if (request.destination.isEmpty()) {
            recordFailure({}, QStringLiteral("Administrator copy destination is empty"));
            return result;
        }
        FileProvider *destProvider = getProviderForPath(request.destination);
        if (!destProvider || destProvider->scheme() != QLatin1String("file")) {
            recordFailure(request.destination, QStringLiteral("Administrator copy is available for local folders only"));
            return result;
        }

        for (int i = 0; i < totalFileCount; ++i) {
            if (m_abort) {
                result.aborted = true;
                return result;
            }

            const QString &source = request.sources.at(i);
            FileProvider *srcProvider = getProviderForPath(source);
            if (!srcProvider || srcProvider->scheme() != QLatin1String("file")) {
                recordFailure(source, QStringLiteral("Administrator copy is available for local files and folders only"));
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
                    const ConflictResolution res = waitForResolution(source, finalPath);
                    if (res == ConflictResolution::Skip) {
                        QMetaObject::invokeMethod(this, [this, i]() {
                            setCompletedItems(i + 1);
                        }, Qt::QueuedConnection);
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
                    adminRequest.sessionNonce = requireLinuxAdminSessionNonce();
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
                if (m_abort) {
                    result.aborted = true;
                    return result;
                }

                ++result.succeededCount;
                const double progress = static_cast<double>(i + 1) / static_cast<double>(totalFileCount);
                QMetaObject::invokeMethod(this, [this, i, progress]() {
                    setCompletedItems(i + 1);
                    setProgress(progress);
                }, Qt::QueuedConnection);
            } catch (const std::exception &exception) {
                recordFailure(source, QString::fromUtf8(exception.what()));
            }
        }

        if (m_abort) {
            result.aborted = true;
        } else if (result.failedCount > 0) {
            result.error = partialFailureSummary(result.failedCount, totalFileCount, result.error);
        }
        return result;
    }

    if (request.type == Type::Copy || request.type == Type::Move || request.type == Type::Duplicate) {
        for (const QString &source : request.sources) {
            if (ArchiveSupport::isArchivePath(source)) {
                continue;
            }

            FileProvider* srcProvider = getProviderForPath(source);
            if (!isRealDirectory(source)) {
                continue;
            }

            const QString sourceName = destinationNameForCopy(srcProvider, source);
            FileProvider* destProvider = request.type == Type::Duplicate
                ? srcProvider
                : getProviderForPath(request.destination);
            const QString destinationPath = request.type == Type::Duplicate
                ? duplicateDestinationPath(source)
                : (request.destination.isEmpty()
                    ? QString()
                    : destProvider->childPath(request.destination, sourceName));

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
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
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
            QMetaObject::invokeMethod(this, [this]() {
                setProgress(1.0);
            }, Qt::QueuedConnection);
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
                batchTempFinalized[i] = true;
            }

            currentProgressBytes = totalBytes;
            QMetaObject::invokeMethod(this, [this]() {
                setProgress(1.0);
            }, Qt::QueuedConnection);
            return result;
        }
    }

    if (request.type == Type::Copy
        && copySmallLocalFilesToProviderBatch(request.sources, request.destination, totalBytes, currentProgressBytes)) {
        if (m_abort) {
            result.aborted = true;
            return result;
        }
        result.succeededCount = totalFileCount;
        QMetaObject::invokeMethod(this, [this, totalFileCount]() {
            setCompletedItems(totalFileCount);
        }, Qt::QueuedConnection);
        return result;
    }

    if (request.type == Type::Copy
        && copyProviderFilesToLocalBatch(request.sources, request.destination, totalBytes, currentProgressBytes)) {
        if (m_abort) {
            result.aborted = true;
            return result;
        }
        result.succeededCount = totalFileCount;
        QMetaObject::invokeMethod(this, [this, totalFileCount]() {
            setCompletedItems(totalFileCount);
        }, Qt::QueuedConnection);
        return result;
    }

    if (request.type == Type::Copy
        && copyProviderFilesToProviderStagedBatch(request.sources, request.destination, totalBytes, currentProgressBytes)) {
        if (m_abort) {
            result.aborted = true;
            return result;
        }
        result.succeededCount = totalFileCount;
        QMetaObject::invokeMethod(this, [this, totalFileCount]() {
            setCompletedItems(totalFileCount);
        }, Qt::QueuedConnection);
        return result;
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
        const QString destinationName = destinationNameForCopy(srcProvider, source);
        FileProvider* destProvider = request.type == Type::Duplicate
            ? srcProvider
            : getProviderForPath(request.destination);
        const QString destinationPath = request.type == Type::Duplicate
            ? duplicateDestinationPath(source)
            : (request.destination.isEmpty() ? QString() : destProvider->childPath(request.destination, destinationName));
        if (request.type == Type::Copy) {
            const int batchCount = copyNextSmallLocalFilesToProviderBatch(
                request.sources,
                i,
                request.destination,
                totalBytes,
                currentProgressBytes);
            if (m_abort) {
                result.aborted = true;
                return result;
            }
            if (batchCount > 0) {
                result.succeededCount += batchCount;
                i += batchCount - 1;
                QMetaObject::invokeMethod(this, [this, i]() {
                    setCompletedItems(i + 1);
                }, Qt::QueuedConnection);
                continue;
            }
        }
        const int failureCountBefore = result.failedCount;

        try {
            if (request.type == Type::Copy) {
                copyPath(source, destinationPath, totalBytes, currentProgressBytes);
            } else if (request.type == Type::Duplicate) {
                copyPath(source, destinationPath, totalBytes, currentProgressBytes);
            } else if (request.type == Type::Extract) {
                extractArchiveContents(source, request.destination, totalBytes, currentProgressBytes);
            } else if (request.type == Type::Move) {
                movePath(source, destinationPath, totalBytes, currentProgressBytes);
            } else if (request.type == Type::Delete) {
                QMetaObject::invokeMethod(this, [this, name = sourceName, i]() {
                    setCurrentLabel(name);
                }, Qt::QueuedConnection);

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
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
            }
        } catch (const std::exception &exception) {
            recordFailure(source, QString::fromUtf8(exception.what()));
        }

        if (m_abort) {
            result.aborted = true;
            return result;
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
    const QStringList archiveTokens = ArchiveSupport::splitArchiveTokens(sourcePath);
    if (archiveTokens.size() == 2
        && ArchiveSupport::archiveBrowsePath(sourcePath) == QLatin1String("/")
        && ArchiveSupport::isArchiveFilePath(physicalArchivePath)) {
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

    QString extractionRoot = sourcePath;
    const QString archiveName = ArchiveSupport::archiveFileName(sourcePath);
    if (ArchiveSupport::isArchiveExtension(QFileInfo(archiveName).suffix().toLower())
        && ArchiveSupport::archiveBrowsePath(sourcePath) != QLatin1String("/")) {
        extractionRoot = ArchiveSupport::archiveRootPathForPath(sourcePath);
    }

    const QStringList children = childPaths(extractionRoot);
    QStringList sourceItems;
    QStringList destinationItems;
    sourceItems.reserve(children.size());
    destinationItems.reserve(children.size());
    for (const QString &child : children) {
        const std::optional<FileEntry> childInfo = srcProvider->entryInfo(child);
        const QString childName = childInfo ? childInfo->name : srcProvider->fileName(child);
        if (childName.isEmpty()) {
            continue;
        }
        sourceItems.append(child);
        destinationItems.append(destProvider->childPath(destinationPath, childName));
    }

    if (sourceItems.isEmpty()) {
        copiedBytes = (std::max)(copiedBytes, totalBytes);
        return;
    }

    QString error;
    const qint64 baseBytes = copiedBytes;
    const qint64 remainingBytes = (std::max<qint64>)(1, totalBytes - baseBytes);
    const bool extracted = ArchiveFileProvider::extractArchiveItemsTo(
        sourceItems,
        destinationItems,
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
            const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
            QMetaObject::invokeMethod(this, [this, progress]() {
                setProgress(progress);
            }, Qt::QueuedConnection);
            updateMetrics(progressBytes, totalBytes);
            return true;
        });
    if (!extracted) {
        if (m_abort) {
            return;
        }
        throw std::runtime_error(error.isEmpty()
            ? QStringLiteral("Cannot extract archive contents from %1").arg(sourcePath).toStdString()
            : error.toStdString());
    }

    copiedBytes = totalBytes;
}

void OperationQueue::compressPathsToSevenZip(const QStringList &sources, const QString &archivePath, qint64 totalBytes)
{
    if (m_abort || sources.isEmpty() || archivePath.isEmpty()) {
        return;
    }

    const QString executable = ArchiveSupport::sevenZipExecutablePath();
    if (executable.isEmpty()) {
        throw std::runtime_error("7-Zip executable was not found");
    }

    const QFileInfo archiveInfo(archivePath);
    const QString parentPath = archiveInfo.absolutePath();
    const QString archiveType = sevenZipArchiveTypeForPath(archivePath);
    if (archivePath.toLower().endsWith(QStringLiteral(".tar.gz"))
        || archivePath.toLower().endsWith(QStringLiteral(".tgz"))) {
        throw std::runtime_error("tar.gz compression is not available in a single 7-Zip pass");
    }
    if (isSingleFileCompressionType(archiveType)) {
        if (sources.size() != 1 || !QFileInfo(sources.constFirst()).isFile()) {
            throw std::runtime_error(QStringLiteral("%1 compression supports a single file only")
                                         .arg(archiveType)
                                         .toStdString());
        }
    }
    if (!QFileInfo(parentPath).isDir()) {
        throw std::runtime_error(QStringLiteral("Cannot create archive in %1").arg(parentPath).toStdString());
    }

    const QString tempArchivePath = archivePath + QStringLiteral(".part");
    QFile::remove(tempArchivePath);
    QFile::remove(archivePath);

    QString tempArchiveLeaseId;
    bool tempArchiveFinalized = false;
    CleanupSubsystem::instance().registerArtifact(
        CleanupArtifactKind::PartFile,
        tempArchivePath,
        QFileInfo(tempArchivePath).absolutePath(),
        false,
        &tempArchiveLeaseId);
    const auto tempArchiveCleanup = qScopeGuard([&]() {
        if (tempArchiveLeaseId.isEmpty()) {
            return;
        }
        if (tempArchiveFinalized) {
            CleanupSubsystem::instance().completeWithoutDelete(tempArchiveLeaseId);
        } else {
            CleanupSubsystem::instance().scheduleDeleteOnFailure(tempArchiveLeaseId);
        }
    });

    QStringList arguments = {
        QStringLiteral("a"),
        QStringLiteral("-t%1").arg(archiveType),
        QStringLiteral("-y"),
        QStringLiteral("-bso0"),
        QStringLiteral("-bsp1"),
        QStringLiteral("-bse1"),
        QDir::toNativeSeparators(tempArchivePath),
    };

    for (const QString &source : sources) {
        const QFileInfo sourceInfo(source);
        const QString sourceParent = sourceInfo.absolutePath();
        arguments.append(sourceParent.compare(parentPath, Qt::CaseInsensitive) == 0
                             ? sourceInfo.fileName()
                             : QDir::toNativeSeparators(sourceInfo.absoluteFilePath()));
    }

    QProcess process;
    process.setProgram(executable);
    process.setArguments(arguments);
    process.setWorkingDirectory(parentPath);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForStarted(5000)) {
        QFile::remove(tempArchivePath);
        throw std::runtime_error(QStringLiteral("Could not start 7-Zip: %1").arg(process.errorString()).toStdString());
    }

    QByteArray outputBuffer;
    int lastPercent = -1;
    QElapsedTimer progressTimer;
    progressTimer.start();
    const qint64 boundedTotalBytes = std::max<qint64>(1, totalBytes);
    const QRegularExpression percentPattern(QStringLiteral("(\\d{1,3})%"));

    auto consumeOutput = [&]() {
        outputBuffer.append(process.readAll());
        if (outputBuffer.size() > 8192) {
            outputBuffer = outputBuffer.right(8192);
        }

        const QString text = QString::fromLocal8Bit(outputBuffer);
        QRegularExpressionMatchIterator matches = percentPattern.globalMatch(text);
        int percent = -1;
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            bool ok = false;
            const int value = match.captured(1).toInt(&ok);
            if (ok) {
                percent = std::clamp(value, 0, 100);
            }
        }

        if (percent >= 0 && percent != lastPercent && progressTimer.elapsed() >= 120) {
            lastPercent = percent;
            progressTimer.restart();
            const qint64 processedBytes = (boundedTotalBytes * percent) / 100;
            const double progress = static_cast<double>(processedBytes) / static_cast<double>(boundedTotalBytes);
            QMetaObject::invokeMethod(this, [this, progress]() {
                setProgress(progress);
            }, Qt::QueuedConnection);
            updateMetrics(processedBytes, boundedTotalBytes);
        }
    };

    while (!process.waitForFinished(100)) {
        consumeOutput();
        if (m_abort) {
            process.kill();
            process.waitForFinished(3000);
            QFile::remove(tempArchivePath);
            return;
        }
    }
    consumeOutput();

    if (m_abort) {
        QFile::remove(tempArchivePath);
        return;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString output = QString::fromLocal8Bit(outputBuffer).trimmed();
        QFile::remove(tempArchivePath);
        throw std::runtime_error(output.isEmpty()
            ? QStringLiteral("7-Zip compression failed").toStdString()
            : output.toStdString());
    }

    if (QFile::exists(archivePath) && !QFile::remove(archivePath)) {
        QFile::remove(tempArchivePath);
        throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(archivePath).toStdString());
    }
    if (!QFile::rename(tempArchivePath, archivePath)) {
        QFile::remove(tempArchivePath);
        throw std::runtime_error(QStringLiteral("Cannot finalize %1").arg(archivePath).toStdString());
    }
    tempArchiveFinalized = true;

    QMetaObject::invokeMethod(this, [this]() {
        setProgress(1.0);
    }, Qt::QueuedConnection);
    updateMetrics(boundedTotalBytes, boundedTotalBytes);
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
            && ArchiveSupport::archiveBrowsePath(source) == QLatin1String("/")
            && ArchiveSupport::splitArchiveTokens(source).size() == 2) {
            const QString physicalPath = ArchiveSupport::physicalArchivePath(source);
            if (ArchiveSupport::isArchiveFilePath(physicalPath)) {
                total += (std::max<qint64>)(1, QFileInfo(physicalPath).size());
                continue;
            }
        }

        if (ArchiveSupport::isArchivePath(source)
            && ArchiveSupport::archiveBrowsePath(source) != QLatin1String("/")
            && ArchiveSupport::isArchiveExtension(
                QFileInfo(ArchiveSupport::archiveFileName(source)).suffix().toLower())) {
            total += totalBytesForPath(ArchiveSupport::archiveRootPathForPath(source));
            continue;
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

    return true;
}

void OperationQueue::copyPath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes)
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

                qint64 bufferSize = getBufferSizeByStorageType(getDriveTypeByPath(targetPath));
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
            qint64 bufferSize = getBufferSizeByStorageType(getDriveTypeByPath(targetPath));
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
        request.sessionNonce = requireLinuxAdminSessionNonce();
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
            setCurrentLabel(fileName);
        }, Qt::QueuedConnection);

        if (srcProvider == destProvider && samePath(*srcProvider, frame.sourcePath, frame.destinationPath)) {
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
            if (srcProvider == destProvider && isDescendantPath(*srcProvider, targetPath, frame.sourcePath)) {
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
    request.sessionNonce = requireLinuxAdminSessionNonce();
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
    request.sessionNonce = requireLinuxAdminSessionNonce();
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
