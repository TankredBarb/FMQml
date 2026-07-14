#include "DirectoryModel.h"
#include "DirectoryModelSemanticRoles.h"
#include "DirectoryModelAlgorithms.h"
#include "DirectoryWatchPolicy.h"

#include "../core/ArchiveSupport.h"
#include "../core/DriveUtils.h"
#include "../core/FileAccessResolver.h"
#include "../core/FileError.h"
#include "../core/FileProviderFactory.h"
#include "../core/IsoSupport.h"
#include "../core/LocalFileProvider.h"
#include "../core/LocalFileBadgeResolver.h"
#include "../core/LocalMountPointIndex.h"
#include "../core/FavoritesStore.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHash>
#include <QLocale>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QtGlobal>
#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "DirectoryModelInternal.h"

namespace DirectoryModelInternal {
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

#ifdef Q_OS_WIN
DWORD entryAttributesWindows(const QFileInfo &fileInfo)
{
    const QString nativePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    return GetFileAttributesW(reinterpret_cast<LPCWSTR>(nativePath.utf16()));
}
#endif

FileEntry entryFromInfo(const QFileInfo &fileInfo)
{
    FileEntry entry;
    entry.name = fileInfo.fileName();
    entry.path = fileInfo.absoluteFilePath();
    entry.suffix = fileInfo.suffix();
    entry.size = fileInfo.size();
    entry.modified = fileInfo.lastModified();
    entry.created = fileInfo.birthTime().isValid() ? fileInfo.birthTime() : fileInfo.lastModified();
#ifdef Q_OS_WIN
    const DWORD attributes = entryAttributesWindows(fileInfo);
    const bool hasNativeAttributes = attributes != INVALID_FILE_ATTRIBUTES;
    const bool isDirectory = hasNativeAttributes
        ? ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        : fileInfo.isDir();
    const bool isHidden = hasNativeAttributes
        ? ((attributes & FILE_ATTRIBUTE_HIDDEN) != 0 || entry.name.startsWith(QLatin1Char('.')))
        : (fileInfo.isHidden() || entry.name.startsWith(QLatin1Char('.')));
    const bool isReadOnly = hasNativeAttributes
        ? ((attributes & FILE_ATTRIBUTE_READONLY) != 0)
        : !fileInfo.isWritable();
    const bool isSystem = hasNativeAttributes
        ? ((attributes & FILE_ATTRIBUTE_SYSTEM) != 0)
        : false;
    const bool isLink = hasNativeAttributes
        ? ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        : fileInfo.isSymLink();
    entry.isDirectory = isDirectory;
    entry.isHidden = isHidden;
    entry.isReadOnly = isReadOnly;
    entry.isSystem = isSystem;
#else
    entry.isDirectory = fileInfo.isDir();
    entry.isHidden = fileInfo.isHidden() || fileInfo.fileName().startsWith(QLatin1Char('.'));
    entry.isReadOnly = !fileInfo.isWritable();
    entry.isSystem = false;
    const bool isLink = fileInfo.isSymLink();
#endif

    QLocale loc;
    entry.sizeText = entry.isDirectory
        ? QString()
        : DriveUtils::formatSize(entry.size);
    entry.modifiedText = loc.toString(entry.modified, QLocale::ShortFormat);
    entry.createdText  = loc.toString(entry.created,  QLocale::ShortFormat);

    // Build attributes string
    QString attrs;
    if (entry.isDirectory) attrs += QLatin1Char('D');
    if (entry.isHidden)    attrs += QLatin1Char('H');
    if (entry.isReadOnly)  attrs += QLatin1Char('R');
    if (entry.isSystem)    attrs += QLatin1Char('S');
    if (isLink)            attrs += QLatin1Char('L');
    entry.attributesText = attrs;

    const LocalFileBadgeState badgeState = LocalFileBadgeResolver::resolve(fileInfo, isLink);
    entry.isSymLink = badgeState.isSymLink;
    entry.isBrokenSymLink = badgeState.isBrokenSymLink;
    entry.isLocked = badgeState.isLocked;
    entry.primaryBadgeKind = badgeState.primaryBadgeKind;

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
        || a.providerCapabilitiesText != b.providerCapabilitiesText
        || a.iconName != b.iconName
        || a.overlayIconName != b.overlayIconName
        || a.mimeType != b.mimeType
        || a.shortcutOpenPath != b.shortcutOpenPath
        || a.shortcutTargetPath != b.shortcutTargetPath
        || a.shortcutTargetMimeType != b.shortcutTargetMimeType
        || a.shortcutTargetResourceKey != b.shortcutTargetResourceKey
        || a.shortcutTargetIsDirectory != b.shortcutTargetIsDirectory
        || a.isDirectory != b.isDirectory
        || a.isHidden != b.isHidden
        || a.isImage != b.isImage
        || a.hasThumbnail != b.hasThumbnail
        || a.isReadOnly != b.isReadOnly
        || a.isLocked != b.isLocked
        || a.isSymLink != b.isSymLink
        || a.isBrokenSymLink != b.isBrokenSymLink
        || a.isMountPoint != b.isMountPoint
        || a.isPinned != b.isPinned
        || a.isSystem != b.isSystem
        || a.primaryBadgeKind != b.primaryBadgeKind
        || a.isShortcut != b.isShortcut
        || a.specialAction != b.specialAction
        || a.iconRecolorAllowed != b.iconRecolorAllowed;
}

bool thumbnailIdentityChanged(const FileEntry &a, const FileEntry &b)
{
    return a.path != b.path || a.size != b.size || a.modified != b.modified;
}

bool watchDebugEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_WATCH_DEBUG");
    return enabled;
}

void traceDirectoryWatch(const char *stage, const QString &path, const QString &detail)
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
    const QString normalizedLeft = QDir::cleanPath(QDir::fromNativeSeparators(left));
    const QString normalizedRight = QDir::cleanPath(QDir::fromNativeSeparators(right));
#ifdef Q_OS_WIN
    return normalizedLeft.compare(normalizedRight, Qt::CaseInsensitive) == 0;
#else
    return normalizedLeft == normalizedRight;
#endif
}

bool isUriPath(const QString &path)
{
    const int separatorIndex = path.indexOf(QStringLiteral("://"));
    return separatorIndex > 0;
}

QString modelPathKey(const QString &path)
{
    return DirectoryModelAlgorithms::pathKey(path);
}

bool isProviderEntryPath(const QString &path)
{
    const int separatorIndex = path.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return false;
    }
    const QString scheme = path.left(separatorIndex).toLower();
    return scheme != QStringLiteral("file")
        && scheme != QStringLiteral("archive")
        && scheme != QStringLiteral("devices")
        && scheme != QStringLiteral("favorites");
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

bool directoryNavTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_NAV_TRACE");
    return enabled;
}

void traceDirectoryNav(const char *stage, const QString &path, const QString &detail)
{
    if (!directoryNavTraceEnabled()) {
        return;
    }

    qInfo().noquote() << "[FM_NAV][directory-model]" << stage
                      << "path=" << QDir::toNativeSeparators(path)
                      << detail;
}

bool scannerFailureIndicatesUnavailable(const QString &error)
{
    const QString lower = error.toLower();
    return lower.contains(QStringLiteral("does not exist"))
        || lower.contains(QStringLiteral("no longer available"))
        || lower.contains(QStringLiteral("not found"));
}

QString failedNavigationSelectionPath(const QString &failedPath)
{
    if (!ArchiveSupport::isArchivePath(failedPath)) {
        return failedPath;
    }

    const QString normalized = ArchiveSupport::normalizeArchivePath(failedPath);
    const QStringList tokens = ArchiveSupport::splitArchiveTokens(normalized);
    if (tokens.size() == 2 && tokens.last() == QLatin1String("/")) {
        return ArchiveSupport::physicalArchivePath(normalized);
    }
    if (tokens.size() > 2 && tokens.last() == QLatin1String("/")) {
        return QStringLiteral("archive://") + tokens.mid(0, tokens.size() - 1).join(QLatin1Char('|'));
    }
    return normalized;
}


bool entryMatchesFilterSnapshot(const FileEntry &entry,
                                const QString &searchText,
                                DirectoryModel::CategoryFilter categoryFilter)
{
    return DirectoryModelAlgorithms::matchesFilter(entry, searchText, categoryFilter);
}

bool compareEntriesForPolicy(const FileEntry &a,
                             const FileEntry &b,
                             bool mixFilesAndFolders,
                             DirectoryModel::SortRole sortRole,
                             Qt::SortOrder sortOrder)
{
    return DirectoryModelAlgorithms::lessThan(
        a, b, mixFilesAndFolders, sortRole, sortOrder);
}

AsyncFreshLoadResult buildAsyncFreshLoadResult(int generation,
                                               const QString &path,
                                               QList<FileEntry> baseEntries,
                                               QList<FileEntry> pendingEntries,
                                               qsizetype pendingOffset,
                                               bool showHidden,
                                               const QString &searchText,
                                               DirectoryModel::CategoryFilter categoryFilter,
                                               bool mixFilesAndFolders,
                                               DirectoryModel::SortRole sortRole,
                                               Qt::SortOrder sortOrder)
{
    AsyncFreshLoadResult result;
    result.generation = generation;
    result.path = path;
    result.showHidden = showHidden;
    result.searchText = searchText;
    result.categoryFilter = categoryFilter;
    result.mixFilesAndFolders = mixFilesAndFolders;
    result.sortRole = sortRole;
    result.sortOrder = sortOrder;

    const qsizetype normalizedOffset = std::clamp(pendingOffset, qsizetype(0), pendingEntries.size());
    result.entries.reserve(baseEntries.size() + pendingEntries.size() - normalizedOffset);
    result.pathIndex.reserve(baseEntries.size() + pendingEntries.size() - normalizedOffset);
    result.foundPaths.reserve(baseEntries.size() + pendingEntries.size() - normalizedOffset);

    auto appendEntry = [&result](FileEntry entry) {
        const QString normalizedPath = modelPathKey(entry.path);
        result.foundPaths.insert(normalizedPath);
        if (result.pathIndex.contains(normalizedPath)) {
            return;
        }

        entry.isSelected = false;
        const int newAbsoluteIdx = result.entries.size();
        result.entries.append(std::move(entry));
        result.pathIndex.insert(normalizedPath, newAbsoluteIdx);
    };

    for (FileEntry &entry : baseEntries) {
        appendEntry(std::move(entry));
    }
    for (qsizetype i = normalizedOffset; i < pendingEntries.size(); ++i) {
        appendEntry(std::move(pendingEntries[i]));
    }

    result.filteredIndices = DirectoryModelAlgorithms::filteredAndSortedIndices(
        result.entries,
        showHidden,
        searchText,
        categoryFilter,
        mixFilesAndFolders,
        sortRole,
        sortOrder);

    return result;
}
} // namespace DirectoryModelInternal

using namespace DirectoryModelInternal;

DirectoryModel::DirectoryModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_provider(std::make_unique<LocalFileProvider>())
    , m_changeWatcher(createDirectoryChangeWatcher())
    , m_parentChangeWatcher(createDirectoryChangeWatcher())
{
    connect(m_provider.get(), &FileProvider::started, this, &DirectoryModel::onScannerStarted);
    connect(m_provider.get(), &FileProvider::batchReady, this, &DirectoryModel::onScannerBatchReady);
    connect(m_provider.get(), &FileProvider::progress, this, &DirectoryModel::onScannerProgress);
    connect(m_provider.get(), &FileProvider::statusMessage, this, &DirectoryModel::providerStatusMessage);
    connect(m_provider.get(), &FileProvider::finished, this, &DirectoryModel::onScannerFinished);
    connect(m_changeWatcher.get(), &DirectoryChangeWatcher::eventsReady,
            this, &DirectoryModel::onDirectoryEventsReady);
    connect(m_changeWatcher.get(), &DirectoryChangeWatcher::watchFailed,
            this, &DirectoryModel::onDirectoryWatchFailed);
    connect(m_parentChangeWatcher.get(), &DirectoryChangeWatcher::eventsReady,
            this, &DirectoryModel::onParentDirectoryEventsReady);
    connect(m_parentChangeWatcher.get(), &DirectoryChangeWatcher::watchFailed,
            this, &DirectoryModel::onParentDirectoryWatchFailed);

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
    const QVariant semanticValue = directoryModelSemanticRoleValue(entry, role);
    if (semanticValue.isValid()) return semanticValue;
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
    case IsReadOnlyRole:
        return entry.isReadOnly;
    case IsLockedRole:
        return entry.isLocked;
    case IsSymLinkRole:
        return entry.isSymLink;
    case IsBrokenSymLinkRole:
        return entry.isBrokenSymLink;
    case IsMountPointRole:
        return entry.isMountPoint;
    case PrimaryBadgeKindRole:
        return entry.primaryBadgeKind;
    case IsPinnedRole:
        return entry.isPinned;
    case IsArchiveFileRole:
        return !entry.isDirectory && ArchiveSupport::isArchiveExtension(entry.suffix);
    case IsIsoImageFileRole:
        return !entry.isDirectory && IsoSupport::isIsoImageExtension(entry.suffix);
    case IsShortcutRole:
        return entry.isShortcut;
    case ShortcutTargetPathRole:
        return entry.shortcutTargetPath;
    case ShortcutTargetIsDirectoryRole:
        return entry.shortcutTargetIsDirectory;
    case MimeTypeRole:
        return entry.mimeType;
    case ThumbnailRevisionRole:
        return m_thumbnailRevisions.value(entry.path, 0);
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
        {IsReadOnlyRole, "isReadOnly"},
        {IsLockedRole, "isLocked"},
        {IsSymLinkRole, "isSymLink"},
        {IsBrokenSymLinkRole, "isBrokenSymLink"},
        {IsMountPointRole, "isMountPoint"},
        {PrimaryBadgeKindRole, "primaryBadgeKind"},
        {IsPinnedRole, "isPinned"},
        {IsArchiveFileRole, "isArchiveFile"},
        {IsIsoImageFileRole, "isIsoImageFile"},
        {IsShortcutRole, "isShortcut"},
        {ShortcutTargetPathRole, "shortcutTargetPath"},
        {ShortcutTargetIsDirectoryRole, "shortcutTargetIsDirectory"},
        {MimeTypeRole, "mimeType"},
        {ThumbnailRevisionRole, "thumbnailRevision"},
        {SpecialActionRole, "specialAction"},
        {OverlayIconNameRole, "overlayIconName"},
        {IconRecolorAllowedRole, "iconRecolorAllowed"},
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

double DirectoryModel::scanProgress() const
{
    return m_scanProgress;
}

QString DirectoryModel::scanProgressText() const
{
    return m_scanProgressText;
}

int DirectoryModel::count() const
{
    return m_filteredIndices.size();
}

int DirectoryModel::selectedCount() const
{
    return m_selectedCount;
}

int DirectoryModel::firstSelectedRow() const
{
    for (int row = 0; row < m_filteredIndices.size(); ++row) {
        if (m_entries.at(m_filteredIndices.at(row)).isSelected) {
            return row;
        }
    }
    return -1;
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

    if (m_loading) {
        const QString reloadPath = !m_pendingFreshLoadPath.isEmpty()
            ? m_pendingFreshLoadPath
            : (m_provider ? m_provider->currentPath() : QString{});
        if (!reloadPath.isEmpty()) {
            m_insertTimer.stop();
            m_pendingInserts.clear();
            m_pendingInsertOffset = 0;
            m_pendingScannerFinish = false;
            m_pendingScannerPath.clear();
            m_pendingScannerError.clear();
            m_pendingScannerSuccess = false;
            m_provider->scan(reloadPath);
            emit showHiddenChanged();
            return;
        }
    }
    
    // Immediately update the filtered indices for items we already have.
    applyFilterInternal(true);
    
    refresh();
    emit showHiddenChanged();
}
