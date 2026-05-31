#include "DirectoryModel.h"

#include "../core/ArchiveSupport.h"
#include "../core/FileAccessResolver.h"
#include "../core/FileError.h"
#include "../core/FileProviderFactory.h"
#include "../core/IsoSupport.h"
#include "../core/LocalFileProvider.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QtGlobal>
#include <algorithm>
#include <utility>

namespace {
const QSet<QString> kExecutableSuffixes = {
    QStringLiteral("exe"),
    QStringLiteral("bat"),
    QStringLiteral("cmd"),
    QStringLiteral("com"),
    QStringLiteral("ps1"),
    QStringLiteral("msi"),
    QStringLiteral("scr"),
    QStringLiteral("jar")
};

const QSet<QString> kLibrarySuffixes = {
    QStringLiteral("dll"),
    QStringLiteral("lib"),
    QStringLiteral("a"),
    QStringLiteral("so"),
    QStringLiteral("dylib"),
    QStringLiteral("ocx")
};

const QSet<QString> kImageSuffixes = {
    QStringLiteral("jpg"),
    QStringLiteral("jpeg"),
    QStringLiteral("png"),
    QStringLiteral("gif"),
    QStringLiteral("bmp"),
    QStringLiteral("webp"),
    QStringLiteral("ico"),
    QStringLiteral("svg"),
    QStringLiteral("svgz"),
    QStringLiteral("tif"),
    QStringLiteral("tiff"),
    QStringLiteral("avif"),
    QStringLiteral("heic")
};

const QSet<QString> kAudioSuffixes = {
    QStringLiteral("mp3"),
    QStringLiteral("flac"),
    QStringLiteral("ogg"),
    QStringLiteral("m4a"),
    QStringLiteral("m4b"),
    QStringLiteral("wav"),
    QStringLiteral("wma")
};

const QSet<QString> kVideoSuffixes = {
    QStringLiteral("mp4"),
    QStringLiteral("avi"),
    QStringLiteral("mkv"),
    QStringLiteral("mov"),
    QStringLiteral("wmv"),
    QStringLiteral("webm"),
    QStringLiteral("m4v")
};

const QSet<QString> kDocumentSuffixes = {
    QStringLiteral("pdf"),
    QStringLiteral("txt"),
    QStringLiteral("rtf"),
    QStringLiteral("md"),
    QStringLiteral("json"),
    QStringLiteral("xml"),
    QStringLiteral("html"),
    QStringLiteral("htm"),
    QStringLiteral("css"),
    QStringLiteral("js"),
    QStringLiteral("ts"),
    QStringLiteral("cpp"),
    QStringLiteral("c"),
    QStringLiteral("h"),
    QStringLiteral("hpp"),
    QStringLiteral("py"),
    QStringLiteral("rs"),
    QStringLiteral("go"),
    QStringLiteral("java"),
    QStringLiteral("kt"),
    QStringLiteral("qml"),
    QStringLiteral("ini"),
    QStringLiteral("yaml"),
    QStringLiteral("yml"),
    QStringLiteral("toml"),
    QStringLiteral("csv"),
    QStringLiteral("doc"),
    QStringLiteral("docx"),
    QStringLiteral("odt"),
    QStringLiteral("xls"),
    QStringLiteral("xlsx"),
    QStringLiteral("ods"),
    QStringLiteral("ppt"),
    QStringLiteral("pptx"),
    QStringLiteral("odp"),
    QStringLiteral("epub")
};

QString categoryFilterLabel(DirectoryModel::CategoryFilter filter)
{
    switch (filter) {
    case DirectoryModel::FilterExecutables:
        return QStringLiteral("Executables");
    case DirectoryModel::FilterLibraries:
        return QStringLiteral("Libraries");
    case DirectoryModel::FilterImages:
        return QStringLiteral("Images");
    case DirectoryModel::FilterArchives:
        return QStringLiteral("Archives");
    case DirectoryModel::FilterMedia:
        return QStringLiteral("Media");
    case DirectoryModel::FilterDocuments:
        return QStringLiteral("Documents");
    case DirectoryModel::FilterAll:
        break;
    }
    return QString();
}

FileEntry entryFromInfo(const QFileInfo &fileInfo)
{
    FileEntry entry;
    entry.name = fileInfo.fileName();
    entry.path = fileInfo.absoluteFilePath();
    entry.suffix = fileInfo.suffix();
    entry.size = fileInfo.size();
    entry.modified = fileInfo.lastModified();
    entry.created = fileInfo.birthTime().isValid() ? fileInfo.birthTime() : fileInfo.lastModified();
    entry.isDirectory = fileInfo.isDir();
    entry.isHidden = fileInfo.isHidden() || fileInfo.fileName().startsWith(QLatin1Char('.'));
    entry.isReadOnly = !fileInfo.isWritable();
    entry.isSystem = fileInfo.isSymLink();

    QLocale loc;
    entry.sizeText = entry.isDirectory
        ? QString()
        : loc.formattedDataSize(entry.size, 1, QLocale::DataSizeTraditionalFormat);
    entry.modifiedText = loc.toString(entry.modified, QLocale::ShortFormat);
    entry.createdText  = loc.toString(entry.created,  QLocale::ShortFormat);

    // Build attributes string
    QString attrs;
    if (entry.isDirectory) attrs += QLatin1Char('D');
    if (entry.isHidden)    attrs += QLatin1Char('H');
    if (entry.isReadOnly)  attrs += QLatin1Char('R');
    if (fileInfo.isSymLink()) attrs += QLatin1Char('L');
    entry.attributesText = attrs;

    static const QStringList imageSuffixes = kImageSuffixes.values();
    static const QStringList mediaSuffixes = {
        QStringLiteral("mp3"),
        QStringLiteral("flac"),
        QStringLiteral("ogg"),
        QStringLiteral("m4a"),
        QStringLiteral("mp4"),
        QStringLiteral("m4b"),
        QStringLiteral("wav"),
        QStringLiteral("wma"),
        QStringLiteral("avi"),
        QStringLiteral("mkv"),
        QStringLiteral("mov"),
        QStringLiteral("wmv"),
        QStringLiteral("pdf"),
        QStringLiteral("svg"),
        QStringLiteral("svgz"),
        QStringLiteral("ttf"),
        QStringLiteral("otf"),
        QStringLiteral("woff"),
        QStringLiteral("woff2")
    };
    entry.isImage = !entry.isDirectory && imageSuffixes.contains(entry.suffix.toLower());
    entry.hasThumbnail = entry.isImage || (!entry.isDirectory && mediaSuffixes.contains(entry.suffix.toLower()));
    return entry;
}

bool fileEntryMetadataChanged(const FileEntry &a, const FileEntry &b)
{
    return a.name != b.name
        || a.path != b.path
        || a.suffix != b.suffix
        || a.size != b.size
        || a.sizeText != b.sizeText
        || a.modified != b.modified
        || a.modifiedText != b.modifiedText
        || a.created != b.created
        || a.createdText != b.createdText
        || a.attributesText != b.attributesText
        || a.isDirectory != b.isDirectory
        || a.isHidden != b.isHidden
        || a.isImage != b.isImage
        || a.hasThumbnail != b.hasThumbnail
        || a.isReadOnly != b.isReadOnly
        || a.isSystem != b.isSystem;
}

bool watchDebugEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_WATCH_DEBUG");
    return enabled;
}

void traceDirectoryWatch(const char *stage, const QString &path, const QString &detail = {})
{
    if (!watchDebugEnabled()) {
        return;
    }
    qInfo().noquote() << "[DirectoryWatch]" << stage
                      << "path=" << path
                      << detail;
}

bool sameFilesystemPath(const QString &left, const QString &right)
{
#ifdef Q_OS_WIN
    return left.compare(right, Qt::CaseInsensitive) == 0;
#else
    return left == right;
#endif
}

QString modelPathKey(const QString &path)
{
    QString key = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    key = key.toLower();
#endif
    return key;
}

bool pathIsInDirectory(const QString &path, const QString &directoryPath)
{
    if (path.isEmpty() || directoryPath.isEmpty()) {
        return false;
    }

    QString normalizedPath = QDir::fromNativeSeparators(path);
    QString normalizedDirectory = QDir::fromNativeSeparators(directoryPath);
    if (!normalizedDirectory.endsWith(QLatin1Char('/'))) {
        normalizedDirectory += QLatin1Char('/');
    }

#ifdef Q_OS_WIN
    return normalizedPath.startsWith(normalizedDirectory, Qt::CaseInsensitive);
#else
    return normalizedPath.startsWith(normalizedDirectory);
#endif
}

bool eventSourceMatches(const DirectoryChangeEvent &event, const QString &watchPath)
{
    return event.sourcePath.isEmpty()
        || sameFilesystemPath(QDir::fromNativeSeparators(event.sourcePath),
                              QDir::fromNativeSeparators(watchPath));
}
}

DirectoryModel::DirectoryModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_provider(std::make_unique<LocalFileProvider>())
    , m_changeWatcher(createDirectoryChangeWatcher())
{
    connect(m_provider.get(), &FileProvider::started, this, &DirectoryModel::onScannerStarted);
    connect(m_provider.get(), &FileProvider::batchReady, this, &DirectoryModel::onScannerBatchReady);
    connect(m_provider.get(), &FileProvider::finished, this, &DirectoryModel::onScannerFinished);
    connect(m_changeWatcher.get(), &DirectoryChangeWatcher::eventsReady,
            this, &DirectoryModel::onDirectoryEventsReady);
    connect(m_changeWatcher.get(), &DirectoryChangeWatcher::watchFailed,
            this, &DirectoryModel::onDirectoryWatchFailed);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(500);
    connect(&m_debounceTimer, &QTimer::timeout, this, &DirectoryModel::onDebounceTimeout);

    m_directoryEventTimer.setSingleShot(true);
    m_directoryEventTimer.setInterval(150);
    connect(&m_directoryEventTimer, &QTimer::timeout, this, &DirectoryModel::processPendingDirectoryEvents);

    m_localMutationThrottle.invalidate();

    m_insertTimer.setInterval(16);
    connect(&m_insertTimer, &QTimer::timeout, this, &DirectoryModel::processPendingInserts);

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    openPath(home.isEmpty() ? QDir::homePath() : home);
}

int DirectoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_filteredIndices.size();
}

QVariant DirectoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredIndices.size()) {
        return {};
    }

    const FileEntry &entry = m_entries.at(m_filteredIndices.at(index.row()));
    switch (role) {
    case NameRole:
        return entry.name;
    case PathRole:
        return entry.path;
    case SizeRole:
        return entry.size;
    case SizeTextRole:
        return entry.sizeText;
    case ModifiedTextRole:
        return entry.modifiedText;
    case CreatedTextRole:
        return entry.createdText;
    case AttributesRole:
        return entry.attributesText;
    case IsDirectoryRole:
        return entry.isDirectory;
    case IsHiddenRole:
        return entry.isHidden;
    case IsSelectedRole:
        return entry.isSelected;
    case IconNameRole:
        return iconNameFor(entry);
    case SuffixRole:
        return entry.suffix;
    case IsImageRole:
        return entry.isImage;
    case HasThumbnailRole:
        return entry.hasThumbnail;
    case IsArchiveFileRole:
        return !entry.isDirectory
            && ArchiveSupport::archiveBackendAvailable()
            && ArchiveSupport::isArchiveFilePath(entry.path);
    case IsIsoImageFileRole:
        return !entry.isDirectory && IsoSupport::isIsoImagePath(entry.path);
    default:
        return {};
    }
}

QHash<int, QByteArray> DirectoryModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {SizeRole, "size"},
        {SizeTextRole, "sizeText"},
        {ModifiedTextRole, "modifiedText"},
        {CreatedTextRole, "createdText"},
        {AttributesRole, "attributesText"},
        {IsDirectoryRole, "isDirectory"},
        {IsHiddenRole, "isHidden"},
        {IsSelectedRole, "isSelected"},
        {IconNameRole, "iconName"},
        {SuffixRole, "suffix"},
        {IsImageRole, "isImage"},
        {HasThumbnailRole, "hasThumbnail"},
        {IsArchiveFileRole, "isArchiveFile"},
        {IsIsoImageFileRole, "isIsoImageFile"},
    };
}

QString DirectoryModel::currentPath() const
{
    return m_currentPath;
}

bool DirectoryModel::loading() const
{
    return m_loading;
}

QString DirectoryModel::error() const
{
    return m_error;
}

QVariantMap DirectoryModel::lastError() const
{
    return m_lastError;
}

int DirectoryModel::count() const
{
    return m_filteredIndices.size();
}

int DirectoryModel::selectedCount() const
{
    return m_selectedCount;
}

QString DirectoryModel::searchText() const
{
    return m_searchText;
}

void DirectoryModel::setSearchText(const QString &text)
{
    if (m_searchText == text) {
        return;
    }
    m_searchText = text;
    applyFilter();
    emit searchTextChanged();
}

DirectoryModel::CategoryFilter DirectoryModel::categoryFilter() const
{
    return m_categoryFilter;
}

void DirectoryModel::setCategoryFilter(CategoryFilter filter)
{
    if (m_categoryFilter == filter) {
        return;
    }

    m_categoryFilter = filter;
    applyFilter();
    notifyFiltersChanged();
}

bool DirectoryModel::hasActiveFilters() const
{
    return m_categoryFilter != FilterAll;
}

QString DirectoryModel::activeFiltersSummary() const
{
    return categoryFilterLabel(m_categoryFilter);
}

bool DirectoryModel::mixFilesAndFolders() const
{
    return m_mixFilesAndFolders;
}

void DirectoryModel::setMixFilesAndFolders(bool mix)
{
    if (m_mixFilesAndFolders == mix) {
        return;
    }
    m_mixFilesAndFolders = mix;
    sortModel();
    emit mixFilesAndFoldersChanged();
}

bool DirectoryModel::showHidden() const
{
    return m_showHidden;
}

void DirectoryModel::setShowHidden(bool show)
{
    if (m_showHidden == show) {
        return;
    }
    m_showHidden = show;
    m_provider->setShowHidden(show);
    
    // Immediately update the filtered indices for items we already have.
    applyFilterInternal(true);
    
    refresh();
    emit showHiddenChanged();
}

bool DirectoryModel::openPath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    const bool wantsArchive = (ArchiveSupport::isArchivePath(path) || ArchiveSupport::isArchiveFilePath(path))
        && ArchiveSupport::archiveBackendAvailable();
    const QString normalizedPath = wantsArchive && !ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::archiveRootPath(path)
        : ArchiveSupport::normalizeArchivePath(path);
    if (normalizedPath.isEmpty()) {
        return false;
    }
    if (wantsArchive && (!m_provider || m_provider->scheme() != QStringLiteral("archive"))) {
        replaceProvider(FileProviderFactory::createProvider(normalizedPath));
    } else if (!m_provider || !m_provider->canHandle(normalizedPath)) {
        replaceProvider(FileProviderFactory::createProvider(normalizedPath));
    }
    if (!m_provider || !m_provider->canHandle(normalizedPath)) {
        return false;
    }
    m_provider->setShowHidden(m_showHidden);
    m_provider->scan(normalizedPath);
    return true;
}

void DirectoryModel::replaceProvider(std::unique_ptr<FileProvider> provider)
{
    if (!provider) {
        return;
    }

    if (m_provider) {
        m_provider->cancel();
        disconnect(m_provider.get(), nullptr, this, nullptr);
    }

    m_provider = std::move(provider);
    connect(m_provider.get(), &FileProvider::started, this, &DirectoryModel::onScannerStarted);
    connect(m_provider.get(), &FileProvider::batchReady, this, &DirectoryModel::onScannerBatchReady);
    connect(m_provider.get(), &FileProvider::finished, this, &DirectoryModel::onScannerFinished);
    m_provider->setShowHidden(m_showHidden);
}

void DirectoryModel::onScannerStarted()
{
    m_debounceTimer.stop();
    m_directoryEventTimer.stop();
    m_pendingDirectoryEvents.clear();
    m_insertTimer.stop();
    
    const QString scanPath = m_provider->currentPath();
    const QString previousPath = m_currentPath;
    m_previousPath = previousPath;
    m_freshLoad = (scanPath != previousPath);
    m_currentScanGeneration = m_provider->currentGeneration();
    m_recoveringUnavailablePath = false;
    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    m_foundPaths.clear();
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;
    if (m_freshLoad) {
        m_localMutationThrottle.invalidate();
    }

    if (m_freshLoad) {
        m_selectedCount = 0;
        beginResetModel();
        m_entries.clear();
        m_filteredIndices.clear();
        m_pathIndex.clear();
        endResetModel();

        m_currentPath = scanPath;
        restartChangeWatcherForCurrentPath();
        emit currentPathChanged();
    }

    setLoading(true);
    setError({});
    setLastError({});
    emit countChanged();
    emit selectionChanged();
}

void DirectoryModel::onScannerBatchReady(const QList<FileEntry> &entries, int generation)
{
    if (generation != m_currentScanGeneration) {
        return;
    }

    if (entries.isEmpty()) {
        return;
    }

    m_pendingInserts.append(entries);
    if (!m_insertTimer.isActive()) {
        m_insertTimer.start();
    }
}

void DirectoryModel::processPendingInserts()
{
    if (m_pendingInsertOffset >= m_pendingInserts.size()) {
        m_pendingInserts.clear();
        m_pendingInsertOffset = 0;
        m_insertTimer.stop();
        if (m_pendingScannerFinish) {
            finalizeScannerFinished(m_pendingScannerPath, m_pendingScannerSuccess, m_pendingScannerError);
        }
        return;
    }

    const int chunkSize = 150;
    int processed = 0;

    while (m_pendingInsertOffset < m_pendingInserts.size() && processed < chunkSize) {
        FileEntry entry = m_pendingInserts.at(m_pendingInsertOffset++);
        processed++;

        const QString normalizedPath = modelPathKey(entry.path);
        const int absoluteIdx = m_pathIndex.value(normalizedPath, -1);

        const bool visible = m_showHidden || !entry.isHidden;
        const bool matchesFilter = this->matchesFilter(entry);
        const bool shouldBeVisible = visible && matchesFilter;

        if (absoluteIdx >= 0 && absoluteIdx < m_entries.size()) {
            FileEntry &existing = m_entries[absoluteIdx];
            const bool hasChanged = fileEntryMetadataChanged(existing, entry);
            const bool sortOrderChanged = hasChanged && (compareEntries(existing, entry) || compareEntries(entry, existing));

            int filteredRow = -1;
            for (int i = 0; i < m_filteredIndices.size(); ++i) {
                if (m_filteredIndices[i] == absoluteIdx) {
                    filteredRow = i;
                    break;
                }
            }

            if (shouldBeVisible && filteredRow == -1) {
                auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), absoluteIdx,
                    [this, &entry](int existingIdx, int) {
                        return this->compareEntries(m_entries.at(existingIdx), entry);
                    });
                const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
                beginInsertRows(QModelIndex(), row, row);
                m_filteredIndices.insert(row, absoluteIdx);
                endInsertRows();
            } else if (!shouldBeVisible && filteredRow != -1) {
                beginRemoveRows(QModelIndex(), filteredRow, filteredRow);
                m_filteredIndices.removeAt(filteredRow);
                endRemoveRows();
            } else if (shouldBeVisible && filteredRow != -1 && hasChanged) {
                bool wasSelected = existing.isSelected;
                existing = entry;
                existing.isSelected = wasSelected;
                emit dataChanged(index(filteredRow), index(filteredRow));
                if (sortOrderChanged) {
                    sortModel();
                }
            } else if (hasChanged) {
                bool wasSelected = existing.isSelected;
                existing = entry;
                existing.isSelected = wasSelected;
            }
            m_foundPaths.insert(normalizedPath);
        } else {
            const int newAbsoluteIdx = m_entries.size();
            m_entries.append(entry);
            m_pathIndex.insert(normalizedPath, newAbsoluteIdx);
            m_foundPaths.insert(normalizedPath);

            if (shouldBeVisible) {
                auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
                    [this, &entry](int existingIdx, int) {
                        return this->compareEntries(m_entries.at(existingIdx), entry);
                    });
                const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
                beginInsertRows(QModelIndex(), row, row);
                m_filteredIndices.insert(row, newAbsoluteIdx);
                endInsertRows();
            }
        }
    }

    if (m_pendingInsertOffset >= m_pendingInserts.size()) {
        m_pendingInserts.clear();
        m_pendingInsertOffset = 0;
        m_insertTimer.stop();
        if (m_pendingScannerFinish) {
            finalizeScannerFinished(m_pendingScannerPath, m_pendingScannerSuccess, m_pendingScannerError);
            return;
        }
    } else if (!m_insertTimer.isActive()) {
        m_insertTimer.start();
    }
    
    emit countChanged();
}

void DirectoryModel::onScannerFinished(const QString &path, bool success, int generation, const QString &error)
{
    if (generation != m_currentScanGeneration) {
        return;
    }

    const qsizetype pendingCount = m_pendingInserts.size() - m_pendingInsertOffset;
    if (success
        && pendingCount > 0
        && (pendingCount <= SmallDirectoryThreshold
            || (m_freshLoad && pendingCount >= LargeDirectoryBulkFinishThreshold))) {
        m_insertTimer.stop();
        processAllPendingInsertsFast();
        finalizeScannerFinished(path, success, error);
        return;
    }

    m_pendingScannerFinish = true;
    m_pendingScannerPath = path;
    m_pendingScannerSuccess = success;
    m_pendingScannerError = error;

    if (m_pendingInsertOffset < m_pendingInserts.size()) {
        if (!m_insertTimer.isActive()) {
            m_insertTimer.start();
        }
        return;
    }

    finalizeScannerFinished(path, success, error);
}

void DirectoryModel::finalizeScannerFinished(const QString &path, bool success, const QString &error)
{
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;

    setLoading(false);
    if (success) {
        if (!m_freshLoad) {
            for (int i = m_entries.size() - 1; i >= 0; --i) {
                const QString normPath = modelPathKey(m_entries.at(i).path);
                if (!m_foundPaths.contains(normPath)) {
                    if (m_entries.at(i).isSelected) {
                        --m_selectedCount;
                    }
                    
                    int filteredIdx = -1;
                    for (int j = 0; j < m_filteredIndices.size(); ++j) {
                        if (m_filteredIndices[j] == i) {
                            filteredIdx = j;
                            break;
                        }
                    }

                    if (filteredIdx != -1) {
                        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
                        m_filteredIndices.removeAt(filteredIdx);
                        endRemoveRows();
                    }

                    m_entries.removeAt(i);
                    for (int &idx : m_filteredIndices) {
                        if (idx > i) idx--;
                    }
                }
            }
            updatePathIndex();
            emit selectionChanged();
        }
        emit countChanged();
        if (m_deferredWatchRestartPending
            && sameFilesystemPath(QDir::fromNativeSeparators(path),
                                  QDir::fromNativeSeparators(m_deferredWatchRestartPath))) {
            scheduleDeferredWatchRestart();
        }
    } else {
        if (sameFilesystemPath(QDir::fromNativeSeparators(path), QDir::fromNativeSeparators(m_currentPath))
            && !currentPathExists()) {
            notifyCurrentPathUnavailable(error);
            m_previousPath.clear();
            return;
        }

        if (m_freshLoad) {
            if (!m_currentPath.isEmpty() && !ArchiveSupport::isArchivePath(m_currentPath)) {
                m_changeWatcher->stop();
            }
            m_currentPath = m_previousPath;
            restartChangeWatcherForCurrentPath();
            emit currentPathChanged();

            beginResetModel();
            m_entries.clear();
            m_filteredIndices.clear();
            m_pathIndex.clear();
            m_selectedCount = 0;
            endResetModel();
            emit countChanged();
            emit selectionChanged();
        }
        setError(error);
        setLastError(FileError::classify(error, path, QStringLiteral("open")));
        emit directoryUnavailable(path, error);
    }
    m_previousPath.clear();
}

void DirectoryModel::updatePathIndex()
{
    m_pathIndex.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        m_pathIndex.insert(modelPathKey(m_entries[i].path), i);
    }
}

int DirectoryModel::filteredRowForAbsoluteIndex(int absoluteIdx) const
{
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        if (m_filteredIndices.at(i) == absoluteIdx) {
            return i;
        }
    }
    return -1;
}

bool DirectoryModel::canWatchPath(const QString &path) const
{
    return !path.isEmpty()
        && !ArchiveSupport::isArchivePath(path)
        && m_provider
        && m_provider->capabilities().testFlag(FileProvider::Watch);
}

void DirectoryModel::restartChangeWatcherForCurrentPath()
{
    m_changeWatcher->stop();
    if (m_suppressNextWatchRestart) {
        m_suppressNextWatchRestart = false;
        m_deferredWatchRestartPending = true;
        m_deferredWatchRestartPath = m_currentPath;
        traceDirectoryWatch("restart-watch-suppressed", m_currentPath);
        return;
    }
    if (!canWatchPath(m_currentPath)) {
        return;
    }

    const bool watching = m_changeWatcher->watch(m_currentPath);
    if (!watching) {
        traceDirectoryWatch("restart-watch-failed", m_currentPath);
    }
}

void DirectoryModel::scheduleDeferredWatchRestart()
{
    if (!m_deferredWatchRestartPending) {
        return;
    }

    const QString expectedPath = m_deferredWatchRestartPath;
    traceDirectoryWatch("deferred-watch-schedule", expectedPath);
    QTimer::singleShot(600, this, [this, expectedPath]() {
        traceDirectoryWatch("deferred-watch-fire", expectedPath,
                            QStringLiteral("current=%1 loading=%2 exists=%3 watched=%4 pending=%5")
                                .arg(m_currentPath)
                                .arg(m_loading)
                                .arg(currentPathExists())
                                .arg(m_changeWatcher->watchedPath())
                                .arg(m_deferredWatchRestartPending));
        if (!m_deferredWatchRestartPending) {
            return;
        }
        if (m_loading
            || !sameFilesystemPath(QDir::fromNativeSeparators(m_currentPath),
                                   QDir::fromNativeSeparators(expectedPath))
            || !currentPathExists()
            || !m_changeWatcher->watchedPath().isEmpty()) {
            return;
        }

        m_deferredWatchRestartPending = false;
        m_deferredWatchRestartPath.clear();
        restartChangeWatcherForCurrentPath();
    });
}

void DirectoryModel::onDirectoryEventsReady(const QList<DirectoryChangeEvent> &events)
{
    if (events.isEmpty() || m_loading) {
        return;
    }
    const QString watchedPath = m_changeWatcher->watchedPath();
    for (const DirectoryChangeEvent &event : events) {
        if (!eventSourceMatches(event, watchedPath)) {
            traceDirectoryWatch("events-drop-source", m_currentPath,
                                QStringLiteral("source=%1 watched=%2")
                                    .arg(event.sourcePath)
                                    .arg(watchedPath));
            return;
        }
    }
    if (!currentPathExists()) {
        notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
        return;
    }

    m_watchEventsReceived += events.size();
    m_pendingDirectoryEvents.append(events);
    if (m_pendingDirectoryEvents.size() > 256) {
        m_pendingDirectoryEvents.clear();
        DirectoryChangeEvent overflow;
        overflow.type = DirectoryChangeEvent::Type::Overflow;
        overflow.path = m_currentPath;
        m_pendingDirectoryEvents.append(overflow);
    }
    if (watchDebugEnabled()) {
        qDebug() << "[DirectoryWatch] queued"
                 << "path" << m_currentPath
                 << "incoming" << events.size()
                 << "pending" << m_pendingDirectoryEvents.size()
                 << "received" << m_watchEventsReceived;
    }
    m_directoryEventTimer.start();
}

void DirectoryModel::processPendingDirectoryEvents()
{
    if (m_pendingDirectoryEvents.isEmpty()) {
        return;
    }
    if (m_loading) {
        m_pendingDirectoryEvents.clear();
        return;
    }
    if (!currentPathExists()) {
        m_pendingDirectoryEvents.clear();
        notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
        return;
    }

    const QList<DirectoryChangeEvent> events = std::exchange(m_pendingDirectoryEvents, {});
    applyDirectoryChangeEvents(events);
}

void DirectoryModel::applyDirectoryChangeEvents(const QList<DirectoryChangeEvent> &events)
{
    ++m_watchBatchesApplied;
    bool needsRefresh = false;
    QHash<QString, DirectoryChangeEvent> pendingByPath;
    QList<DirectoryChangeEvent> orderedEvents;

    for (const DirectoryChangeEvent &event : events) {
        if (!event.path.isEmpty()) {
            FileAccessResolver::invalidate(event.path);
        }
        if (!event.oldPath.isEmpty()) {
            FileAccessResolver::invalidate(event.oldPath);
        }
        if (!event.newPath.isEmpty()) {
            FileAccessResolver::invalidate(event.newPath);
        }

        if (event.type == DirectoryChangeEvent::Type::Overflow) {
            if (!sameFilesystemPath(QDir::fromNativeSeparators(event.path), QDir::fromNativeSeparators(m_currentPath))) {
                continue;
            }
            needsRefresh = true;
            break;
        }

        if ((!event.path.isEmpty() && !pathIsInDirectory(event.path, m_currentPath))
            || (!event.oldPath.isEmpty() && !pathIsInDirectory(event.oldPath, m_currentPath))
            || (!event.newPath.isEmpty() && !pathIsInDirectory(event.newPath, m_currentPath))) {
            continue;
        }

        switch (event.type) {
        case DirectoryChangeEvent::Type::Added:
        case DirectoryChangeEvent::Type::Modified:
            if (!event.path.isEmpty()) {
                DirectoryChangeEvent coalesced = event;
                coalesced.type = DirectoryChangeEvent::Type::Modified;
                pendingByPath.insert(modelPathKey(event.path), coalesced);
            }
            break;
        case DirectoryChangeEvent::Type::Removed:
            if (!event.path.isEmpty()) {
                const QString normalizedPath = modelPathKey(event.path);
                pendingByPath.remove(normalizedPath);
                DirectoryChangeEvent coalesced = event;
                coalesced.path = QDir::fromNativeSeparators(event.path);
                pendingByPath.insert(normalizedPath, coalesced);
            }
            break;
        case DirectoryChangeEvent::Type::Renamed:
            if (!event.oldPath.isEmpty() && !event.newPath.isEmpty()) {
                pendingByPath.remove(modelPathKey(event.oldPath));
                pendingByPath.remove(modelPathKey(event.newPath));
                orderedEvents.append(event);
            }
            break;
        case DirectoryChangeEvent::Type::Overflow:
            break;
        }
    }

    if (!needsRefresh) {
        int renameCount = 0;
        int upsertCount = 0;
        int removeCount = 0;

        for (const DirectoryChangeEvent &event : std::as_const(orderedEvents)) {
            ++renameCount;
            if (!renamePath(event.oldPath, event.newPath)) {
                removePath(event.oldPath);
                upsertPath(event.newPath);
            }
        }

        for (const DirectoryChangeEvent &event : std::as_const(pendingByPath)) {
            switch (event.type) {
            case DirectoryChangeEvent::Type::Added:
            case DirectoryChangeEvent::Type::Modified:
                ++upsertCount;
                upsertPath(event.path);
                break;
            case DirectoryChangeEvent::Type::Removed:
                ++removeCount;
                removePath(event.path);
                break;
            case DirectoryChangeEvent::Type::Renamed:
            case DirectoryChangeEvent::Type::Overflow:
                break;
            }
        }
        if (watchDebugEnabled()) {
            qDebug() << "[DirectoryWatch] applied"
                     << "path" << m_currentPath
                     << "batch" << m_watchBatchesApplied
                     << "events" << events.size()
                     << "renames" << renameCount
                     << "upserts" << upsertCount
                     << "removes" << removeCount;
        }
        return;
    }

    ++m_watchOverflowRefreshes;
    if (watchDebugEnabled()) {
        qDebug() << "[DirectoryWatch] overflow-refresh"
                 << "path" << m_currentPath
                 << "batch" << m_watchBatchesApplied
                 << "events" << events.size()
                 << "overflows" << m_watchOverflowRefreshes;
    }
    if (!QFileInfo::exists(m_currentPath) || !QFileInfo(m_currentPath).isDir()) {
        notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
        return;
    }
    m_debounceTimer.start();
}

void DirectoryModel::onDirectoryWatchFailed(const QString &path, const QString &error)
{
    traceDirectoryWatch("watch-failed", path,
                        QStringLiteral("current=%1 exists=%2 recovering=%3 error=%4")
                            .arg(m_currentPath)
                            .arg(currentPathExists())
                            .arg(m_recoveringUnavailablePath)
                            .arg(error));

    const QString failedPath = QDir::fromNativeSeparators(path);
    const QString currentPath = QDir::fromNativeSeparators(m_currentPath);
    if (!currentPath.isEmpty()
        && sameFilesystemPath(failedPath, currentPath)
        && !currentPathExists()) {
        notifyCurrentPathUnavailable(error);
    }
}

void DirectoryModel::onDebounceTimeout()
{
    if (!m_currentPath.isEmpty() && !m_loading) {
        if (!currentPathExists()) {
            notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
            return;
        }
        refresh();
    }
}

void DirectoryModel::applyFilter()
{
    applyFilterInternal(false);
}

bool DirectoryModel::matchesFilter(const FileEntry &entry) const
{
    if (!m_searchText.isEmpty()
        && !entry.name.contains(m_searchText, Qt::CaseInsensitive)) {
        return false;
    }

    if (m_categoryFilter == FilterAll) {
        return true;
    }

    if (entry.isDirectory) {
        return false;
    }

    const QString suffix = entry.suffix.toLower();
    switch (m_categoryFilter) {
    case FilterExecutables:
        return kExecutableSuffixes.contains(suffix);
    case FilterLibraries:
        return kLibrarySuffixes.contains(suffix);
    case FilterImages:
        return kImageSuffixes.contains(suffix);
    case FilterArchives:
        return ArchiveSupport::isArchiveExtension(suffix) || IsoSupport::isIsoImageExtension(suffix);
    case FilterMedia:
        return kAudioSuffixes.contains(suffix) || kVideoSuffixes.contains(suffix);
    case FilterDocuments:
        return kDocumentSuffixes.contains(suffix);
    case FilterAll:
        break;
    }

    return true;
}

void DirectoryModel::notifyFiltersChanged()
{
    emit filtersChanged();
}

void DirectoryModel::applyFilterInternal(bool keepSelection)
{
    if (!keepSelection) {
        for (FileEntry &entry : m_entries) {
            entry.isSelected = false;
        }
        m_selectedCount = 0;
    }

    beginResetModel();
    m_filteredIndices.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        const FileEntry &entry = m_entries.at(i);
        const bool visible = m_showHidden || !entry.isHidden;
        const bool matchesFilter = this->matchesFilter(entry);
        
        if (visible && matchesFilter) {
            m_filteredIndices.append(i);
        }
    }
    std::stable_sort(m_filteredIndices.begin(), m_filteredIndices.end(),
        [this](int aIdx, int bIdx) {
            return compareEntries(m_entries.at(aIdx), m_entries.at(bIdx));
        });
    endResetModel();
    emit countChanged();
    emit selectionChanged();
}

void DirectoryModel::refresh()
{
    if (!m_currentPath.isEmpty()) {
        if (!currentPathExists()) {
            notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
            return;
        }
        m_provider->setShowHidden(m_showHidden);
        m_provider->scan(m_currentPath);
    }
}

bool DirectoryModel::currentPathExists() const
{
    return m_currentPath.isEmpty()
        || ArchiveSupport::isArchivePath(m_currentPath)
        || (QFileInfo::exists(m_currentPath) && QFileInfo(m_currentPath).isDir());
}

void DirectoryModel::notifyCurrentPathUnavailable(const QString &error)
{
    traceDirectoryWatch("unavailable-enter", m_currentPath,
                        QStringLiteral("recovering=%1 error=%2 watched=%3")
                            .arg(m_recoveringUnavailablePath)
                            .arg(error)
                            .arg(m_changeWatcher->watchedPath()));
    if (m_currentPath.isEmpty() || m_recoveringUnavailablePath) {
        return;
    }
    m_recoveringUnavailablePath = true;

    const QString unavailablePath = m_currentPath;
    if (m_provider) {
        m_provider->cancel();
        m_currentScanGeneration = m_provider->currentGeneration();
    }
    m_changeWatcher->stop();
    m_deferredWatchRestartPending = false;
    m_deferredWatchRestartPath.clear();
    m_debounceTimer.stop();
    m_directoryEventTimer.stop();
    m_pendingDirectoryEvents.clear();
    m_insertTimer.stop();
    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;
    beginResetModel();
    m_entries.clear();
    m_filteredIndices.clear();
    m_pathIndex.clear();
    m_foundPaths.clear();
    m_selectedCount = 0;
    m_currentPath.clear();
    endResetModel();
    setLoading(false);
    setError(QStringLiteral("Folder is no longer available"));
    emit currentPathChanged();
    emit countChanged();
    emit selectionChanged();
    emit directoryUnavailable(unavailablePath,
                              error.isEmpty()
                                  ? QStringLiteral("Folder is no longer available")
                                  : error);
}

void DirectoryModel::clearError()
{
    setError({});
    setLastError({});
}

void DirectoryModel::clearFilters()
{
    const bool hadFilters = hasActiveFilters();
    if (!hadFilters) {
        return;
    }

    m_categoryFilter = FilterAll;
    applyFilter();
    notifyFiltersChanged();
}

void DirectoryModel::noteLocalMutation()
{
    m_localMutationThrottle.restart();
    m_debounceTimer.stop();
}

void DirectoryModel::suppressNextWatchRestart()
{
    m_suppressNextWatchRestart = true;
    m_deferredWatchRestartPending = false;
    m_deferredWatchRestartPath.clear();
    traceDirectoryWatch("suppress-next-watch", m_currentPath);
}

bool DirectoryModel::upsertPath(const QString &path)
{
    if (path.isEmpty() || m_currentPath.isEmpty() || ArchiveSupport::isArchivePath(m_currentPath)) {
        return false;
    }

    const QFileInfo info(path);
    const QString normalizedPath = QDir::fromNativeSeparators(info.absoluteFilePath());
    const QString pathKey = modelPathKey(normalizedPath);
    const QString parentPath = QDir::fromNativeSeparators(info.absolutePath());
    const QString currentPath = QDir::fromNativeSeparators(QFileInfo(m_currentPath).absoluteFilePath());

    if (!sameFilesystemPath(parentPath, currentPath)) {
        return false;
    }

    std::optional<FileEntry> maybeEntry = m_provider ? m_provider->entryInfo(normalizedPath) : std::nullopt;
    if (!maybeEntry.has_value()) {
        return removePath(path);
    }

    FileEntry entry = maybeEntry.value();
    const QString entryPathKey = modelPathKey(entry.path);
    const int absoluteIdx = m_pathIndex.value(pathKey, -1);
    const bool shouldBeVisible = (m_showHidden || !entry.isHidden) && matchesFilter(entry);

    if (absoluteIdx < 0) {
        const int newAbsoluteIdx = m_entries.size();
        m_entries.append(entry);
        m_pathIndex.insert(entryPathKey, newAbsoluteIdx);

        if (shouldBeVisible) {
            auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
                [this, &entry](int existingIdx, int) {
                    return compareEntries(m_entries.at(existingIdx), entry);
                });
            const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
            beginInsertRows(QModelIndex(), row, row);
            m_filteredIndices.insert(row, newAbsoluteIdx);
            endInsertRows();
        }

        emit countChanged();
        return true;
    }

    const int filteredRow = filteredRowForAbsoluteIndex(absoluteIdx);

    FileEntry &existing = m_entries[absoluteIdx];
    const bool wasSelected = existing.isSelected;
    const bool changed = fileEntryMetadataChanged(existing, entry);
    const bool sortOrderChanged = changed && (compareEntries(existing, entry) || compareEntries(entry, existing));
    entry.isSelected = wasSelected;

    if (shouldBeVisible && filteredRow == -1) {
        existing = entry;
        auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), absoluteIdx,
            [this, &entry](int existingIdx, int) {
                return compareEntries(m_entries.at(existingIdx), entry);
            });
        const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
        beginInsertRows(QModelIndex(), row, row);
        m_filteredIndices.insert(row, absoluteIdx);
        endInsertRows();
        emit countChanged();
        return true;
    }

    if (!shouldBeVisible && filteredRow != -1) {
        existing = entry;
        beginRemoveRows(QModelIndex(), filteredRow, filteredRow);
        m_filteredIndices.removeAt(filteredRow);
        endRemoveRows();
        emit countChanged();
        return true;
    }

    if (changed) {
        existing = entry;
        if (filteredRow != -1) {
            emit dataChanged(index(filteredRow), index(filteredRow));
            if (sortOrderChanged) {
                sortModel();
            }
        }
        return true;
    }

    return false;
}

bool DirectoryModel::insertPath(const QString &path)
{
    if (path.isEmpty() || m_currentPath.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    if (!info.exists()) {
        return false;
    }

    const QString normPath = modelPathKey(info.absoluteFilePath());
    if (!sameFilesystemPath(QDir::fromNativeSeparators(info.absolutePath()),
                            QDir::fromNativeSeparators(m_currentPath))) {
        return false;
    }
    if (m_pathIndex.contains(normPath)) {
        return false;
    }

    const FileEntry entry = entryFromInfo(info);
    const int newAbsoluteIdx = m_entries.size();
    m_entries.append(entry);
    m_pathIndex.insert(normPath, newAbsoluteIdx);

    const bool visible = m_showHidden || !entry.isHidden;
    const bool matchesEntryFilter = this->matchesFilter(entry);

    if (visible && matchesEntryFilter) {
        auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
            [&](int existingIdx, int) {
                return this->compareEntries(m_entries.at(existingIdx), entry);
            });
        const int row = std::distance(m_filteredIndices.begin(), it);
        beginInsertRows(QModelIndex(), row, row);
        m_filteredIndices.insert(row, newAbsoluteIdx);
        endInsertRows();
    }

    emit countChanged();
    return true;
}

bool DirectoryModel::removePath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    const QString normalizedPath = modelPathKey(QFileInfo(path).absoluteFilePath());
    const int absoluteIdx = m_pathIndex.value(normalizedPath, -1);
    
    if (absoluteIdx < 0) {
        return false;
    }

    if (m_entries.at(absoluteIdx).isSelected) {
        --m_selectedCount;
        emit selectionChanged();
    }

    const int filteredIdx = filteredRowForAbsoluteIndex(absoluteIdx);

    if (filteredIdx != -1) {
        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
        m_filteredIndices.removeAt(filteredIdx);
        endRemoveRows();
    }

    m_pathIndex.remove(normalizedPath);
    m_entries.removeAt(absoluteIdx);
    
    for (int &idx : m_filteredIndices) {
        if (idx > absoluteIdx) {
            --idx;
        }
    }
    updatePathIndex();
    
    emit countChanged();
    return true;
}

bool DirectoryModel::renamePath(const QString &oldPath, const QString &newPath)
{
    if (oldPath.isEmpty() || newPath.isEmpty()) {
        return false;
    }

    const QString oldPathKey = modelPathKey(QFileInfo(oldPath).absoluteFilePath());
    const QString newPathKey = modelPathKey(QFileInfo(newPath).absoluteFilePath());
    if (oldPathKey == newPathKey) {
        if (!m_pathIndex.contains(oldPathKey)) {
            return false;
        }
        return upsertPath(newPath) || QFileInfo(newPath).exists();
    }

    const int absoluteIdx = m_pathIndex.value(oldPathKey, -1);
    if (absoluteIdx < 0) {
        return false;
    }

    const bool wasSelected = m_entries.at(absoluteIdx).isSelected;
    if (!removePath(oldPath)) {
        return false;
    }

    const QString normalizedNewPath = QDir::fromNativeSeparators(QFileInfo(newPath).absoluteFilePath());
    const bool inserted = insertPath(normalizedNewPath);
    if (inserted && wasSelected) {
        const int row = indexOfPath(normalizedNewPath);
        if (row >= 0) {
            const int actualIdx = m_filteredIndices.at(row);
            m_entries[actualIdx].isSelected = true;
            ++m_selectedCount;
            emit dataChanged(index(row), index(row), {IsSelectedRole});
            emit selectionChanged();
        }
    }
    return inserted;
}

void DirectoryModel::toggleSelected(int row)
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return;
    }
    const int actualIdx = m_filteredIndices.at(row);
    m_entries[actualIdx].isSelected = !m_entries[actualIdx].isSelected;
    m_selectedCount += m_entries[actualIdx].isSelected ? 1 : -1;
    emit dataChanged(index(row), index(row), {IsSelectedRole});
    emit selectionChanged();
}

void DirectoryModel::selectOnly(int row)
{
    const int targetActualIdx = (row >= 0 && row < m_filteredIndices.size()) 
        ? m_filteredIndices.at(row) 
        : -1;

    bool selectionChangedOccurred = false;

    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].isSelected && i != targetActualIdx) {
            m_entries[i].isSelected = false;
            --m_selectedCount;
            selectionChangedOccurred = true;
            for (int j = 0; j < m_filteredIndices.size(); ++j) {
                if (m_filteredIndices[j] == i) {
                    emit dataChanged(index(j), index(j), {IsSelectedRole});
                    break;
                }
            }
        }
    }

    if (targetActualIdx != -1 && !m_entries[targetActualIdx].isSelected) {
        m_entries[targetActualIdx].isSelected = true;
        ++m_selectedCount;
        selectionChangedOccurred = true;
        emit dataChanged(index(row), index(row), {IsSelectedRole});
    }

    if (selectionChangedOccurred) {
        emit selectionChanged();
    }
}

void DirectoryModel::selectRange(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_filteredIndices.size() || to >= m_filteredIndices.size()) {
        return;
    }

    int start = std::min(from, to);
    int end = std::max(from, to);

    bool selectionChangedOccurred = false;

    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].isSelected) {
            m_entries[i].isSelected = false;
            --m_selectedCount;
            selectionChangedOccurred = true;
            for (int j = 0; j < m_filteredIndices.size(); ++j) {
                if (m_filteredIndices[j] == i) {
                    emit dataChanged(index(j), index(j), {IsSelectedRole});
                    break;
                }
            }
        }
    }

    for (int i = start; i <= end; ++i) {
        int absIdx = m_filteredIndices.at(i);
        if (!m_entries[absIdx].isSelected) {
            m_entries[absIdx].isSelected = true;
            ++m_selectedCount;
            selectionChangedOccurred = true;
            emit dataChanged(index(i), index(i), {IsSelectedRole});
        }
    }

    if (selectionChangedOccurred) {
        emit selectionChanged();
    }
}

void DirectoryModel::selectRows(const QVariantList &rows)
{
    QSet<int> targetActualIndices;
    targetActualIndices.reserve(rows.size());
    for (const QVariant &rowValue : rows) {
        bool ok = false;
        const int row = rowValue.toInt(&ok);
        if (!ok || row < 0 || row >= m_filteredIndices.size()) {
            continue;
        }
        targetActualIndices.insert(m_filteredIndices.at(row));
    }

    QSet<int> changedActualIndices;
    qsizetype selectedCount = 0;

    for (int i = 0; i < m_entries.size(); ++i) {
        const bool shouldSelect = targetActualIndices.contains(i);
        if (m_entries[i].isSelected != shouldSelect) {
            m_entries[i].isSelected = shouldSelect;
            changedActualIndices.insert(i);
        }
        if (shouldSelect) {
            ++selectedCount;
        }
    }

    if (changedActualIndices.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_filteredIndices.size(); ++row) {
        if (changedActualIndices.contains(m_filteredIndices.at(row))) {
            emit dataChanged(index(row), index(row), {IsSelectedRole});
        }
    }

    m_selectedCount = static_cast<int>(selectedCount);
    emit selectionChanged();
}

void DirectoryModel::invertSelection()
{
    if (m_filteredIndices.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_filteredIndices.size(); ++row) {
        const int actualIdx = m_filteredIndices.at(row);
        m_entries[actualIdx].isSelected = !m_entries[actualIdx].isSelected;
        emit dataChanged(index(row), index(row), {IsSelectedRole});
    }

    int selectedCount = 0;
    for (const FileEntry &entry : m_entries) {
        if (entry.isSelected) {
            ++selectedCount;
        }
    }
    m_selectedCount = selectedCount;
    emit selectionChanged();
}

void DirectoryModel::clearSelection()
{
    if (m_selectedCount == 0) return;

    bool selectionChangedOccurred = false;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].isSelected) {
            m_entries[i].isSelected = false;
            --m_selectedCount;
            selectionChangedOccurred = true;

            for (int j = 0; j < m_filteredIndices.size(); ++j) {
                if (m_filteredIndices[j] == i) {
                    emit dataChanged(index(j), index(j), {IsSelectedRole});
                    break;
                }
            }
        }
    }

    if (selectionChangedOccurred) {
        emit selectionChanged();
    }
}

void DirectoryModel::selectAll()
{
    bool changed = false;
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        int absIdx = m_filteredIndices[i];
        if (!m_entries[absIdx].isSelected) {
            m_entries[absIdx].isSelected = true;
            ++m_selectedCount;
            changed = true;
            emit dataChanged(index(i), index(i), {IsSelectedRole});
        }
    }
    if (changed)
        emit selectionChanged();
}

QString DirectoryModel::pathAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return {};
    }
    return m_entries.at(m_filteredIndices.at(row)).path;
}

bool DirectoryModel::isDirectoryAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return false;
    }
    return m_entries.at(m_filteredIndices.at(row)).isDirectory;
}

int DirectoryModel::indexOfPath(const QString &path) const
{
    const QString normPath = modelPathKey(path);
    const int absIdx = m_pathIndex.value(normPath, -1);
    if (absIdx == -1) return -1;
    
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        if (m_filteredIndices[i] == absIdx) return i;
    }
    return -1;
}

QStringList DirectoryModel::selectedPaths() const
{
    QStringList paths;
    for (const FileEntry &entry : m_entries) {
        if (entry.isSelected) {
            paths.append(entry.path);
        }
    }
    return paths;
}

QString DirectoryModel::formatSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

QString DirectoryModel::iconNameFor(const FileEntry &entry)
{
    if (entry.isDirectory) {
        return QStringLiteral("folder");
    }
    if (ArchiveSupport::isArchiveExtension(entry.suffix)) {
        return QStringLiteral("archive");
    }
    if (IsoSupport::isIsoImageExtension(entry.suffix)) {
        return QStringLiteral("archive");
    }
    return QStringLiteral("file");
}

void DirectoryModel::processAllPendingInsertsFast()
{
    if (m_pendingInsertOffset >= m_pendingInserts.size()) {
        m_pendingInserts.clear();
        m_pendingInsertOffset = 0;
        return;
    }

    if (m_freshLoad) {
        beginResetModel();
        while (m_pendingInsertOffset < m_pendingInserts.size()) {
            FileEntry entry = m_pendingInserts.at(m_pendingInsertOffset++);
            const QString normalizedPath = modelPathKey(entry.path);

            if (m_pathIndex.contains(normalizedPath)) {
                m_foundPaths.insert(normalizedPath);
                continue;
            }

            const int newAbsoluteIdx = m_entries.size();
            m_entries.append(entry);
            m_pathIndex.insert(normalizedPath, newAbsoluteIdx);
            m_foundPaths.insert(normalizedPath);
        }

        m_filteredIndices.clear();
        m_filteredIndices.reserve(m_entries.size());
        for (int i = 0; i < m_entries.size(); ++i) {
            const FileEntry &entry = m_entries.at(i);
            const bool visible = m_showHidden || !entry.isHidden;
            const bool matchesFilter = this->matchesFilter(entry);
            if (visible && matchesFilter) {
                m_filteredIndices.append(i);
            }
        }
        std::stable_sort(m_filteredIndices.begin(), m_filteredIndices.end(),
            [this](int aIdx, int bIdx) {
                return compareEntries(m_entries.at(aIdx), m_entries.at(bIdx));
            });
        endResetModel();
    } else {
        while (m_pendingInsertOffset < m_pendingInserts.size()) {
            FileEntry entry = m_pendingInserts.at(m_pendingInsertOffset++);
            const QString normalizedPath = modelPathKey(entry.path);
            const int absoluteIdx = m_pathIndex.value(normalizedPath, -1);

            if (absoluteIdx >= 0 && absoluteIdx < m_entries.size()) {
                FileEntry &existing = m_entries[absoluteIdx];
                const bool changed = fileEntryMetadataChanged(existing, entry);
                const bool sortOrderChanged = changed && (compareEntries(existing, entry) || compareEntries(entry, existing));

                const bool visible = m_showHidden || !entry.isHidden;
                const bool matchesFilter = this->matchesFilter(entry);
                const bool shouldBeVisible = visible && matchesFilter;

                int filteredRow = -1;
                for (int i = 0; i < m_filteredIndices.size(); ++i) {
                    if (m_filteredIndices[i] == absoluteIdx) {
                        filteredRow = i;
                        break;
                    }
                }

                if (shouldBeVisible && filteredRow == -1) {
                    auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), absoluteIdx,
                        [this, &entry](int existingIdx, int val) {
                            Q_UNUSED(val);
                            return this->compareEntries(m_entries.at(existingIdx), entry);
                        });
                    const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
                    beginInsertRows(QModelIndex(), row, row);
                    m_filteredIndices.insert(row, absoluteIdx);
                    endInsertRows();
                } else if (!shouldBeVisible && filteredRow != -1) {
                    beginRemoveRows(QModelIndex(), filteredRow, filteredRow);
                    m_filteredIndices.removeAt(filteredRow);
                    endRemoveRows();
                } else if (shouldBeVisible && filteredRow != -1 && changed) {
                    bool wasSelected = existing.isSelected;
                    existing = entry;
                    existing.isSelected = wasSelected;
                    emit dataChanged(index(filteredRow), index(filteredRow));
                    if (sortOrderChanged) {
                        sortModel();
                    }
                } else if (changed) {
                    bool wasSelected = existing.isSelected;
                    existing = entry;
                    existing.isSelected = wasSelected;
                }
                m_foundPaths.insert(normalizedPath);
            } else {
                const int newAbsoluteIdx = m_entries.size();
                m_entries.append(entry);
                m_pathIndex.insert(normalizedPath, newAbsoluteIdx);
                m_foundPaths.insert(normalizedPath);

                const bool visible = m_showHidden || !entry.isHidden;
                const bool matchesFilter = this->matchesFilter(entry);
                const bool shouldBeVisible = visible && matchesFilter;

                if (shouldBeVisible) {
                    auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
                        [this, &entry](int existingIdx, int) {
                            return this->compareEntries(m_entries.at(existingIdx), entry);
                        });
                    const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
                    beginInsertRows(QModelIndex(), row, row);
                    m_filteredIndices.insert(row, newAbsoluteIdx);
                    endInsertRows();
                }
            }
        }
    }

    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    emit countChanged();
}

void DirectoryModel::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

void DirectoryModel::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

void DirectoryModel::setLastError(const QVariantMap &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

DirectoryModel::SortRole DirectoryModel::sortRole() const
{
    return m_sortRole;
}

void DirectoryModel::setSortRole(SortRole role)
{
    if (m_sortRole == role) {
        return;
    }
    m_sortRole = role;
    sortModel();
    emit sortRoleChanged();
}

Qt::SortOrder DirectoryModel::sortOrder() const
{
    return m_sortOrder;
}

void DirectoryModel::setSortOrder(Qt::SortOrder order)
{
    if (m_sortOrder == order) {
        return;
    }
    m_sortOrder = order;
    sortModel();
    emit sortOrderChanged();
}

bool DirectoryModel::compareEntries(const FileEntry &a, const FileEntry &b) const
{
    if (!m_mixFilesAndFolders && a.isDirectory != b.isDirectory) {
        return a.isDirectory; // Directories always come first unless mixing is enabled
    }

    switch (m_sortRole) {
    case SortByName: {
        int comp = a.name.compare(b.name, Qt::CaseInsensitive);
        if (comp != 0) {
            return m_sortOrder == Qt::AscendingOrder ? (comp < 0) : (comp > 0);
        }
        break;
    }
    case SortBySize: {
        if (a.size != b.size) {
            return m_sortOrder == Qt::AscendingOrder ? (a.size < b.size) : (a.size > b.size);
        }
        break;
    }
    case SortByType: {
        int comp = a.suffix.compare(b.suffix, Qt::CaseInsensitive);
        if (comp != 0) {
            return m_sortOrder == Qt::AscendingOrder ? (comp < 0) : (comp > 0);
        }
        break;
    }
    case SortByDate: {
        if (a.modified != b.modified) {
            return m_sortOrder == Qt::AscendingOrder ? (a.modified < b.modified) : (a.modified > b.modified);
        }
        break;
    }
    case SortByDateCreated: {
        if (a.created != b.created) {
            return m_sortOrder == Qt::AscendingOrder ? (a.created < b.created) : (a.created > b.created);
        }
        break;
    }
    case SortByExtension: {
        int comp = a.suffix.compare(b.suffix, Qt::CaseInsensitive);
        if (comp != 0) {
            return m_sortOrder == Qt::AscendingOrder ? (comp < 0) : (comp > 0);
        }
        int nameComp = a.name.compare(b.name, Qt::CaseInsensitive);
        if (nameComp != 0) {
            return m_sortOrder == Qt::AscendingOrder ? (nameComp < 0) : (nameComp > 0);
        }
        break;
    }
    }

    int nameComp = a.name.compare(b.name, Qt::CaseInsensitive);
    if (nameComp != 0) {
        return m_sortOrder == Qt::AscendingOrder ? (nameComp < 0) : (nameComp > 0);
    }
    const int pathComp = a.path.compare(b.path, Qt::CaseInsensitive);
    return m_sortOrder == Qt::AscendingOrder ? (pathComp < 0) : (pathComp > 0);
}

void DirectoryModel::sortModel()
{
    if (m_filteredIndices.isEmpty()) {
        return;
    }

    emit layoutAboutToBeChanged();
    std::stable_sort(m_filteredIndices.begin(), m_filteredIndices.end(),
        [this](int aIdx, int bIdx) {
            return compareEntries(m_entries.at(aIdx), m_entries.at(bIdx));
        });
    emit layoutChanged();
}
