#include "DirectoryModel.h"

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
    QStringLiteral("epub"),
    QStringLiteral("fb2")
};

#ifdef Q_OS_WIN
DWORD entryAttributesWindows(const QFileInfo &fileInfo)
{
    const QString nativePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    return GetFileAttributesW(reinterpret_cast<LPCWSTR>(nativePath.utf16()));
}
#endif

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
        || a.isShortcut != b.isShortcut;
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
    QString key = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    key = key.toLower();
#endif
    return key;
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

bool eventSourceMatches(const DirectoryChangeEvent &event, const QString &watchPath)
{
    return event.sourcePath.isEmpty()
        || sameFilesystemPath(QDir::fromNativeSeparators(event.sourcePath),
                              QDir::fromNativeSeparators(watchPath));
}

bool isPartStagingPath(const QString &path)
{
    return modelPathKey(path).endsWith(QStringLiteral(".part"));
}

bool isTransientPartWriteEvent(const DirectoryChangeEvent &event)
{
    return event.type == DirectoryChangeEvent::Type::Modified
        && !event.path.isEmpty()
        && isPartStagingPath(event.path);
}

QString directoryEventCoalescingKey(const DirectoryChangeEvent &event)
{
    switch (event.type) {
    case DirectoryChangeEvent::Type::Added:
    case DirectoryChangeEvent::Type::Modified:
    case DirectoryChangeEvent::Type::Removed:
        return event.path.isEmpty()
            ? QString{}
            : QStringLiteral("path:") + modelPathKey(event.path);
    case DirectoryChangeEvent::Type::Renamed:
        return QStringLiteral("rename:")
            + modelPathKey(event.oldPath)
            + QStringLiteral("->")
            + modelPathKey(event.newPath);
    case DirectoryChangeEvent::Type::Overflow:
        return QStringLiteral("overflow:") + modelPathKey(event.path);
    }
    return {};
}

void appendCoalescedDirectoryEvent(QList<DirectoryChangeEvent> &pending,
                                   const DirectoryChangeEvent &event)
{
    if (event.type == DirectoryChangeEvent::Type::Overflow) {
        pending.clear();
        pending.append(event);
        return;
    }

    if (event.type == DirectoryChangeEvent::Type::Renamed) {
        pending.append(event);
        return;
    }

    const QString key = directoryEventCoalescingKey(event);
    if (key.isEmpty()) {
        pending.append(event);
        return;
    }

    for (int i = pending.size() - 1; i >= 0; --i) {
        const DirectoryChangeEvent &existing = pending.at(i);
        if (existing.type == DirectoryChangeEvent::Type::Renamed
            || existing.type == DirectoryChangeEvent::Type::Overflow) {
            continue;
        }
        if (directoryEventCoalescingKey(existing) == key) {
            pending[i] = event;
            return;
        }
    }

    pending.append(event);
}

bool directoryNavTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_NAV_TRACE");
    return enabled;
}

void traceDirectoryNav(const char *stage, const QString &path = {}, const QString &detail = {})
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

struct AsyncFreshLoadResult {
    int generation = 0;
    QString path;
    QList<FileEntry> entries;
    QList<int> filteredIndices;
    QHash<QString, int> pathIndex;
    QSet<QString> foundPaths;
    bool showHidden = false;
    bool mixFilesAndFolders = false;
    QString searchText;
    DirectoryModel::CategoryFilter categoryFilter = DirectoryModel::FilterAll;
    DirectoryModel::SortRole sortRole = DirectoryModel::SortByName;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
};

bool entryMatchesFilterSnapshot(const FileEntry &entry,
                                const QString &searchText,
                                DirectoryModel::CategoryFilter categoryFilter)
{
    if (!searchText.isEmpty()
        && !entry.name.contains(searchText, Qt::CaseInsensitive)) {
        return false;
    }

    if (categoryFilter == DirectoryModel::FilterAll) {
        return true;
    }

    if (entry.isDirectory) {
        return false;
    }

    const QString suffix = entry.suffix.toLower();
    switch (categoryFilter) {
    case DirectoryModel::FilterExecutables:
        return kExecutableSuffixes.contains(suffix);
    case DirectoryModel::FilterLibraries:
        return kLibrarySuffixes.contains(suffix);
    case DirectoryModel::FilterImages:
        return kImageSuffixes.contains(suffix);
    case DirectoryModel::FilterArchives:
        return ArchiveSupport::isArchiveExtension(suffix) || IsoSupport::isIsoImageExtension(suffix);
    case DirectoryModel::FilterMedia:
        return kAudioSuffixes.contains(suffix) || kVideoSuffixes.contains(suffix);
    case DirectoryModel::FilterDocuments:
        return kDocumentSuffixes.contains(suffix)
            || entry.name.endsWith(QStringLiteral(".fb2.zip"), Qt::CaseInsensitive);
    case DirectoryModel::FilterAll:
        break;
    }

    return true;
}

bool compareEntriesForPolicy(const FileEntry &a,
                             const FileEntry &b,
                             bool mixFilesAndFolders,
                             DirectoryModel::SortRole sortRole,
                             Qt::SortOrder sortOrder)
{
    const bool aLoadMore = (a.path.startsWith(QStringLiteral("instagram://"), Qt::CaseInsensitive)
            || a.path.startsWith(QStringLiteral("telegram://"), Qt::CaseInsensitive))
        && a.path.endsWith(QStringLiteral("/__load_more__"));
    const bool bLoadMore = (b.path.startsWith(QStringLiteral("instagram://"), Qt::CaseInsensitive)
            || b.path.startsWith(QStringLiteral("telegram://"), Qt::CaseInsensitive))
        && b.path.endsWith(QStringLiteral("/__load_more__"));
    if (aLoadMore != bLoadMore) {
        return !aLoadMore;
    }

    if (!mixFilesAndFolders && a.isDirectory != b.isDirectory) {
        return a.isDirectory;
    }

    switch (sortRole) {
    case DirectoryModel::SortByName: {
        const int comp = a.name.compare(b.name, Qt::CaseInsensitive);
        if (comp != 0) {
            return sortOrder == Qt::AscendingOrder ? (comp < 0) : (comp > 0);
        }
        break;
    }
    case DirectoryModel::SortBySize:
        if (a.size != b.size) {
            return sortOrder == Qt::AscendingOrder ? (a.size < b.size) : (a.size > b.size);
        }
        break;
    case DirectoryModel::SortByType: {
        const int comp = a.suffix.compare(b.suffix, Qt::CaseInsensitive);
        if (comp != 0) {
            return sortOrder == Qt::AscendingOrder ? (comp < 0) : (comp > 0);
        }
        break;
    }
    case DirectoryModel::SortByDate:
        if (a.modified != b.modified) {
            return sortOrder == Qt::AscendingOrder ? (a.modified < b.modified) : (a.modified > b.modified);
        }
        break;
    case DirectoryModel::SortByDateCreated:
        if (a.created != b.created) {
            return sortOrder == Qt::AscendingOrder ? (a.created < b.created) : (a.created > b.created);
        }
        break;
    case DirectoryModel::SortByExtension: {
        const int comp = a.suffix.compare(b.suffix, Qt::CaseInsensitive);
        if (comp != 0) {
            return sortOrder == Qt::AscendingOrder ? (comp < 0) : (comp > 0);
        }
        const int nameComp = a.name.compare(b.name, Qt::CaseInsensitive);
        if (nameComp != 0) {
            return sortOrder == Qt::AscendingOrder ? (nameComp < 0) : (nameComp > 0);
        }
        break;
    }
    }

    const int nameComp = a.name.compare(b.name, Qt::CaseInsensitive);
    if (nameComp != 0) {
        return sortOrder == Qt::AscendingOrder ? (nameComp < 0) : (nameComp > 0);
    }
    const int pathComp = a.path.compare(b.path, Qt::CaseInsensitive);
    return sortOrder == Qt::AscendingOrder ? (pathComp < 0) : (pathComp > 0);
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

    result.filteredIndices.reserve(result.entries.size());
    for (int i = 0; i < result.entries.size(); ++i) {
        const FileEntry &entry = result.entries.at(i);
        if ((showHidden || !entry.isHidden)
                && entryMatchesFilterSnapshot(entry, searchText, categoryFilter)) {
            result.filteredIndices.append(i);
        }
    }

    std::sort(result.filteredIndices.begin(), result.filteredIndices.end(),
        [&result](int aIdx, int bIdx) {
            return compareEntriesForPolicy(result.entries.at(aIdx),
                                           result.entries.at(bIdx),
                                           result.mixFilesAndFolders,
                                           result.sortRole,
                                           result.sortOrder);
        });

    return result;
}
}

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

bool DirectoryModel::openPath(const QString &path)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    traceDirectoryNav("openPath-begin", path,
                      QStringLiteral("current=%1 provider=%2")
                          .arg(QDir::toNativeSeparators(m_currentPath),
                               m_provider ? m_provider->scheme() : QStringLiteral("<none>")));

    if (path.isEmpty()) {
        traceDirectoryNav("openPath-end", path,
                          QStringLiteral("result=false reason=empty elapsedMs=%1").arg(totalTimer.elapsed()));
        return false;
    }
    const bool archivePath = ArchiveSupport::isArchivePath(path);
    std::unique_ptr<FileProvider> targetProvider = FileProviderFactory::createProvider(path);
    const QString targetScheme = targetProvider ? targetProvider->scheme() : QStringLiteral("<none>");
    const QString normalizedPath = targetProvider
        ? targetProvider->normalizedPath(path)
        : FileProviderFactory::normalizePath(path);
    traceDirectoryNav("openPath-normalized", normalizedPath,
                      QStringLiteral("archivePath=%1 targetProvider=%2 elapsedMs=%3")
                          .arg(archivePath)
                          .arg(targetScheme)
                          .arg(totalTimer.elapsed()));
    if (normalizedPath.isEmpty()) {
        traceDirectoryNav("openPath-end", path,
                          QStringLiteral("result=false reason=normalize elapsedMs=%1").arg(totalTimer.elapsed()));
        return false;
    }
    if (!m_provider || !m_provider->canHandle(normalizedPath)) {
        traceDirectoryNav("openPath-replaceProvider", normalizedPath,
                          QStringLiteral("reason=canHandle targetProvider=%1 elapsedMs=%2")
                              .arg(targetScheme)
                              .arg(totalTimer.elapsed()));
        if (targetProvider) {
            replaceProvider(std::move(targetProvider));
        }
    }
    if (!m_provider || !m_provider->canHandle(normalizedPath)) {
        traceDirectoryNav("openPath-end", normalizedPath,
                          QStringLiteral("result=false reason=no-provider elapsedMs=%1").arg(totalTimer.elapsed()));
        return false;
    }
    m_provider->setShowHidden(m_showHidden);
    const bool pathChanged = isUriPath(normalizedPath) || isUriPath(m_currentPath)
        ? normalizedPath != m_currentPath
        : !sameFilesystemPath(QDir::fromNativeSeparators(normalizedPath),
                              QDir::fromNativeSeparators(m_currentPath));
    if (pathChanged) {
        m_insertTimer.stop();
        m_pendingInserts.clear();
        m_pendingInsertOffset = 0;
        m_pendingScannerFinish = false;
        m_pendingScannerPath.clear();
        m_pendingScannerError.clear();
        m_pendingScannerSuccess = false;
        m_foundPaths.clear();
        traceDirectoryNav("openPath-deferFreshReset", normalizedPath,
                          QStringLiteral("totalMs=%1").arg(totalTimer.elapsed()));
    }
    QElapsedTimer scanTimer;
    scanTimer.start();
    m_provider->scan(normalizedPath);
    traceDirectoryNav("openPath-provider.scan-returned", normalizedPath,
                      QStringLiteral("pathChanged=%1 scanCallMs=%2 totalMs=%3 generation=%4")
                          .arg(pathChanged)
                          .arg(scanTimer.elapsed())
                          .arg(totalTimer.elapsed())
                          .arg(m_provider->currentGeneration()));
    return true;
}

void DirectoryModel::cancelLoading()
{
    if (!m_loading || !m_provider) {
        return;
    }

    m_provider->cancel();
    m_currentScanGeneration = m_provider->currentGeneration();
    m_insertTimer.stop();
    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;
    setScanProgress(-1.0);
    setLoading(false);
    setError(QStringLiteral("Archive preparation was cancelled"));
}

void DirectoryModel::clear()
{
    if (m_provider) {
        m_provider->cancel();
    }
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
    m_foundPaths.clear();
    m_previousPath.clear();
    m_pendingFreshLoadPath.clear();
    m_freshLoadCommitted = true;
    m_currentScanGeneration = 0;
    m_selectedCount = 0;

    if (!m_currentPath.isEmpty() && !ArchiveSupport::isArchivePath(m_currentPath)) {
        m_changeWatcher->stop();
        m_parentChangeWatcher->stop();
    }

    emit visualStructureAboutToChange();
    beginResetModel();
    m_entries.clear();
    m_filteredIndices.clear();
    m_pathIndex.clear();
    m_currentPath.clear();
    endResetModel();

    setLoading(false);
    setError({});
    setLastError({});
    setScanProgress(-1.0);
    emit currentPathChanged();
    emit countChanged();
    emit selectionChanged();
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
    connect(m_provider.get(), &FileProvider::progress, this, &DirectoryModel::onScannerProgress);
    connect(m_provider.get(), &FileProvider::statusMessage, this, &DirectoryModel::providerStatusMessage);
    connect(m_provider.get(), &FileProvider::finished, this, &DirectoryModel::onScannerFinished);
    m_provider->setShowHidden(m_showHidden);
}

void DirectoryModel::onScannerStarted()
{
    QElapsedTimer totalTimer;
    totalTimer.start();
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
        m_pendingFreshLoadPath = scanPath;
        m_freshLoadCommitted = false;
    } else {
        m_pendingFreshLoadPath.clear();
        m_freshLoadCommitted = true;
    }

    setLoading(true);
    setError({});
    setLastError({});
    setScanProgress(-1.0);
    emit countChanged();
    emit selectionChanged();
    traceDirectoryNav("scannerStarted-end", scanPath,
                      QStringLiteral("previous=%1 fresh=%2 generation=%3 elapsedMs=%4")
                          .arg(QDir::toNativeSeparators(previousPath))
                          .arg(m_freshLoad)
                          .arg(m_currentScanGeneration)
                          .arg(totalTimer.elapsed()));
}

void DirectoryModel::onScannerBatchReady(const QList<FileEntry> &entries, int generation)
{
    if (generation != m_currentScanGeneration) {
        return;
    }

    if (entries.isEmpty()) {
        return;
    }

    QList<FileEntry> pinnedEntries = entries;
    for (FileEntry &entry : pinnedEntries) {
        entry.isPinned = m_pinnedPathKeys.contains(FavoritesStore::normalizedPathKey(entry.path));
    }
    m_pendingInserts.append(pinnedEntries);
    if (m_freshLoad && m_provider && m_provider->scheme() == QStringLiteral("file")) {
        if (!m_freshLoadCommitted) {
            commitFreshLoad(m_pendingFreshLoadPath);
        }
        return;
    }
    if (!m_insertTimer.isActive()) {
        m_insertTimer.start();
    }
}

void DirectoryModel::onScannerProgress(qint64 processedBytes, qint64 totalBytes, const QString &message, int generation)
{
    if (generation != m_currentScanGeneration || totalBytes <= 0) {
        return;
    }

    const double progress = std::clamp(
        static_cast<double>(processedBytes) / static_cast<double>(totalBytes),
        0.0,
        1.0);
    const QString text = message.isEmpty()
        ? QStringLiteral("%1%").arg(qRound(progress * 100.0))
        : QStringLiteral("%1 %2%").arg(message).arg(qRound(progress * 100.0));
    setScanProgress(progress, text);
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
    if (m_freshLoad && !m_freshLoadCommitted) {
        commitFreshLoad(m_pendingFreshLoadPath);
    }

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
                emit visualStructureAboutToChange();
                beginInsertRows(QModelIndex(), row, row);
                m_filteredIndices.insert(row, absoluteIdx);
                endInsertRows();
            } else if (!shouldBeVisible && filteredRow != -1) {
                emit visualStructureAboutToChange();
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
                emit visualStructureAboutToChange();
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
        && m_freshLoad
        && m_provider
        && m_provider->scheme() == QStringLiteral("file")
        && pendingCount >= AsyncFreshLoadThreshold) {
        startAsyncFreshLoad(path);
        return;
    }

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
    QElapsedTimer totalTimer;
    totalTimer.start();
    traceDirectoryNav("finalize-begin", path,
                      QStringLiteral("success=%1 fresh=%2 entries=%3 filtered=%4 error=%5")
                          .arg(success)
                          .arg(m_freshLoad)
                          .arg(m_entries.size())
                          .arg(m_filteredIndices.size())
                          .arg(error));
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;

    setLoading(false);
    setScanProgress(-1.0);
    if (success) {
        if (m_freshLoad && !m_freshLoadCommitted) {
            commitFreshLoad(path);
        }
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
                        emit visualStructureAboutToChange();
                        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
                        m_filteredIndices.removeAt(filteredIdx);
                        m_entries.removeAt(i);
                        for (int &idx : m_filteredIndices) {
                            if (idx > i) idx--;
                        }
                        endRemoveRows();
                    } else {
                        m_entries.removeAt(i);
                        for (int &idx : m_filteredIndices) {
                            if (idx > i) idx--;
                        }
                    }
                }
            }
            updatePathIndex();
            emit selectionChanged();
        }
        emit countChanged();
        if (m_freshLoad) {
            QElapsedTimer watchTimer;
            watchTimer.start();
            restartChangeWatcherForCurrentPath();
            traceDirectoryNav("finalize-watch-restart", path,
                              QStringLiteral("elapsedMs=%1").arg(watchTimer.elapsed()));
        }
        if (m_deferredWatchRestartPending
            && sameFilesystemPath(QDir::fromNativeSeparators(path),
                                  QDir::fromNativeSeparators(m_deferredWatchRestartPath))) {
            scheduleDeferredWatchRestart();
        }
    } else {
        if (sameFilesystemPath(QDir::fromNativeSeparators(path), QDir::fromNativeSeparators(m_currentPath))
            && scannerFailureIndicatesUnavailable(error)) {
            notifyCurrentPathUnavailable(error);
            m_previousPath.clear();
            return;
        }

        if (m_freshLoad) {
            if (!m_freshLoadCommitted) {
                m_currentPath = m_previousPath;
                emit currentPathChanged();
            }
            selectFailedNavigationTarget(path);
            restoreProviderForCurrentPathLater();
        }
        setError(error);
        setLastError(FileError::classify(error, path, QStringLiteral("open")));
        emit directoryUnavailable(path, error);
    }
    m_previousPath.clear();
    m_pendingFreshLoadPath.clear();
    m_freshLoadCommitted = true;
    traceDirectoryNav("finalize-end", path,
                      QStringLiteral("success=%1 elapsedMs=%2 entries=%3 filtered=%4 loading=%5")
                          .arg(success)
                          .arg(totalTimer.elapsed())
                          .arg(m_entries.size())
                          .arg(m_filteredIndices.size())
                          .arg(m_loading));
}

void DirectoryModel::commitFreshLoad(const QString &path)
{
    if (!m_freshLoad || m_freshLoadCommitted) {
        return;
    }

    const QString targetPath = path.isEmpty() ? m_pendingFreshLoadPath : path;
    m_changeWatcher->stop();
    m_parentChangeWatcher->stop();
    emit visualStructureAboutToChange();
    beginResetModel();
    m_entries.clear();
    m_filteredIndices.clear();
    m_pathIndex.clear();
    m_selectedCount = 0;
    m_currentPath = targetPath;
    endResetModel();

    m_freshLoadCommitted = true;
    m_pendingFreshLoadPath.clear();
    emit currentPathChanged();
    emit countChanged();
    emit selectionChanged();
}

void DirectoryModel::startAsyncFreshLoad(const QString &path)
{
    const int generation = m_currentScanGeneration;
    const bool showHidden = m_showHidden;
    const bool mixFilesAndFolders = m_mixFilesAndFolders;
    const QString searchText = m_searchText;
    const CategoryFilter categoryFilter = m_categoryFilter;
    const SortRole sortRole = m_sortRole;
    const Qt::SortOrder sortOrder = m_sortOrder;

    QList<FileEntry> baseEntries;
    if (m_freshLoadCommitted) {
        baseEntries = m_entries;
    }

    QList<FileEntry> pendingEntries = std::move(m_pendingInserts);
    const qsizetype pendingOffset = m_pendingInsertOffset;
    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    m_insertTimer.stop();
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;

    if (!m_freshLoadCommitted) {
        commitFreshLoad(path);
    }

    auto *watcher = new QFutureWatcher<AsyncFreshLoadResult>(this);
    connect(watcher, &QFutureWatcher<AsyncFreshLoadResult>::finished, this, [this, watcher]() {
        AsyncFreshLoadResult result = watcher->result();
        watcher->deleteLater();

        if (result.generation != m_currentScanGeneration || !m_freshLoad) {
            return;
        }
        if (!sameFilesystemPath(QDir::fromNativeSeparators(result.path),
                                QDir::fromNativeSeparators(m_currentPath))) {
            return;
        }

        const bool policyChanged = result.showHidden != m_showHidden
            || result.mixFilesAndFolders != m_mixFilesAndFolders
            || result.searchText != m_searchText
            || result.categoryFilter != m_categoryFilter
            || result.sortRole != m_sortRole
            || result.sortOrder != m_sortOrder;
        if (policyChanged) {
            m_pendingInserts = std::move(result.entries);
            m_pendingInsertOffset = 0;
            startAsyncFreshLoad(result.path);
            return;
        }

        emit visualStructureAboutToChange();
        beginResetModel();
        m_entries = std::move(result.entries);
        m_filteredIndices = std::move(result.filteredIndices);
        m_pathIndex = std::move(result.pathIndex);
        m_foundPaths = std::move(result.foundPaths);
        m_selectedCount = 0;
        endResetModel();

        emit countChanged();
        emit selectionChanged();
        finalizeScannerFinished(result.path, true, {});
    });

    watcher->setFuture(QtConcurrent::run([generation,
                                          path,
                                          baseEntries = std::move(baseEntries),
                                          pendingEntries = std::move(pendingEntries),
                                          pendingOffset,
                                          showHidden,
                                          searchText,
                                          categoryFilter,
                                          mixFilesAndFolders,
                                          sortRole,
                                          sortOrder]() mutable {
        return buildAsyncFreshLoadResult(generation,
                                         path,
                                         std::move(baseEntries),
                                         std::move(pendingEntries),
                                         pendingOffset,
                                         showHidden,
                                         searchText,
                                         categoryFilter,
                                         mixFilesAndFolders,
                                         sortRole,
                                         sortOrder);
    }));
}

bool DirectoryModel::selectFailedNavigationTarget(const QString &failedPath)
{
    const QString targetPath = failedNavigationSelectionPath(failedPath);
    const int targetIdx = m_pathIndex.value(modelPathKey(targetPath), -1);
    if (targetIdx < 0 || targetIdx >= m_entries.size()) {
        return false;
    }

    const int row = indexOfPath(targetPath);
    if (row < 0) {
        return false;
    }

    selectOnly(row);
    return true;
}

void DirectoryModel::restoreProviderForCurrentPathLater()
{
    const QString path = m_currentPath;
    if (path.isEmpty() || (m_provider && m_provider->canHandle(path))) {
        return;
    }

    QTimer::singleShot(0, this, [this, path]() {
        if (path == m_currentPath && (!m_provider || !m_provider->canHandle(path))) {
            replaceProvider(FileProviderFactory::createProvider(path));
        }
    });
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
    QElapsedTimer totalTimer;
    totalTimer.start();
    traceDirectoryNav("restartWatch-begin", m_currentPath,
                      QStringLiteral("watched=%1 suppress=%2 canWatch=%3")
                          .arg(QDir::toNativeSeparators(m_changeWatcher->watchedPath()))
                          .arg(m_suppressNextWatchRestart)
                          .arg(canWatchPath(m_currentPath)));
    m_changeWatcher->stop();
    m_parentChangeWatcher->stop();
    if (m_suppressNextWatchRestart) {
        m_suppressNextWatchRestart = false;
        m_deferredWatchRestartPending = true;
        m_deferredWatchRestartPath = m_currentPath;
        traceDirectoryWatch("restart-watch-suppressed", m_currentPath);
        traceDirectoryNav("restartWatch-end", m_currentPath,
                          QStringLiteral("result=suppressed elapsedMs=%1").arg(totalTimer.elapsed()));
        return;
    }
    if (!canWatchPath(m_currentPath)) {
        traceDirectoryNav("restartWatch-end", m_currentPath,
                          QStringLiteral("result=skip elapsedMs=%1").arg(totalTimer.elapsed()));
        return;
    }

    const bool watching = m_changeWatcher->watch(m_currentPath);
    if (!watching) {
        traceDirectoryWatch("restart-watch-failed", m_currentPath);
    }
    restartParentChangeWatcherForCurrentPath();
    traceDirectoryNav("restartWatch-end", m_currentPath,
                      QStringLiteral("result=%1 elapsedMs=%2 watched=%3")
                          .arg(watching)
                          .arg(totalTimer.elapsed())
                          .arg(QDir::toNativeSeparators(m_changeWatcher->watchedPath())));
}

void DirectoryModel::restartParentChangeWatcherForCurrentPath()
{
    m_parentChangeWatcher->stop();
    if (!canWatchPath(m_currentPath)) {
        return;
    }

    const QString currentPath = QDir::fromNativeSeparators(QFileInfo(m_currentPath).absoluteFilePath());
    const QString parentPath = QDir::fromNativeSeparators(QFileInfo(currentPath).absolutePath());
    if (parentPath.isEmpty() || sameFilesystemPath(parentPath, currentPath)) {
        return;
    }

    if (!m_parentChangeWatcher->watch(parentPath)) {
        traceDirectoryWatch("restart-parent-watch-failed", parentPath);
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
                            QStringLiteral("current=%1 loading=%2 watched=%3 pending=%4")
                                .arg(m_currentPath)
                                .arg(m_loading)
                                .arg(m_changeWatcher->watchedPath())
                                .arg(m_deferredWatchRestartPending));
        if (!m_deferredWatchRestartPending) {
            return;
        }
        if (m_loading
            || !sameFilesystemPath(QDir::fromNativeSeparators(m_currentPath),
                                   QDir::fromNativeSeparators(expectedPath))
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
    m_watchEventsReceived += events.size();

    if (m_bulkWatchSuppressed
        && sameFilesystemPath(QDir::fromNativeSeparators(m_currentPath),
                              QDir::fromNativeSeparators(m_bulkWatchSuppressedPath))) {
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
        }
        m_bulkWatchDirty = true;
        ++m_bulkWatchSuppressedBatches;
        m_bulkWatchSuppressedEvents += events.size();
        if (watchDebugEnabled()) {
            qDebug() << "[DirectoryWatch] bulk-suppressed"
                     << "path" << m_currentPath
                     << "incoming" << events.size()
                     << "batches" << m_bulkWatchSuppressedBatches
                     << "events" << m_bulkWatchSuppressedEvents;
        }
        return;
    }

    int transientPartEventsDropped = 0;
    int acceptedEvents = 0;
    for (const DirectoryChangeEvent &event : events) {
        if (isTransientPartWriteEvent(event)) {
            ++transientPartEventsDropped;
            continue;
        }
        appendCoalescedDirectoryEvent(m_pendingDirectoryEvents, event);
        ++acceptedEvents;
    }

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
                 << "accepted" << acceptedEvents
                 << "pending" << m_pendingDirectoryEvents.size()
                 << "received" << m_watchEventsReceived
                 << "droppedPart" << transientPartEventsDropped;
    }
    if (acceptedEvents > 0 && !m_pendingDirectoryEvents.isEmpty()) {
        m_directoryEventTimer.start();
    }
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
            if (!m_currentPath.isEmpty() && !QFileInfo::exists(m_currentPath)) {
                notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
                return;
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
    m_debounceTimer.start();
}

void DirectoryModel::onDirectoryWatchFailed(const QString &path, const QString &error)
{
    traceDirectoryWatch("watch-failed", path,
                        QStringLiteral("current=%1 recovering=%2 error=%3")
                            .arg(m_currentPath)
                            .arg(m_recoveringUnavailablePath)
                            .arg(error));

    const QString failedPath = QDir::fromNativeSeparators(path);
    const QString currentPath = QDir::fromNativeSeparators(m_currentPath);
    if (!currentPath.isEmpty()
        && sameFilesystemPath(failedPath, currentPath)
        && !m_loading) {
        if (!QFileInfo::exists(currentPath)) {
            notifyCurrentPathUnavailable(error);
            return;
        }
        refresh();
    }
}

void DirectoryModel::onParentDirectoryEventsReady(const QList<DirectoryChangeEvent> &events)
{
    if (events.isEmpty() || m_loading || m_currentPath.isEmpty()) {
        return;
    }

    const QString watchedPath = m_parentChangeWatcher->watchedPath();
    const QString currentPath = QDir::fromNativeSeparators(QFileInfo(m_currentPath).absoluteFilePath());
    for (const DirectoryChangeEvent &event : events) {
        if (!eventSourceMatches(event, watchedPath)) {
            continue;
        }

        if (event.type == DirectoryChangeEvent::Type::Overflow) {
            if (!QFileInfo::exists(currentPath)) {
                notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
                return;
            }
            continue;
        }

        const QString removedPath = event.type == DirectoryChangeEvent::Type::Renamed
            ? event.oldPath
            : event.path;
        if (!removedPath.isEmpty()
            && sameFilesystemPath(QDir::fromNativeSeparators(removedPath), currentPath)
            && (event.type == DirectoryChangeEvent::Type::Removed
                || event.type == DirectoryChangeEvent::Type::Renamed)) {
            notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
            return;
        }
    }
}

void DirectoryModel::onParentDirectoryWatchFailed(const QString &path, const QString &error)
{
    traceDirectoryWatch("parent-watch-failed", path,
                        QStringLiteral("current=%1 recovering=%2 error=%3")
                            .arg(m_currentPath)
                            .arg(m_recoveringUnavailablePath)
                            .arg(error));

    if (!m_currentPath.isEmpty()
        && !m_loading
        && !QFileInfo::exists(m_currentPath)) {
        notifyCurrentPathUnavailable(error);
    }
}

void DirectoryModel::onDebounceTimeout()
{
    if (!m_currentPath.isEmpty() && !m_loading) {
        refresh();
    }
}

void DirectoryModel::applyFilter()
{
    applyFilterInternal(false);
}

bool DirectoryModel::matchesFilter(const FileEntry &entry) const
{
    return entryMatchesFilterSnapshot(entry, m_searchText, m_categoryFilter);
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

    emit visualStructureAboutToChange();
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
    if (m_bulkWatchSuppressed) {
        m_pendingDirectoryEvents.clear();
    }
    if (!m_currentPath.isEmpty()) {
        m_provider->setShowHidden(m_showHidden);
        m_provider->refresh(m_currentPath);
    }
}

void DirectoryModel::refreshMountPointBadges()
{
    const QList<int> roles = {IsMountPointRole, PrimaryBadgeKindRole};
    for (int absoluteIndex = 0; absoluteIndex < m_entries.size(); ++absoluteIndex) {
        FileEntry &entry = m_entries[absoluteIndex];
        if (!entry.isDirectory || !QDir::isAbsolutePath(entry.path)) {
            continue;
        }

        const bool isMountPoint = LocalMountPointIndex::isMountPoint(entry.path);
        const bool isArchive = !entry.isDirectory
            && ArchiveSupport::isArchiveExtension(entry.suffix);
        const QString primaryBadgeKind = LocalFileBadgeResolver::primaryBadgeKind(
            entry.isBrokenSymLink, entry.isSymLink, isMountPoint, entry.isLocked, isArchive);
        if (entry.isMountPoint == isMountPoint
            && entry.primaryBadgeKind == primaryBadgeKind) {
            continue;
        }

        entry.isMountPoint = isMountPoint;
        entry.primaryBadgeKind = primaryBadgeKind;
        const int filteredIndex = m_filteredIndices.indexOf(absoluteIndex);
        if (filteredIndex >= 0) {
            const QModelIndex modelIndex = index(filteredIndex, 0);
            emit dataChanged(modelIndex, modelIndex, roles);
        }
    }
}

void DirectoryModel::setPinnedPathSnapshot(const QStringList &paths)
{
    m_pinnedPathKeys.clear();
    for (const QString &path : paths) {
        const QString key = FavoritesStore::normalizedPathKey(path);
        if (!key.isEmpty()) {
            m_pinnedPathKeys.insert(key);
        }
    }

    const QList<int> roles = {IsPinnedRole};
    for (int absoluteIndex = 0; absoluteIndex < m_entries.size(); ++absoluteIndex) {
        FileEntry &entry = m_entries[absoluteIndex];
        const bool isPinned = m_pinnedPathKeys.contains(FavoritesStore::normalizedPathKey(entry.path));
        if (entry.isPinned == isPinned) {
            continue;
        }
        entry.isPinned = isPinned;
        const int filteredIndex = m_filteredIndices.indexOf(absoluteIndex);
        if (filteredIndex >= 0) {
            const QModelIndex modelIndex = index(filteredIndex, 0);
            emit dataChanged(modelIndex, modelIndex, roles);
        }
    }
}

void DirectoryModel::updatePinnedPaths(const QStringList &changedPaths, const QStringList &snapshot)
{
    m_pinnedPathKeys.clear();
    for (const QString &path : snapshot) {
        const QString key = FavoritesStore::normalizedPathKey(path);
        if (!key.isEmpty()) {
            m_pinnedPathKeys.insert(key);
        }
    }

    const QList<int> roles = {IsPinnedRole};
    for (const QString &path : changedPaths) {
        const int absoluteIndex = m_pathIndex.value(modelPathKey(path), -1);
        if (absoluteIndex < 0 || absoluteIndex >= m_entries.size()) {
            continue;
        }
        FileEntry &entry = m_entries[absoluteIndex];
        const bool isPinned = m_pinnedPathKeys.contains(FavoritesStore::normalizedPathKey(entry.path));
        if (entry.isPinned == isPinned) {
            continue;
        }
        entry.isPinned = isPinned;
        const int filteredIndex = m_filteredIndices.indexOf(absoluteIndex);
        if (filteredIndex >= 0) {
            const QModelIndex modelIndex = index(filteredIndex, 0);
            emit dataChanged(modelIndex, modelIndex, roles);
        }
    }
}

void DirectoryModel::beginBulkWatchSuppression(const QString &path)
{
    if (path.isEmpty()
        || m_currentPath.isEmpty()
        || !sameFilesystemPath(QDir::fromNativeSeparators(m_currentPath),
                               QDir::fromNativeSeparators(path))) {
        return;
    }

    m_bulkWatchSuppressed = true;
    m_bulkWatchDirty = false;
    m_bulkWatchSuppressedPath = QDir::cleanPath(QDir::fromNativeSeparators(path));
    m_bulkWatchSuppressedBatches = 0;
    m_bulkWatchSuppressedEvents = 0;
    m_pendingDirectoryEvents.clear();
    if (watchDebugEnabled()) {
        qDebug() << "[DirectoryWatch] bulk-suppress-begin"
                 << "path" << m_currentPath;
    }
}

void DirectoryModel::endBulkWatchSuppression(const QString &path)
{
    if (!m_bulkWatchSuppressed) {
        return;
    }
    if (!path.isEmpty()
        && !sameFilesystemPath(QDir::fromNativeSeparators(path),
                               QDir::fromNativeSeparators(m_bulkWatchSuppressedPath))) {
        return;
    }

    if (watchDebugEnabled()) {
        qDebug() << "[DirectoryWatch] bulk-suppress-end"
                 << "path" << m_currentPath
                 << "dirty" << m_bulkWatchDirty
                 << "batches" << m_bulkWatchSuppressedBatches
                 << "events" << m_bulkWatchSuppressedEvents;
    }
    m_bulkWatchSuppressed = false;
    m_bulkWatchDirty = false;
    m_bulkWatchSuppressedPath.clear();
    m_bulkWatchSuppressedBatches = 0;
    m_bulkWatchSuppressedEvents = 0;
    m_pendingDirectoryEvents.clear();
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
    m_parentChangeWatcher->stop();
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
    emit visualStructureAboutToChange();
    beginResetModel();
    m_entries.clear();
    m_filteredIndices.clear();
    m_pathIndex.clear();
    m_foundPaths.clear();
    m_selectedCount = 0;
    endResetModel();
    setLoading(false);
    setError(QStringLiteral("Folder is no longer available"));
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
    entry.isPinned = m_pinnedPathKeys.contains(FavoritesStore::normalizedPathKey(entry.path));
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
            emit visualStructureAboutToChange();
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
        emit visualStructureAboutToChange();
        beginInsertRows(QModelIndex(), row, row);
        m_filteredIndices.insert(row, absoluteIdx);
        endInsertRows();
        emit countChanged();
        return true;
    }

    if (!shouldBeVisible && filteredRow != -1) {
        existing = entry;
        emit visualStructureAboutToChange();
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
#ifdef Q_OS_WIN
    if (!info.exists() && entryAttributesWindows(info) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
#else
    if (!info.exists()) {
        return false;
    }
#endif

    const QString normPath = modelPathKey(info.absoluteFilePath());
    if (!sameFilesystemPath(QDir::fromNativeSeparators(info.absolutePath()),
                            QDir::fromNativeSeparators(m_currentPath))) {
        return false;
    }
    if (m_pathIndex.contains(normPath)) {
        return false;
    }

    FileEntry entry = entryFromInfo(info);
    entry.isPinned = m_pinnedPathKeys.contains(FavoritesStore::normalizedPathKey(entry.path));
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
        emit visualStructureAboutToChange();
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

    QString normalizedPath = modelPathKey(QFileInfo(path).absoluteFilePath());
    int absoluteIdx = m_pathIndex.value(normalizedPath, -1);
    if (absoluteIdx < 0 && isProviderEntryPath(path)) {
        normalizedPath = modelPathKey(path);
        absoluteIdx = m_pathIndex.value(normalizedPath, -1);
    }
    
    if (absoluteIdx < 0) {
        return false;
    }

    if (m_entries.at(absoluteIdx).isSelected) {
        --m_selectedCount;
        emit selectionChanged();
    }

    const int filteredIdx = filteredRowForAbsoluteIndex(absoluteIdx);

    if (filteredIdx != -1) {
        emit visualStructureAboutToChange();
        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
        m_filteredIndices.removeAt(filteredIdx);
        m_pathIndex.remove(normalizedPath);
        m_entries.removeAt(absoluteIdx);

        for (int &idx : m_filteredIndices) {
            if (idx > absoluteIdx) {
                --idx;
            }
        }
        updatePathIndex();
        endRemoveRows();
    } else {
        m_pathIndex.remove(normalizedPath);
        m_entries.removeAt(absoluteIdx);

        for (int &idx : m_filteredIndices) {
            if (idx > absoluteIdx) {
                --idx;
            }
        }
        updatePathIndex();
    }
    
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

void DirectoryModel::extendOrTrimRange(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_filteredIndices.size() || to >= m_filteredIndices.size()) {
        return;
    }

    const int start = std::min(from, to);
    const int end = std::max(from, to);

    bool rangeAlreadySelected = true;
    for (int row = start; row <= end; ++row) {
        if (!m_entries.at(m_filteredIndices.at(row)).isSelected) {
            rangeAlreadySelected = false;
            break;
        }
    }

    if (!rangeAlreadySelected) {
        selectRange(from, to);
        return;
    }

    int selectedStart = start;
    while (selectedStart > 0 && m_entries.at(m_filteredIndices.at(selectedStart - 1)).isSelected) {
        --selectedStart;
    }

    int selectedEnd = end;
    while (selectedEnd + 1 < m_filteredIndices.size()
           && m_entries.at(m_filteredIndices.at(selectedEnd + 1)).isSelected) {
        ++selectedEnd;
    }

    bool selectionChangedOccurred = false;
    for (int row = selectedStart; row <= selectedEnd; ++row) {
        const bool shouldSelect = row >= start && row <= end;
        const int actualIdx = m_filteredIndices.at(row);
        if (m_entries[actualIdx].isSelected != shouldSelect) {
            m_entries[actualIdx].isSelected = shouldSelect;
            m_selectedCount += shouldSelect ? 1 : -1;
            selectionChangedOccurred = true;
            emit dataChanged(index(row), index(row), {IsSelectedRole});
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

bool DirectoryModel::isShortcutAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return false;
    }
    return m_entries.at(m_filteredIndices.at(row)).isShortcut;
}

QString DirectoryModel::shortcutTargetPathAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return {};
    }
    return m_entries.at(m_filteredIndices.at(row)).shortcutTargetPath;
}

QString DirectoryModel::shortcutOpenPathAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return {};
    }
    return m_entries.at(m_filteredIndices.at(row)).shortcutOpenPath;
}

bool DirectoryModel::shortcutTargetIsDirectoryAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return false;
    }
    return m_entries.at(m_filteredIndices.at(row)).shortcutTargetIsDirectory;
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

void DirectoryModel::invalidateThumbnails(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return;
    }

    QSet<int> changedRows;
    for (const QString &path : paths) {
        const QString normPath = modelPathKey(path);
        const int absIdx = m_pathIndex.value(normPath, -1);
        if (absIdx < 0 || absIdx >= m_entries.size()) {
            continue;
        }

        const QString entryPath = m_entries.at(absIdx).path;
        m_thumbnailRevisions[entryPath] = m_thumbnailRevisions.value(entryPath, 0) + 1;
        for (int row = 0; row < m_filteredIndices.size(); ++row) {
            if (m_filteredIndices.at(row) == absIdx) {
                changedRows.insert(row);
                break;
            }
        }
    }

    for (int row : changedRows) {
        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, {ThumbnailRevisionRole});
    }
}

QString DirectoryModel::formatSize(qint64 bytes)
{
    return DriveUtils::formatSize(bytes);
}

QString DirectoryModel::iconNameFor(const FileEntry &entry)
{
    return entry.iconName;
}

void DirectoryModel::processAllPendingInsertsFast()
{
    if (m_pendingInsertOffset >= m_pendingInserts.size()) {
        m_pendingInserts.clear();
        m_pendingInsertOffset = 0;
        return;
    }

    if (m_freshLoad) {
        if (!m_freshLoadCommitted) {
            commitFreshLoad(m_pendingFreshLoadPath);
        }
        emit visualStructureAboutToChange();
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
                    emit visualStructureAboutToChange();
                    beginInsertRows(QModelIndex(), row, row);
                    m_filteredIndices.insert(row, absoluteIdx);
                    endInsertRows();
                } else if (!shouldBeVisible && filteredRow != -1) {
                    emit visualStructureAboutToChange();
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
                    emit visualStructureAboutToChange();
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

void DirectoryModel::setScanProgress(double progress, const QString &text)
{
    const double normalized = progress < 0.0 ? -1.0 : std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_scanProgress + 1.0, normalized + 1.0) && m_scanProgressText == text) {
        return;
    }
    m_scanProgress = normalized;
    m_scanProgressText = normalized < 0.0 ? QString{} : text;
    emit scanProgressChanged();
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

void DirectoryModel::setSortPolicy(SortRole role, Qt::SortOrder order)
{
    const bool roleChanged = m_sortRole != role;
    const bool orderChanged = m_sortOrder != order;
    if (!roleChanged && !orderChanged) {
        return;
    }

    m_sortRole = role;
    m_sortOrder = order;
    sortModel();

    if (roleChanged) {
        emit sortRoleChanged();
    }
    if (orderChanged) {
        emit sortOrderChanged();
    }
}

bool DirectoryModel::compareEntries(const FileEntry &a, const FileEntry &b) const
{
    return compareEntriesForPolicy(a, b, m_mixFilesAndFolders, m_sortRole, m_sortOrder);
}

void DirectoryModel::sortModel()
{
    if (m_filteredIndices.isEmpty()) {
        return;
    }

    emit visualStructureAboutToChange();
    emit layoutAboutToBeChanged();
    std::stable_sort(m_filteredIndices.begin(), m_filteredIndices.end(),
        [this](int aIdx, int bIdx) {
            return compareEntries(m_entries.at(aIdx), m_entries.at(bIdx));
        });
    emit layoutChanged();
}
