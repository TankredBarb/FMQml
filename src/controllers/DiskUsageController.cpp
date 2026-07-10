#include "DiskUsageController.h"

#include "../core/ArchiveSupport.h"
#include "../core/DriveUtils.h"

#include <QDir>
#include <QDebug>
#include <QDesktopServices>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QSet>
#include <QStorageInfo>
#include <QThreadPool>
#include <QUrl>
#include <algorithm>

namespace {
bool diskUsageLoggingEnabled()
{
    static const bool enabled = qEnvironmentVariableIntValue("FM_DISK_USAGE_LOG") != 0;
    return enabled;
}

void logDiskUsageEntries(const char *category, const QList<DiskUsageEntry> &entries, int limit)
{
    if (!diskUsageLoggingEnabled()) {
        return;
    }
    const int count = std::min(static_cast<int>(entries.size()), limit);
    for (int i = 0; i < count; ++i) {
        const DiskUsageEntry &entry = entries.at(i);
        qInfo().noquote()
            << "[DiskUsage]" << category
            << "rank=" << (i + 1)
            << "bytes=" << entry.size
            << "files=" << entry.fileCount
            << "folders=" << entry.folderCount
            << "path=" << QDir::toNativeSeparators(entry.path);
    }
}

QString normalizedCacheKey(const QString &path)
{
    QString key = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    key = key.toLower();
#endif
    return key;
}

bool samePath(const QString &left, const QString &right)
{
    return normalizedCacheKey(left) == normalizedCacheKey(right);
}

QString displayNameForBreadcrumb(const QString &path)
{
    QFileInfo info(path);
    if (info.isRoot()) {
        return QDir::toNativeSeparators(info.absoluteFilePath());
    }
    const QString name = info.fileName();
    return name.isEmpty() ? QDir::toNativeSeparators(path) : name;
}
}

DiskUsageController::DiskUsageController(QObject *parent)
    : QObject(parent)
    , m_rootChildrenModel(this)
    , m_summaryModel(this)
    , m_largestFoldersModel(this)
    , m_largestFilesModel(this)
{
    qRegisterMetaType<DiskUsageEntry>("DiskUsageEntry");
    qRegisterMetaType<QList<DiskUsageEntry>>("QList<DiskUsageEntry>");
}

DiskUsageController::~DiskUsageController()
{
    cancel();
}

DiskUsageController::State DiskUsageController::state() const
{
    return m_state;
}

bool DiskUsageController::busy() const
{
    return m_state == State::Scanning || m_state == State::Canceling;
}

QString DiskUsageController::rootPath() const
{
    return m_rootPath;
}

QString DiskUsageController::displayRootPath() const
{
    return QDir::toNativeSeparators(m_rootPath);
}

QVariantList DiskUsageController::breadcrumbEntries() const
{
    QVariantList entries;
    if (m_rootPath.isEmpty()) {
        return entries;
    }

    QFileInfo info(m_rootPath);
    QStringList chain;
    QString current = info.absoluteFilePath();
    while (!current.isEmpty()) {
        chain.prepend(current);
        QFileInfo currentInfo(current);
        if (currentInfo.isRoot()) {
            break;
        }
        const QString parent = currentInfo.absoluteDir().absolutePath();
        if (parent.isEmpty() || samePath(parent, current)) {
            break;
        }
        current = parent;
    }

    for (const QString &path : std::as_const(chain)) {
        const QFileInfo entryInfo(path);
        QVariantMap entry;
        entry.insert(QStringLiteral("path"), path);
        entry.insert(QStringLiteral("label"), displayNameForBreadcrumb(path));
        entry.insert(QStringLiteral("isDrive"), entryInfo.isRoot());
        entries.append(entry);
    }
    return entries;
}

bool DiskUsageController::canGoBack() const
{
    return !m_backStack.isEmpty();
}

bool DiskUsageController::canGoUp() const
{
    if (m_rootPath.isEmpty()) {
        return false;
    }
    const QFileInfo info(m_rootPath);
    return !info.isRoot();
}

QString DiskUsageController::currentPath() const
{
    return m_currentPath;
}

QString DiskUsageController::currentDisplayPath() const
{
    return QDir::toNativeSeparators(m_currentPath);
}

QString DiskUsageController::error() const
{
    return m_error;
}

QString DiskUsageController::lastError() const
{
    return m_lastError;
}

bool DiskUsageController::cached() const
{
    return m_cached;
}

QString DiskUsageController::cacheStatusText() const
{
    if (!m_cached || !m_cacheTimestamp.isValid()) {
        return {};
    }
    return QStringLiteral("Cached at %1").arg(QLocale().toString(m_cacheTimestamp.time(), QLocale::ShortFormat));
}

qint64 DiskUsageController::totalBytes() const
{
    return m_totalBytes;
}

QString DiskUsageController::totalBytesText() const
{
    return DriveUtils::formatSize(m_totalBytes);
}

QString DiskUsageController::storageUsedText() const
{
    if (m_rootPath.isEmpty()) {
        return {};
    }
    const QStorageInfo storage(m_rootPath);
    const qint64 total = storage.bytesTotal();
    const qint64 free = storage.bytesFree();
    if (!storage.isValid() || total <= 0 || free < 0) {
        return {};
    }
    return DriveUtils::formatSize(total - free);
}

QString DiskUsageController::storageTotalText() const
{
    if (m_rootPath.isEmpty()) {
        return {};
    }
    const QStorageInfo storage(m_rootPath);
    const qint64 total = storage.bytesTotal();
    if (!storage.isValid() || total <= 0) {
        return {};
    }
    return DriveUtils::formatSize(total);
}

int DiskUsageController::scannedFiles() const
{
    return m_scannedFiles;
}

int DiskUsageController::scannedFolders() const
{
    return m_scannedFolders;
}

int DiskUsageController::skippedPaths() const
{
    return m_skippedPaths;
}

int DiskUsageController::inaccessiblePaths() const
{
    return m_inaccessiblePaths;
}

int DiskUsageController::reparsePaths() const
{
    return m_reparsePaths;
}

QString DiskUsageController::coverageStatusText() const
{
    if (m_skippedPaths <= 0) {
        return QStringLiteral("Complete within accessible folders");
    }
    if (m_inaccessiblePaths > 0 && m_reparsePaths > 0) {
        return QStringLiteral("Partial: %1 inaccessible, %2 links skipped").arg(m_inaccessiblePaths).arg(m_reparsePaths);
    }
    if (m_inaccessiblePaths > 0) {
        return QStringLiteral("Partial: %1 inaccessible").arg(m_inaccessiblePaths);
    }
    return QStringLiteral("%1 links skipped").arg(m_reparsePaths);
}

QVariantList DiskUsageController::skippedDetailEntries() const
{
    QVariantList entries;
    for (const QString &detail : m_inaccessiblePathDetails) {
        QVariantMap entry;
        entry.insert(QStringLiteral("kind"), QStringLiteral("inaccessible"));
        entry.insert(QStringLiteral("label"), QStringLiteral("Inaccessible"));
        entry.insert(QStringLiteral("path"), detail);
        entries.append(entry);
    }
    for (const QString &detail : m_reparsePathDetails) {
        QVariantMap entry;
        entry.insert(QStringLiteral("kind"), QStringLiteral("link"));
        entry.insert(QStringLiteral("label"), QStringLiteral("Link"));
        entry.insert(QStringLiteral("path"), detail);
        entries.append(entry);
    }
    return entries;
}

DiskUsageModel *DiskUsageController::largestFoldersModel()
{
    return &m_largestFoldersModel;
}

DiskUsageModel *DiskUsageController::summaryModel()
{
    return &m_summaryModel;
}

DiskUsageModel *DiskUsageController::rootChildrenModel()
{
    return &m_rootChildrenModel;
}

DiskUsageModel *DiskUsageController::largestFilesModel()
{
    return &m_largestFilesModel;
}

bool DiskUsageController::canAnalyzePath(const QString &path) const
{
    if (path.isEmpty()
        || path.startsWith(QStringLiteral("archive://"), Qt::CaseInsensitive)
        || path.startsWith(QStringLiteral("devices://"), Qt::CaseInsensitive)
        || path.startsWith(QStringLiteral("favorites://"), Qt::CaseInsensitive)
        || ArchiveSupport::isArchivePath(path)) {
        return false;
    }

    const QFileInfo info(QDir::fromNativeSeparators(path));
    return info.exists() && info.isDir();
}

void DiskUsageController::scan(const QString &path)
{
    m_backStack.clear();
    emit navigationChanged();
    startScan(path, false);
}

void DiskUsageController::startScan(const QString &path, bool forceRescan)
{
    const QString normalizedPath = QDir::fromNativeSeparators(path.trimmed());
    if (!canAnalyzePath(normalizedPath)) {
        cancel();
        m_rootPath = normalizedPath;
        emit rootPathChanged();
        resetProgress();
        setCached(false);
        setError(QStringLiteral("This location cannot be analyzed."));
        setState(State::Failed);
        return;
    }

    if (!forceRescan && tryLoadCache(normalizedPath)) {
        return;
    }

    cancel();
    ++m_generation;
    m_rootPath = QFileInfo(normalizedPath).absoluteFilePath();
    emit rootPathChanged();
    resetProgress();
    setCached(false);
    setError({});
    if (diskUsageLoggingEnabled()) {
        qInfo().noquote()
            << "[DiskUsage] scan started"
            << "generation=" << m_generation
            << "root=" << QDir::toNativeSeparators(m_rootPath);
    }
    setState(State::Scanning);

    auto *scanner = new DiskUsageScanner(m_rootPath, m_generation, 200);
    m_scanner = scanner;

    connect(scanner,
            &DiskUsageScanner::snapshotReady,
            this,
            [this](const QList<DiskUsageEntry> &folders,
                   const QList<DiskUsageEntry> &files,
                   const QList<DiskUsageEntry> &rootChildren,
                   qint64 totalBytes,
                   int scannedFiles,
                   int scannedFolders,
                   int skippedPaths,
                   int inaccessiblePaths,
                   int reparsePaths,
                   const QStringList &inaccessiblePathDetails,
                   const QStringList &reparsePathDetails,
                   const QString &currentPath,
                   const QString &lastError,
                   int generation) {
                if (generation != m_generation) {
                    return;
                }
                applySnapshot(folders, files, rootChildren, totalBytes, scannedFiles, scannedFolders, skippedPaths, inaccessiblePaths, reparsePaths, inaccessiblePathDetails, reparsePathDetails, currentPath, lastError);
            },
            Qt::QueuedConnection);

    connect(scanner,
            &DiskUsageScanner::finished,
            this,
            [this, scanner](bool success,
                            const QString &error,
                            const QList<DiskUsageEntry> &folders,
                            const QList<DiskUsageEntry> &files,
                            const QList<DiskUsageEntry> &rootChildren,
                            qint64 totalBytes,
                            int scannedFiles,
                            int scannedFolders,
                            int skippedPaths,
                            int inaccessiblePaths,
                            int reparsePaths,
                            const QStringList &inaccessiblePathDetails,
                            const QStringList &reparsePathDetails,
                            int generation) {
                scanner->deleteLater();
                if (generation != m_generation) {
                    return;
                }
                if (m_scanner == scanner) {
                    m_scanner = nullptr;
                }
                applySnapshot(folders, files, rootChildren, totalBytes, scannedFiles, scannedFolders, skippedPaths, inaccessiblePaths, reparsePaths, inaccessiblePathDetails, reparsePathDetails, {}, m_lastError);
                if (diskUsageLoggingEnabled()) {
                    qInfo().noquote()
                        << "[DiskUsage] scan finished"
                        << "generation=" << generation
                        << "success=" << success
                        << "root=" << QDir::toNativeSeparators(m_rootPath)
                        << "bytes=" << totalBytes
                        << "files=" << scannedFiles
                        << "folders=" << scannedFolders
                        << "skipped=" << skippedPaths
                        << "inaccessible=" << inaccessiblePaths
                        << "reparse=" << reparsePaths
                        << "error=" << error;
                }
                logDiskUsageEntries("child", rootChildren, 60);
                logDiskUsageEntries("folder", folders, 40);
                logDiskUsageEntries("file", files, 40);
                setError(error);
                if (success) {
                    storeCache(folders, files, rootChildren, totalBytes, scannedFiles, scannedFolders, skippedPaths, inaccessiblePaths, reparsePaths, inaccessiblePathDetails, reparsePathDetails);
                }
                setState(success ? State::Finished : State::Failed);
            },
            Qt::QueuedConnection);

    QThreadPool::globalInstance()->start(scanner);
}

void DiskUsageController::rescan()
{
    if (!m_rootPath.isEmpty()) {
        startScan(m_rootPath, true);
    }
}

void DiskUsageController::navigateTo(const QString &path)
{
    const QString normalizedPath = QDir::fromNativeSeparators(path.trimmed());
    if (normalizedPath.isEmpty() || samePath(normalizedPath, m_rootPath)) {
        return;
    }
    if (!m_rootPath.isEmpty()) {
        m_backStack.append(m_rootPath);
        emit navigationChanged();
    }
    startScan(normalizedPath, false);
}

void DiskUsageController::navigateBack()
{
    if (m_backStack.isEmpty()) {
        return;
    }
    const QString previous = m_backStack.takeLast();
    emit navigationChanged();
    startScan(previous, false);
}

void DiskUsageController::navigateUp()
{
    if (!canGoUp()) {
        return;
    }
    const QString parent = QFileInfo(m_rootPath).absoluteDir().absolutePath();
    navigateTo(parent);
}

bool DiskUsageController::revealPath(const QString &path) const
{
    const QString normalizedPath = QDir::fromNativeSeparators(path.trimmed());
    if (normalizedPath.isEmpty() || !QFileInfo::exists(normalizedPath)) {
        return false;
    }

    const QFileInfo info(normalizedPath);
    const QString folder = info.isDir() ? normalizedPath : info.absolutePath();
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        return false;
    }

#if defined(Q_OS_WIN)
    const QString nativePath = QDir::toNativeSeparators(folder);
    return QProcess::startDetached(QStringLiteral("explorer.exe"),
                                   {nativePath});
#elif defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("open"), {folder});
#else
    return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
}

void DiskUsageController::cancel()
{
    if (m_scanner) {
        m_scanner->cancel();
        m_scanner = nullptr;
    }
    ++m_generation;
    if (m_state == State::Scanning) {
        setState(State::Canceling);
    }
    if (m_state == State::Canceling) {
        setState(State::Idle);
    }
}

void DiskUsageController::clear()
{
    cancel();
    m_rootPath.clear();
    m_backStack.clear();
    emit rootPathChanged();
    emit navigationChanged();
    resetProgress();
    setCached(false);
    setError({});
    setState(State::Idle);
}

void DiskUsageController::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

void DiskUsageController::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

void DiskUsageController::resetProgress()
{
    m_currentPath.clear();
    m_lastError.clear();
    m_totalBytes = 0;
    m_scannedFiles = 0;
    m_scannedFolders = 0;
    m_skippedPaths = 0;
    m_inaccessiblePaths = 0;
    m_reparsePaths = 0;
    m_inaccessiblePathDetails.clear();
    m_reparsePathDetails.clear();
    m_summaryModel.clear();
    m_rootChildrenModel.clear();
    m_largestFoldersModel.clear();
    m_largestFilesModel.clear();
    emit progressChanged();
}

void DiskUsageController::applySnapshot(const QList<DiskUsageEntry> &folders,
                                        const QList<DiskUsageEntry> &files,
                                        const QList<DiskUsageEntry> &rootChildren,
                                        qint64 totalBytes,
                                        int scannedFiles,
                                        int scannedFolders,
                                        int skippedPaths,
                                        int inaccessiblePaths,
                                        int reparsePaths,
                                        const QStringList &inaccessiblePathDetails,
                                        const QStringList &reparsePathDetails,
                                        const QString &currentPath,
                                        const QString &lastError)
{
    m_totalBytes = totalBytes;
    m_scannedFiles = scannedFiles;
    m_scannedFolders = scannedFolders;
    m_skippedPaths = skippedPaths;
    m_inaccessiblePaths = inaccessiblePaths;
    m_reparsePaths = reparsePaths;
    m_inaccessiblePathDetails = inaccessiblePathDetails;
    m_reparsePathDetails = reparsePathDetails;
    m_currentPath = currentPath;
    if (!lastError.isEmpty()) {
        m_lastError = lastError;
    }
    QList<DiskUsageEntry> summary = rootChildren;
    QSet<QString> summaryPaths;
    summaryPaths.reserve(summary.size() + files.size());
    for (const DiskUsageEntry &entry : std::as_const(summary)) {
        summaryPaths.insert(normalizedCacheKey(entry.path));
    }
    for (const DiskUsageEntry &entry : files) {
        const QString key = normalizedCacheKey(entry.path);
        if (summaryPaths.contains(key)) {
            continue;
        }
        summary.append(entry);
        summaryPaths.insert(key);
    }
    std::sort(summary.begin(), summary.end(), [](const DiskUsageEntry &left, const DiskUsageEntry &right) {
        if (left.size != right.size) {
            return left.size > right.size;
        }
        return left.path.compare(right.path, Qt::CaseInsensitive) < 0;
    });
    constexpr int maxSummaryEntries = 200;
    if (summary.size() > maxSummaryEntries) {
        summary.erase(summary.begin() + maxSummaryEntries, summary.end());
    }
    m_summaryModel.setEntries(summary, totalBytes);
    m_rootChildrenModel.setEntries(rootChildren, totalBytes);
    m_largestFoldersModel.setEntries(folders, totalBytes);
    m_largestFilesModel.setEntries(files, totalBytes);
    emit progressChanged();
}

QString DiskUsageController::cacheKeyForPath(const QString &path) const
{
    return normalizedCacheKey(QFileInfo(QDir::fromNativeSeparators(path)).absoluteFilePath());
}

bool DiskUsageController::tryLoadCache(const QString &path)
{
    const QString key = cacheKeyForPath(path);
    auto it = m_cache.constFind(key);
    if (it == m_cache.constEnd()) {
        return false;
    }

    cancel();
    ++m_generation;
    const CachedScan cachedScan = it.value();
    m_rootPath = cachedScan.rootPath;
    emit rootPathChanged();
    resetProgress();
    setError({});
    applySnapshot(cachedScan.folders,
                  cachedScan.files,
                  cachedScan.rootChildren,
                  cachedScan.totalBytes,
                  cachedScan.scannedFiles,
                  cachedScan.scannedFolders,
                  cachedScan.skippedPaths,
                  cachedScan.inaccessiblePaths,
                  cachedScan.reparsePaths,
                  cachedScan.inaccessiblePathDetails,
                  cachedScan.reparsePathDetails,
                  {},
                  cachedScan.lastError);
    setCached(true, cachedScan.timestamp);
    setState(State::Finished);
    if (diskUsageLoggingEnabled()) {
        qInfo().noquote()
            << "[DiskUsage] cache loaded"
            << "root=" << QDir::toNativeSeparators(m_rootPath)
            << "bytes=" << cachedScan.totalBytes
            << "files=" << cachedScan.scannedFiles
            << "folders=" << cachedScan.scannedFolders
            << "skipped=" << cachedScan.skippedPaths
            << "inaccessible=" << cachedScan.inaccessiblePaths
            << "reparse=" << cachedScan.reparsePaths;
    }
    return true;
}

void DiskUsageController::storeCache(const QList<DiskUsageEntry> &folders,
                                     const QList<DiskUsageEntry> &files,
                                     const QList<DiskUsageEntry> &rootChildren,
                                     qint64 totalBytes,
                                     int scannedFiles,
                                     int scannedFolders,
                                     int skippedPaths,
                                     int inaccessiblePaths,
                                     int reparsePaths,
                                     const QStringList &inaccessiblePathDetails,
                                     const QStringList &reparsePathDetails)
{
    if (m_rootPath.isEmpty()) {
        return;
    }

    CachedScan cachedScan;
    cachedScan.rootPath = m_rootPath;
    cachedScan.folders = folders;
    cachedScan.files = files;
    cachedScan.rootChildren = rootChildren;
    cachedScan.totalBytes = totalBytes;
    cachedScan.scannedFiles = scannedFiles;
    cachedScan.scannedFolders = scannedFolders;
    cachedScan.skippedPaths = skippedPaths;
    cachedScan.inaccessiblePaths = inaccessiblePaths;
    cachedScan.reparsePaths = reparsePaths;
    cachedScan.inaccessiblePathDetails = inaccessiblePathDetails;
    cachedScan.reparsePathDetails = reparsePathDetails;
    cachedScan.lastError = m_lastError;
    cachedScan.timestamp = QDateTime::currentDateTime();
    m_cache.insert(cacheKeyForPath(m_rootPath), cachedScan);
    setCached(false);
}

void DiskUsageController::setCached(bool cached, const QDateTime &timestamp)
{
    if (m_cached == cached && m_cacheTimestamp == timestamp) {
        return;
    }
    m_cached = cached;
    m_cacheTimestamp = timestamp;
    emit cacheStateChanged();
}
