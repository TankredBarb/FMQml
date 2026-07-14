#include "ArchiveFileProvider.h"

#include "ArchiveOperationCallbacks.h"
#include "ArchiveSupport.h"
#include "DriveUtils.h"
#include "CleanupSubsystem.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QMimeDatabase>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtConcurrent>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

#ifdef Q_OS_LINUX
#include <sched.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <unistd.h>
#endif

#ifdef HAS_UNOFFICIAL_BIT7Z
#include <bit7z/bit7z.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitexception.hpp>
#include <bit7z/bitformat.hpp>
#endif

#include "ArchiveFileProviderInternal.h"

using namespace ArchiveFileProviderInternal;

bool ArchiveFileProvider::ensureParentDirectory(const QString &path) const
{
    Q_UNUSED(path)
    return false;
}

bool ArchiveFileProvider::makePath(const QString &path) const
{
    Q_UNUSED(path)
    return false;
}

bool ArchiveFileProvider::removePath(const QString &path) const
{
    Q_UNUSED(path)
    return false;
}

QStringList ArchiveFileProvider::childPaths(const QString &path, bool includeHidden) const
{
    QString browsePath;
    if (auto state = cachedStateForPath(path, &browsePath)) {
        const QString browse = archiveRelativeToken(browsePath);
        QStringList result;
        for (const ArchiveItemRecord &record : state->items) {
            if (record.relativePath == browse) {
                continue;
            }
            const QString parent = parentRelativePath(record.relativePath);
            if (parent != browse) {
                continue;
            }
            if (!includeHidden && record.isHidden) {
                continue;
            }
            result.append(record.absolutePath);
        }
        return result;
    }

    ArchiveState state = stateForPath(path);
    if (!state.valid) {
        return {};
    }

    const QString browse = archiveRelativeToken(state.browsePath);
    QStringList result;
    for (const ArchiveItemRecord &record : state.items) {
        if (record.relativePath == browse) {
            continue;
        }
        const QString parent = parentRelativePath(record.relativePath);
        if (parent != browse) {
            continue;
        }
        if (!includeHidden && record.isHidden) {
            continue;
        }
        result.append(record.absolutePath);
    }
    return result;
}

bool ArchiveFileProvider::movePath(const QString &sourcePath, const QString &destinationPath) const
{
    Q_UNUSED(sourcePath)
    Q_UNUSED(destinationPath)
    return false;
}

std::unique_ptr<QIODevice> ArchiveFileProvider::openRead(const QString &path) const
{
    QString browsePath;
    if (auto state = cachedStateForPath(path, &browsePath)) {
        if (state->reader) {
            return openReadFromState(*state, browsePath);
        }
    }

    ArchiveState state = stateForPath(path);
    if (!state.valid) {
        return {};
    }
    return openReadFromState(state, state.browsePath);
}

std::unique_ptr<QIODevice> ArchiveFileProvider::openReadFromState(const ArchiveState &state, const QString &browsePath)
{
    const QString rel = archiveRelativeToken(browsePath);
    const int idx = state.pathIndex.value(rel, -1);
    if (idx < 0 || idx >= state.items.size()) {
        qWarning() << "[FM_ARCHIVE_READ] item not found"
                   << "browsePath" << browsePath
                   << "rel" << rel
                   << "items" << state.items.size();
        return {};
    }

    const ArchiveItemRecord &record = state.items.at(idx);
    if (record.isDirectory) {
        return {};
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    if (!state.reader) {
        qWarning() << "[FM_ARCHIVE_READ] missing reader"
                   << "sourcePath" << state.sourcePath
                   << "browsePath" << browsePath
                   << "rel" << rel;
        return {};
    }

    try {
        QString readTemporaryParent = archiveSourceTemporaryParentPath(state.sourcePath);
        if (readTemporaryParent.isEmpty()) {
            const QString defaultRoot = StagingLocationPolicy::defaultCleanupRoot();
            if (!defaultRoot.isEmpty()) {
                readTemporaryParent = QDir(defaultRoot).filePath(QStringLiteral("archive-read"));
            }
        }
        if (!readTemporaryParent.isEmpty()) {
            QDir().mkpath(readTemporaryParent);
            cleanupStaleArchiveTemporaryDirs(readTemporaryParent);
        }
        if (readTemporaryParent.isEmpty()) {
            qWarning() << "[FM_ARCHIVE_READ] cannot resolve cleanup temporary parent"
                       << "sourcePath" << state.sourcePath
                       << "rel" << rel;
            return {};
        }
        auto tempDir = std::make_unique<QTemporaryDir>(
            QDir(readTemporaryParent).filePath(QStringLiteral(".fm-read-XXXXXX")));
        if (!tempDir->isValid()) {
            qWarning() << "[FM_ARCHIVE_READ] temp dir invalid"
                       << "path" << tempDir->path()
                       << "sourcePath" << state.sourcePath
                       << "rel" << rel;
            return {};
        }
        tempDir->setAutoRemove(false);
        const QString tempRoot = QDir::fromNativeSeparators(tempDir->path());
        registerArchiveTemporaryDirectory(tempRoot, readTemporaryParent, CleanupArtifactKind::ArchiveBrowse);
        const auto cleanupTempRoot = [&tempRoot]() {
            if (!tempRoot.isEmpty()) {
                scheduleArchiveTemporaryDirectoryCleanup(tempRoot);
            }
        };

        {
            QMutexLocker readerLocker(&archiveReaderMutex());
            state.reader->setProgressCallback([](uint64_t processedBytes) -> bool {
                ArchiveOperationCallbacks::current().reportProgress(processedBytes);
                return !ArchiveOperationCallbacks::current().isAbortRequested();
            });
            try {
                state.reader->extractTo(toBit7zString(QDir::toNativeSeparators(tempRoot)), std::vector<uint32_t>{record.index});
                state.reader->setProgressCallback(nullptr);
            } catch (const bit7z::BitException &exception) {
                state.reader->setProgressCallback(nullptr);
                if (ArchiveOperationCallbacks::current().isAbortRequested()) {
                } else {
                    qWarning() << "[FM_ARCHIVE_READ] extract selected item failed"
                               << "sourcePath" << state.sourcePath
                               << "browsePath" << browsePath
                               << "rel" << rel
                               << "recordName" << record.name
                               << "recordIndex" << record.index
                               << "recordSize" << record.size
                               << "tempRoot" << tempRoot
                               << "message" << QString::fromUtf8(exception.what())
                               << "nativeCode" << exception.nativeCode()
                               << "hresult" << exception.hresultCode();
                    for (const auto &failedFile : exception.failedFiles()) {
                        qWarning() << "[FM_ARCHIVE_READ] failed file"
                                   << toQString(failedFile.first)
                                   << failedFile.second.value()
                                   << QString::fromStdString(failedFile.second.message());
                    }
                }
                cleanupTempRoot();
                throw;
            } catch (const std::exception &exception) {
                state.reader->setProgressCallback(nullptr);
                qWarning() << "[FM_ARCHIVE_READ] extract selected item failed"
                           << "sourcePath" << state.sourcePath
                           << "browsePath" << browsePath
                           << "rel" << rel
                           << "recordName" << record.name
                           << "recordIndex" << record.index
                           << "recordSize" << record.size
                           << "tempRoot" << tempRoot
                           << "message" << QString::fromUtf8(exception.what());
                cleanupTempRoot();
                throw;
            } catch (...) {
                state.reader->setProgressCallback(nullptr);
                qWarning() << "[FM_ARCHIVE_READ] extract selected item failed with unknown exception"
                           << "sourcePath" << state.sourcePath
                           << "browsePath" << browsePath
                           << "rel" << rel
                           << "recordName" << record.name
                           << "recordIndex" << record.index
                           << "recordSize" << record.size
                           << "tempRoot" << tempRoot;
                cleanupTempRoot();
                throw;
            }
        }

        const QString extractedPath = extractedArchiveItemPath(tempRoot, record.relativePath, record.name);
        if (extractedPath.isEmpty()) {
            qWarning() << "[FM_ARCHIVE_READ] extracted item missing"
                       << "sourcePath" << state.sourcePath
                       << "browsePath" << browsePath
                       << "rel" << rel
                       << "recordName" << record.name
                       << "recordIndex" << record.index
                       << "recordSize" << record.size
                       << "tempRoot" << tempRoot
                       << "files" << sampledExtractedFiles(tempRoot);
            cleanupTempRoot();
            return {};
        }

        auto device = std::make_unique<TemporaryFileDevice>(extractedPath, tempRoot);
        if (!device->open(QIODevice::ReadOnly)) {
            qWarning() << "[FM_ARCHIVE_READ] extracted item open failed"
                       << "sourcePath" << state.sourcePath
                       << "browsePath" << browsePath
                       << "rel" << rel
                       << "extractedPath" << extractedPath
                       << "error" << device->errorString();
            cleanupTempRoot();
            return {};
        }
        return device;
    } catch (const std::exception &exception) {
        if (ArchiveOperationCallbacks::current().isAbortRequested()) {
            return {};
        }
        qWarning() << "[FM_ARCHIVE_READ] openRead failed"
                   << "sourcePath" << state.sourcePath
                   << "browsePath" << browsePath
                   << "rel" << rel
                   << "recordName" << record.name
                   << "recordIndex" << record.index
                   << "message" << QString::fromUtf8(exception.what());
        return {};
    }
#else
    Q_UNUSED(record)
    return {};
#endif
}

std::unique_ptr<QIODevice> ArchiveFileProvider::openWrite(const QString &path, bool truncate) const
{
    Q_UNUSED(path)
    Q_UNUSED(truncate)
    return {};
}

bool ArchiveFileProvider::renamePath(const QString &oldPath, const QString &newName)
{
    Q_UNUSED(oldPath)
    Q_UNUSED(newName)
    return false;
}

bool ArchiveFileProvider::createFolder(const QString &parentPath, const QString &name, QString *createdPath)
{
    Q_UNUSED(parentPath)
    Q_UNUSED(name)
    Q_UNUSED(createdPath)
    return false;
}

bool ArchiveFileProvider::createFile(const QString &parentPath, const QString &name, QString *createdPath)
{
    Q_UNUSED(parentPath)
    Q_UNUSED(name)
    Q_UNUSED(createdPath)
    return false;
}

bool ArchiveFileProvider::ensureLibrary() const
{
#ifdef HAS_UNOFFICIAL_BIT7Z
    if (m_library) {
        return true;
    }
    m_library = getGlobalLibrary();
    return m_library != nullptr;
#else
    return false;
#endif
}

QString ArchiveFileProvider::normalizeRelativePath(QString path)
{
    path = QDir::fromNativeSeparators(path.trimmed());
    if (path == QLatin1String("/")) {
        return {};
    }
    if (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path;
}

QString ArchiveFileProvider::parentRelativePath(const QString &path)
{
    const QString normalized = normalizeRelativePath(path);
    if (normalized.isEmpty()) {
        return {};
    }
    const int slash = normalized.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) {
        return {};
    }
    return normalized.left(slash);
}

bool ArchiveFileProvider::isArchiveLike(const QString &suffix)
{
    return ArchiveSupport::isArchiveExtension(suffix);
}

std::string ArchiveFileProvider::toBit7zString(const QString &path)
{
    return path.toStdString();
}

QDateTime ArchiveFileProvider::toDateTime(const std::chrono::time_point<std::chrono::system_clock> &timePoint)
{
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(timePoint.time_since_epoch()).count();
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secs));
}

QString ArchiveFileProvider::itemAbsolutePath(const QString &archivePrefix, const QString &relativePath)
{
    if (relativePath.isEmpty()) {
        return archivePrefix;
    }
    // archivePrefix is expected to end with '|' (e.g., "archive://C:/a.zip|")
    // or "|/" for root. We want to ensure that for items inside the archive,
    // we use '|/' as the base and then the relative path.
    QString base = archivePrefix;
    if (base.endsWith(QLatin1Char('|'))) {
        base.append(QLatin1Char('/'));
    }
    return base + relativePath;
}

FileEntry ArchiveFileProvider::fileEntryFromRecord(const ArchiveState &state, const ArchiveItemRecord &record)
{
    FileEntry entry;
    entry.name = record.name;
    entry.path = record.absolutePath;
    entry.suffix = record.suffix;
    entry.size = record.size;
    entry.modified = record.modified;
    entry.created = record.created;
    entry.isDirectory = record.isDirectory;
    entry.isHidden = record.isHidden;
    entry.isReadOnly = false;
    entry.isSystem = record.isSymLink;

    QLocale loc;
    entry.sizeText = entry.isDirectory ? QString() : DriveUtils::formatSize(entry.size);
    entry.modifiedText = entry.modified.isValid() ? loc.toString(entry.modified, QLocale::ShortFormat) : QString();
    entry.createdText = entry.created.isValid() ? loc.toString(entry.created, QLocale::ShortFormat) : QString();

    QString attrs;
    if (entry.isDirectory) attrs += QLatin1Char('D');
    if (entry.isHidden) attrs += QLatin1Char('H');
    if (entry.isReadOnly) attrs += QLatin1Char('R');
    if (entry.isSystem) attrs += QLatin1Char('L');
    entry.attributesText = attrs;
    entry.isImage = false;
    entry.hasThumbnail = false;
    Q_UNUSED(state)
    return entry;
}

ArchiveFileProvider::ArchiveState ArchiveFileProvider::stateForPath(const QString &path) const
{
    if (!ensureLibrary()) {
        ArchiveState state;
        state.currentPath = normalizedPath(path);
        state.error = QStringLiteral("bit7z backend was not found or could not be loaded");
        return state;
    }
    return buildStateFromScratch(path, m_library);
}

std::shared_ptr<ArchiveFileProvider::ArchiveState> ArchiveFileProvider::cachedStateForPath(const QString &path, QString *browsePath) const
{
    const QString normalized = normalizedPath(path);
    if (m_state && m_state->valid
        && archiveContainerPart(normalized) == archiveContainerPart(m_state->currentPath)) {
        if (browsePath) {
            *browsePath = archiveBrowsePathForPath(normalized);
        }
        return m_state;
    }

    const QString key = archiveCacheKey(normalized);
    auto cached = cachedStateForKey(key);
    if (!cached || !cached->valid) {
        return nullptr;
    }
    if (browsePath) {
        *browsePath = archiveBrowsePathForPath(normalized);
    }
    m_state = cached;
    return cached;
}

QList<FileEntry> ArchiveFileProvider::visibleEntriesForBrowse(const ArchiveState &state, const QString &browsePath, bool showHidden)
{
    QList<FileEntry> entries;
    entries.reserve(state.items.size());
    const QString browse = normalizeRelativePath(browsePath);
    for (const ArchiveItemRecord &record : std::as_const(state.items)) {
        if (record.relativePath == browse) {
            continue;
        }
        const QString parent = parentRelativePath(record.relativePath);
        if (parent != browse) {
            continue;
        }
        if (!showHidden && record.isHidden) {
            continue;
        }
        entries.append(fileEntryFromRecord(state, record));
    }

    std::sort(entries.begin(), entries.end(), [](const FileEntry &lhs, const FileEntry &rhs) {
        if (lhs.isDirectory != rhs.isDirectory) {
            return lhs.isDirectory;
        }
        return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
    });
    return entries;
}

QString ArchiveFileProvider::archiveContainerPart(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return path;
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
        parts.append(normalizeRelativePath(tokens.at(i)));
    }
    return QStringLiteral("archive://") + parts.join(QLatin1Char('|'));
}

QString ArchiveFileProvider::archiveBrowsePathForPath(const QString &path)
{
    QString working = path;
    if (ArchiveSupport::isArchiveFilePath(working)) {
        working = ArchiveSupport::archiveRootPath(working);
    }
    const QStringList tokens = archiveTokenPath(working).split(QLatin1Char('|'), Qt::KeepEmptyParts);
    if (tokens.isEmpty()) {
        return {};
    }
    QString browse = normalizeRelativePath(tokens.last());
    if (tokens.last() == QLatin1String("/")) {
        browse.clear();
    }
    return browse;
}

QString ArchiveFileProvider::archiveCacheKey(const QString &path)
{
    QString normalized = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::normalizeArchivePath(path)
        : (ArchiveSupport::isArchiveFilePath(path)
            ? ArchiveSupport::archiveRootPath(path)
            : ArchiveSupport::normalizeArchivePath(path));
    const QString physicalPath = ArchiveSupport::physicalArchivePath(normalized);
    const QFileInfo info(physicalPath);
    return QStringLiteral("%1|%2|%3|%4")
        .arg(archiveContainerPart(normalized))
        .arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch())
        .arg(info.exists());
}

ArchiveFileProvider::ArchiveState ArchiveFileProvider::buildStateFromScratch(
    const QString &path,
    const std::shared_ptr<bit7z::Bit7zLibrary> &library,
    const std::function<void(const QList<FileEntry> &)> &batchCallback,
    bool showHidden,
    const std::shared_ptr<std::atomic_bool> &cancelled,
    const QString &temporaryParentPath,
    const std::function<void(uint64_t, uint64_t)> &progressReporter)
{
    QString normalized = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::normalizeArchivePath(path)
        : (ArchiveSupport::isArchiveFilePath(path)
            ? ArchiveSupport::archiveRootPath(path)
            : ArchiveSupport::normalizeArchivePath(path));
    ArchiveState state;
    state.currentPath = normalized;
    if (state.currentPath.isEmpty()) {
        state.error = QStringLiteral("Invalid archive path");
        return state;
    }

    QString working = state.currentPath;
    if (ArchiveSupport::isArchiveFilePath(working)) {
        working = ArchiveSupport::archiveRootPath(working);
    }
    if (!ArchiveSupport::isArchivePath(working)) {
        state.error = QStringLiteral("Path is not an archive");
        return state;
    }

    const QStringList tokens = archiveTokenPath(working).split(QLatin1Char('|'), Qt::KeepEmptyParts);
    if (tokens.isEmpty()) {
        state.error = QStringLiteral("Archive path is empty");
        return state;
    }

    const QString sourcePath = tokens.first();
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        state.error = QStringLiteral("Archive file was not found");
        return state;
    }

    if (!library) {
        state.error = QStringLiteral("bit7z backend was not found or could not be loaded");
        return state;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    try {
        const QStringList chain = tokens.mid(1, qMax(0, tokens.size() - 2));
        const QString browsePathToken = tokens.last();

        std::unique_ptr<bit7z::BitArchiveReader> reader;
        std::unique_ptr<QTemporaryDir> currentTempDir;
        const QString sourceTemporaryParent = archiveSourceTemporaryParentPath(sourcePath);
        const QString effectiveTemporaryParent = !sourceTemporaryParent.isEmpty()
            ? sourceTemporaryParent
            : (!temporaryParentPath.isEmpty()
                ? QDir::fromNativeSeparators(temporaryParentPath)
                : g_currentThreadTemporaryParentPath);

        QString readerOpenError;
        auto openReaderFromFile = [&](const QString &archivePath, const QString &formatSuffix) -> std::unique_ptr<bit7z::BitArchiveReader> {
            const QStringList candidates = formatSuffix.compare(QStringLiteral("rar"), Qt::CaseInsensitive) == 0
                ? QStringList{rarFormatCandidateForFile(archivePath)}
                : archiveFormatCandidatesForSuffix(formatSuffix);
            const QString password = archivePasswordForPath(working);
            for (const QString &candidate : candidates) {
                try {
                    const auto &format = archiveFormatForSuffix(candidate);
                    return std::make_unique<bit7z::BitArchiveReader>(
                        *library,
                        toBit7zString(archivePath),
                        bit7z::ArchiveStartOffset::FileStart,
                        format,
                        toBit7zString(password));
                } catch (const std::exception &exception) {
                    readerOpenError = QString::fromUtf8(exception.what());
                    continue;
                }
            }
            return {};
        };

        ResolvedArchiveContainer resolvedContainer;
        const auto cleanupResolvedContainer = qScopeGuard([&resolvedContainer]() {
            releaseTemporaryDirAsync(std::move(resolvedContainer.tempDir));
        });
        QString resolveError;
        const bool resolved = resolveArchiveContainerWithSevenZip(
            working,
            effectiveTemporaryParent,
            &resolvedContainer,
            &resolveError,
            [cancelled](uint64_t) -> bool {
                if (cancelled && cancelled->load()) {
                    return false;
                }
                return !ArchiveOperationCallbacks::current().isAbortRequested();
            },
            progressReporter);
        if (!resolved) {
            state.error = resolveError.isEmpty()
                ? QStringLiteral("Could not prepare archive container")
                : resolveError;
            if (archiveNestedTraceEnabled()) {
                qInfo().noquote() << "[ArchiveNested] build resolve failed"
                                  << "working=" << working
                                  << "error=" << state.error;
            }
            return state;
        }

        if (archiveNestedTraceEnabled()) {
            qInfo().noquote() << "[ArchiveNested] reader open begin"
                              << "working=" << working
                              << "physical=" << QDir::toNativeSeparators(resolvedContainer.physicalPath)
                              << "size=" << QFileInfo(resolvedContainer.physicalPath).size()
                              << "suffix=" << QFileInfo(resolvedContainer.physicalPath).suffix().toLower();
        }
        reader = openReaderFromFile(resolvedContainer.physicalPath,
                                    QFileInfo(resolvedContainer.physicalPath).suffix().toLower());
        if (!reader) {
            state.error = errorNeedsPassword(readerOpenError)
                ? QStringLiteral("Archive password required")
                : QStringLiteral("Unsupported archive format");
            if (archiveNestedTraceEnabled()) {
                qInfo().noquote() << "[ArchiveNested] reader open failed"
                                  << "working=" << working
                                  << "physical=" << QDir::toNativeSeparators(resolvedContainer.physicalPath)
                                  << "error=" << readerOpenError
                                  << "stateError=" << state.error;
            }
            return state;
        }
        if (archivePasswordForPath(working).isEmpty() && reader->hasEncryptedItems()) {
            state.error = QStringLiteral("Archive password required");
            if (archiveNestedTraceEnabled()) {
                qInfo().noquote() << "[ArchiveNested] reader encrypted"
                                  << "working=" << working
                                  << "physical=" << QDir::toNativeSeparators(resolvedContainer.physicalPath);
            }
            return state;
        }
        currentTempDir = std::move(resolvedContainer.tempDir);

        state.valid = true;
        state.sourcePath = sourcePath;
        state.physicalContainerPath = resolvedContainer.physicalPath;
        state.browsePath = normalizeRelativePath(browsePathToken);
        if (browsePathToken == QLatin1String("/")) {
            state.browsePath.clear();
        }

        // Correctly build prefix for nested archives
        QStringList prefixParts;
        prefixParts << sourcePath;
        for (const QString &segment : chain) {
            prefixParts << normalizeRelativePath(segment);
        }
        state.archivePrefix = QStringLiteral("archive://") + prefixParts.join(QLatin1Char('|')) + QLatin1Char('|');

        state.reader = std::move(reader);
        state.tempDir = std::move(currentTempDir);
        if (state.tempDir && state.tempDir->isValid()) {
            const QString safetyRoot = !effectiveTemporaryParent.isEmpty()
                ? effectiveTemporaryParent
                : QFileInfo(state.tempDir->path()).absolutePath();
            state.tempLeaseId = CleanupSubsystem::instance().registerArtifact(
                CleanupArtifactKind::ArchiveBrowse,
                state.tempDir->path(),
                safetyRoot,
                true);
            if (!state.tempLeaseId.isEmpty()) {
                state.tempDir->setAutoRemove(false);
            }
        }
        state.tempFile.reset();

        const uint32_t itemCount = state.reader->itemsCount();
        if (archiveNestedTraceEnabled()) {
            qInfo().noquote() << "[ArchiveNested] listing begin"
                              << "working=" << working
                              << "physical=" << QDir::toNativeSeparators(state.physicalContainerPath)
                              << "browsePath=" << state.browsePath
                              << "itemCount=" << itemCount;
        }
        state.items.reserve(static_cast<int>(itemCount));
        QList<FileEntry> visibleBatch;
        visibleBatch.reserve(512);
        bool firstVisibleBatchSent = false;

        for (uint32_t i = 0; i < itemCount; ++i) {
            if (cancelled && cancelled->load()) {
                state.error = QStringLiteral("Archive scan was cancelled");
                return state;
            }
            const auto item = state.reader->itemAt(i);
            ArchiveItemRecord record;
            record.relativePath = normalizeRelativePath(toQString(item.path()));
            record.name = toQString(item.name());
            record.suffix = archiveSuffixFromName(record.name);
            record.size = static_cast<qint64>(item.size());
            record.modified = toDateTime(item.lastWriteTime());
            record.created = toDateTime(item.creationTime());
            record.isDirectory = item.isDir();
            record.isHidden = isHiddenName(record.name);
            record.isSymLink = item.isSymLink();
            record.isArchive = isArchiveLike(record.suffix);
            record.index = item.index();
            record.absolutePath = itemAbsolutePath(state.archivePrefix, record.relativePath);
            const bool isVisibleDirectChild = record.relativePath != state.browsePath
                && parentRelativePath(record.relativePath) == state.browsePath
                && (showHidden || !record.isHidden);
            state.pathIndex.insert(record.relativePath, state.items.size());
            state.items.append(record);

            if (batchCallback && isVisibleDirectChild) {
                visibleBatch.append(fileEntryFromRecord(state, state.items.constLast()));
                if (!firstVisibleBatchSent || visibleBatch.size() >= 512) {
                    if (archiveNestedTraceEnabled()) {
                        qInfo().noquote() << "[ArchiveNested] listing batch"
                                          << "working=" << working
                                          << "phase=items"
                                          << "batchSize=" << visibleBatch.size()
                                          << "itemsScanned=" << state.items.size();
                    }
                    batchCallback(visibleBatch);
                    visibleBatch.clear();
                    firstVisibleBatchSent = true;
                }
            }

            QString parent = parentRelativePath(record.relativePath);
            while (!parent.isEmpty()) {
                state.directories.insert(parent);
                const int slash = parent.lastIndexOf(QLatin1Char('/'));
                if (slash < 0) {
                    break;
                }
                parent = parent.left(slash);
            }
            if (!record.relativePath.isEmpty()) {
                state.directories.insert(parentRelativePath(record.relativePath));
            }
        }

        for (const QString &directoryPath : std::as_const(state.directories)) {
            const QString normalizedDirectory = normalizeRelativePath(directoryPath);
            if (normalizedDirectory.isEmpty() || state.pathIndex.contains(normalizedDirectory)) {
                continue;
            }

            ArchiveItemRecord record;
            record.relativePath = normalizedDirectory;
            record.name = QFileInfo(normalizedDirectory).fileName();
            record.suffix = archiveSuffixFromName(record.name);
            record.isDirectory = true;
            record.isHidden = isHiddenName(record.name);
            record.isArchive = false;
            record.absolutePath = itemAbsolutePath(state.archivePrefix, record.relativePath);

            const bool isVisibleDirectChild = parentRelativePath(record.relativePath) == state.browsePath
                && (showHidden || !record.isHidden);
            state.pathIndex.insert(record.relativePath, state.items.size());
            state.items.append(record);

            if (batchCallback && isVisibleDirectChild) {
                visibleBatch.append(fileEntryFromRecord(state, state.items.constLast()));
                if (!firstVisibleBatchSent || visibleBatch.size() >= 512) {
                    if (archiveNestedTraceEnabled()) {
                        qInfo().noquote() << "[ArchiveNested] listing batch"
                                          << "working=" << working
                                          << "phase=directories"
                                          << "batchSize=" << visibleBatch.size()
                                          << "itemsScanned=" << state.items.size();
                    }
                    batchCallback(visibleBatch);
                    visibleBatch.clear();
                    firstVisibleBatchSent = true;
                }
            }
        }

        if (batchCallback && !visibleBatch.isEmpty()) {
            if (archiveNestedTraceEnabled()) {
                qInfo().noquote() << "[ArchiveNested] listing batch"
                                  << "working=" << working
                                  << "phase=final"
                                  << "batchSize=" << visibleBatch.size()
                                  << "itemsScanned=" << state.items.size();
            }
            batchCallback(visibleBatch);
        }

        state.directories.insert(QString());
        if (archiveNestedTraceEnabled()) {
            qInfo().noquote() << "[ArchiveNested] listing done"
                              << "working=" << working
                              << "physical=" << QDir::toNativeSeparators(state.physicalContainerPath)
                              << "items=" << state.items.size()
                              << "directories=" << state.directories.size()
                              << "firstBatchSent=" << firstVisibleBatchSent;
        }
        return state;
    } catch (const std::exception &ex) {
        state.error = QString::fromUtf8(ex.what());
        if (archiveNestedTraceEnabled()) {
            qInfo().noquote() << "[ArchiveNested] build exception"
                              << "working=" << working
                              << "error=" << state.error;
        }
        return state;
    }
#else
    state.error = QStringLiteral("bit7z support is disabled");
    return state;
#endif
}
