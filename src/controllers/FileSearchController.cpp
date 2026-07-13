#include "FileSearchController.h"

#include "../core/ArchiveSupport.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QThreadPool>
#include <QUrl>

FileSearchController::FileSearchController(QObject *parent)
    : QObject(parent)
    , m_resultsModel(this)
{
    qRegisterMetaType<FileSearchResult>("FileSearchResult");
    qRegisterMetaType<QList<FileSearchResult>>("QList<FileSearchResult>");
}

FileSearchController::~FileSearchController()
{
    cancel();
}

FileSearchController::State FileSearchController::state() const
{
    return m_state;
}

bool FileSearchController::busy() const
{
    return m_state == State::Searching || m_state == State::Canceling;
}

QString FileSearchController::rootPath() const
{
    return m_rootPath;
}

QString FileSearchController::displayRootPath() const
{
    return QDir::toNativeSeparators(m_rootPath);
}

QString FileSearchController::query() const
{
    return m_query;
}

QString FileSearchController::currentPath() const
{
    return m_currentPath;
}

QString FileSearchController::currentDisplayPath() const
{
    return QDir::toNativeSeparators(m_currentPath);
}

QString FileSearchController::error() const
{
    return m_error;
}

QString FileSearchController::lastError() const
{
    return m_lastError;
}

int FileSearchController::scannedFiles() const
{
    return m_scannedFiles;
}

int FileSearchController::scannedFolders() const
{
    return m_scannedFolders;
}

int FileSearchController::inaccessiblePaths() const
{
    return m_inaccessiblePaths;
}

int FileSearchController::contentFilesScanned() const
{
    return m_contentFilesScanned;
}

int FileSearchController::contentFilesSkipped() const
{
    return m_contentFilesSkipped;
}

QString FileSearchController::coverageStatusText() const
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

QVariantList FileSearchController::skippedDetailEntries() const
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

FileSearchModel *FileSearchController::resultsModel()
{
    return &m_resultsModel;
}

bool FileSearchController::holdResultUpdates() const
{
    return m_holdResultUpdates;
}

void FileSearchController::setHoldResultUpdates(bool hold)
{
    if (m_holdResultUpdates == hold) {
        return;
    }
    m_holdResultUpdates = hold;
    emit holdResultUpdatesChanged();
    if (!m_holdResultUpdates) {
        flushPendingResults();
    }
}

bool FileSearchController::canSearchPath(const QString &path) const
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

void FileSearchController::search(const QString &rootPath, const QString &query, bool includeHidden, bool searchContents, bool caseSensitive, int matchMode, bool includeFolders)
{
    const QString normalizedPath = QDir::fromNativeSeparators(rootPath.trimmed());
    const QString trimmedQuery = query.trimmed();
    cancel();
    ++m_generation;
    m_rootPath = normalizedPath;
    m_query = trimmedQuery;
    emit rootPathChanged();
    emit queryChanged();
    resetProgress();
    m_pendingModelResults.clear();
    m_resultsModel.clear();
    setError({});

    if (!canSearchPath(normalizedPath)) {
        setError(QStringLiteral("This location cannot be searched."));
        setState(State::Failed);
        return;
    }

    if (trimmedQuery.isEmpty()) {
        setState(State::Idle);
        return;
    }

    m_rootPath = QFileInfo(normalizedPath).absoluteFilePath();
    emit rootPathChanged();
    setState(State::Searching);

    auto *scanner = new FileSearchScanner(m_rootPath, trimmedQuery, includeHidden, searchContents, caseSensitive, matchMode, includeFolders, m_generation);
    m_scanner = scanner;

    connect(scanner, &FileSearchScanner::finished, scanner, &QObject::deleteLater, Qt::QueuedConnection);

    connect(scanner,
            &FileSearchScanner::resultsReady,
            this,
            [this](const QList<FileSearchResult> &results,
                   int scannedFiles,
                   int scannedFolders,
                   int skippedPaths,
                   int inaccessiblePaths,
                   int reparsePaths,
                   int contentFilesScanned,
                   int contentFilesSkipped,
                   const QStringList &inaccessiblePathDetails,
                   const QStringList &reparsePathDetails,
                   const QString &currentPath,
                   const QString &lastError,
                   int generation) {
                if (generation != m_generation) {
                    return;
                }
                if (!results.isEmpty()) {
                    appendOrQueueResults(results);
                }
                applyProgress(scannedFiles,
                              scannedFolders,
                              skippedPaths,
                              inaccessiblePaths,
                              reparsePaths,
                              contentFilesScanned,
                              contentFilesSkipped,
                              inaccessiblePathDetails,
                              reparsePathDetails,
                              currentPath,
                              lastError);
            },
            Qt::QueuedConnection);

    connect(scanner,
            &FileSearchScanner::finished,
            this,
            [this, scanner](bool success,
                            const QString &error,
                            int scannedFiles,
                            int scannedFolders,
                            int skippedPaths,
                            int inaccessiblePaths,
                            int reparsePaths,
                            int contentFilesScanned,
                            int contentFilesSkipped,
                            const QStringList &inaccessiblePathDetails,
                            const QStringList &reparsePathDetails,
                            int generation) {
                if (generation != m_generation) {
                    return;
                }
                if (m_scanner == scanner) {
                    m_scanner = nullptr;
                }
                if (!m_holdResultUpdates) {
                    flushPendingResults();
                }
                applyProgress(scannedFiles,
                              scannedFolders,
                              skippedPaths,
                              inaccessiblePaths,
                              reparsePaths,
                              contentFilesScanned,
                              contentFilesSkipped,
                              inaccessiblePathDetails,
                              reparsePathDetails,
                              {},
                              m_lastError);
                setError(error);
                setState(success ? State::Finished : State::Failed);
            },
            Qt::QueuedConnection);

    QThreadPool::globalInstance()->start(scanner);
}

void FileSearchController::cancel()
{
    if (m_scanner) {
        m_scanner->cancel();
        m_scanner = nullptr;
    }
    m_pendingModelResults.clear();
    ++m_generation;
    if (m_state == State::Searching) {
        setState(State::Canceling);
    }
    if (m_state == State::Canceling) {
        setState(State::Idle);
    }
}

void FileSearchController::clear()
{
    cancel();
    m_rootPath.clear();
    m_query.clear();
    emit rootPathChanged();
    emit queryChanged();
    resetProgress();
    m_pendingModelResults.clear();
    m_resultsModel.clear();
    setError({});
    setState(State::Idle);
}

bool FileSearchController::revealPath(const QString &path) const
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
    return QProcess::startDetached(QStringLiteral("explorer.exe"), {QDir::toNativeSeparators(folder)});
#elif defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("open"), {folder});
#else
    return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
}

void FileSearchController::appendOrQueueResults(const QList<FileSearchResult> &results)
{
    if (m_holdResultUpdates) {
        m_pendingModelResults.append(results);
        return;
    }
    m_resultsModel.appendResults(results);
}

void FileSearchController::flushPendingResults()
{
    if (m_pendingModelResults.isEmpty()) {
        return;
    }

    QList<FileSearchResult> results;
    results.swap(m_pendingModelResults);
    m_resultsModel.appendResults(results);
}

void FileSearchController::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

void FileSearchController::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

void FileSearchController::resetProgress()
{
    m_currentPath.clear();
    m_lastError.clear();
    m_scannedFiles = 0;
    m_scannedFolders = 0;
    m_skippedPaths = 0;
    m_inaccessiblePaths = 0;
    m_reparsePaths = 0;
    m_contentFilesScanned = 0;
    m_contentFilesSkipped = 0;
    m_inaccessiblePathDetails.clear();
    m_reparsePathDetails.clear();
    emit progressChanged();
}

void FileSearchController::applyProgress(int scannedFiles,
                                         int scannedFolders,
                                         int skippedPaths,
                                         int inaccessiblePaths,
                                         int reparsePaths,
                                         int contentFilesScanned,
                                         int contentFilesSkipped,
                                         const QStringList &inaccessiblePathDetails,
                                         const QStringList &reparsePathDetails,
                                         const QString &currentPath,
                                         const QString &lastError)
{
    m_scannedFiles = scannedFiles;
    m_scannedFolders = scannedFolders;
    m_skippedPaths = skippedPaths;
    m_inaccessiblePaths = inaccessiblePaths;
    m_reparsePaths = reparsePaths;
    m_contentFilesScanned = contentFilesScanned;
    m_contentFilesSkipped = contentFilesSkipped;
    m_inaccessiblePathDetails = inaccessiblePathDetails;
    m_reparsePathDetails = reparsePathDetails;
    m_currentPath = currentPath;
    if (!lastError.isEmpty()) {
        m_lastError = lastError;
    }
    emit progressChanged();
}
