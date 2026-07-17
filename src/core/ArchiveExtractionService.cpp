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

bool ArchiveFileProvider::extractArchiveFileTo(const QString &archivePath,
                                               const QString &destinationPath,
                                               QString *error,
                                               const std::function<bool(uint64_t)> &progressCallback,
                                               const std::function<void(const QString &)> &fileCallback)
{
    if (error) {
        error->clear();
    }

    if (archivePath.isEmpty() || destinationPath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Archive path or destination is empty");
        }
        return false;
    }

    if (!ArchiveSupport::isArchiveFilePath(archivePath)) {
        if (error) {
            *error = QStringLiteral("Path is not a supported archive: %1").arg(archivePath);
        }
        return false;
    }

    const QString normalizedArchivePath = QDir::fromNativeSeparators(QFileInfo(archivePath).absoluteFilePath());
    const QString normalizedDestinationPath = QDir::fromNativeSeparators(QFileInfo(destinationPath).absoluteFilePath());
    const QFileInfo destinationInfo(normalizedDestinationPath);
    const bool destinationExisted = destinationInfo.exists();
    const QString extractionParent = QDir::fromNativeSeparators(destinationInfo.absolutePath());
    std::unique_ptr<QTemporaryDir> stagedDir;
    QString extractionPath = normalizedDestinationPath;
    bool stagedExtractionFinalized = false;

    if (destinationExisted) {
        QDir destinationDir(normalizedDestinationPath);
        if (!destinationDir.exists() && !QDir().mkpath(normalizedDestinationPath)) {
            if (error) {
                *error = QStringLiteral("Cannot create folder %1").arg(normalizedDestinationPath);
            }
            return false;
        }
    } else {
        if (!QDir().mkpath(extractionParent)) {
            if (error) {
                *error = QStringLiteral("Cannot create folder %1").arg(extractionParent);
            }
            return false;
        }
        cleanupStaleArchiveTemporaryDirs(extractionParent);
        stagedDir = std::make_unique<QTemporaryDir>(
            QDir(extractionParent).filePath(QStringLiteral(".fm-full-extract-XXXXXX")));
        if (!stagedDir->isValid()) {
            if (error) {
                *error = QStringLiteral("Cannot create temporary extraction folder in %1").arg(extractionParent);
            }
            return false;
        }
        stagedDir->setAutoRemove(false);
        extractionPath = QDir::fromNativeSeparators(stagedDir->path());
    }

    QString stagedExtractionLeaseId;
    if (stagedDir && stagedDir->isValid()) {
        CleanupSubsystem::instance().registerArtifact(
            CleanupArtifactKind::ArchiveExtract,
            stagedDir->path(),
            extractionParent,
            true,
            &stagedExtractionLeaseId);
    }

    const auto cleanupStagedExtraction = qScopeGuard([&]() {
        if (!stagedExtractionLeaseId.isEmpty()) {
            if (stagedExtractionFinalized) {
                CleanupSubsystem::instance().completeWithoutDelete(stagedExtractionLeaseId);
            } else {
                CleanupSubsystem::instance().scheduleDeleteOnFailure(stagedExtractionLeaseId);
            }
        } else if (stagedDir && !stagedExtractionFinalized) {
            scheduleRecursiveRemove(extractionPath);
        }
    });

    auto finalizeStagedExtraction = [&]() -> bool {
        if (destinationExisted) {
            return true;
        }

        const QString ownerMarkerPath = QDir(extractionPath).filePath(
            QStringLiteral(".fm-cleanup-owner.json"));
        QByteArray ownerMarkerContents;
        if (QFileInfo::exists(ownerMarkerPath)) {
            QFile ownerMarker(ownerMarkerPath);
            if (!ownerMarker.open(QIODevice::ReadOnly)) {
                if (error) {
                    *error = QStringLiteral("Cannot finalize extracted folder %1: cannot read cleanup marker")
                                 .arg(normalizedDestinationPath);
                }
                return false;
            }
            ownerMarkerContents = ownerMarker.readAll();
            ownerMarker.close();
            if (!QFile::remove(ownerMarkerPath)) {
                if (error) {
                    *error = QStringLiteral("Cannot finalize extracted folder %1: cannot remove cleanup marker")
                                 .arg(normalizedDestinationPath);
                }
                return false;
            }
        }

        if (QFile::rename(extractionPath, normalizedDestinationPath)) {
            stagedExtractionFinalized = true;
            return true;
        }

        if (!ownerMarkerContents.isEmpty()) {
            QFile ownerMarker(ownerMarkerPath);
            if (ownerMarker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                ownerMarker.write(ownerMarkerContents);
            }
        }
        if (error) {
            *error = QStringLiteral("Cannot finalize extracted folder %1").arg(normalizedDestinationPath);
        }
        return false;
    };

    QString fastPathError;
    if (extractArchiveWithSevenZip(normalizedArchivePath, extractionPath, progressCallback, &fastPathError)) {
        if (!finalizeStagedExtraction()) {
            return false;
        }
        return true;
    }
    if (isCompressedTarArchivePath(normalizedArchivePath)) {
        if (error) {
            *error = fastPathError.isEmpty()
                ? QStringLiteral("7-Zip could not extract compressed tar archive")
                : fastPathError;
        }
        return false;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    const auto library = getGlobalLibrary();
    if (!library) {
        if (error) {
            *error = QStringLiteral("bit7z backend was not found or could not be loaded");
        }
        return false;
    }

    const QString suffix = QFileInfo(normalizedArchivePath).suffix().toLower();
    const QStringList candidates = suffix.compare(QStringLiteral("rar"), Qt::CaseInsensitive) == 0
        ? QStringList{rarFormatCandidateForFile(normalizedArchivePath)}
        : archiveFormatCandidatesForSuffix(suffix);

    for (const QString &candidate : candidates) {
        try {
            const auto &format = archiveFormatForSuffix(candidate);
            bit7z::BitArchiveReader reader(
                *library,
                toBit7zString(QDir::toNativeSeparators(normalizedArchivePath)),
                bit7z::ArchiveStartOffset::FileStart,
                format,
                toBit7zString(archivePasswordForPath(normalizedArchivePath)));
            reader.setOverwriteMode(bit7z::OverwriteMode::Skip);
            if (progressCallback) {
                reader.setProgressCallback(progressCallback);
            }
            if (fileCallback) {
                reader.setFileCallback([fileCallback](bit7z::tstring filePath) {
                    fileCallback(toQString(filePath));
                });
            }
            reader.extractTo(toBit7zString(QDir::toNativeSeparators(extractionPath)));
            if (progressCallback) {
                reader.setProgressCallback(nullptr);
            }
            if (fileCallback) {
                reader.setFileCallback(nullptr);
            }
            if (!finalizeStagedExtraction()) {
                return false;
            }
            return true;
        } catch (const std::exception &exception) {
            if (error) {
                *error = QStringLiteral("Extract failed for %1 to %2 using %3: %4")
                    .arg(normalizedArchivePath, normalizedDestinationPath, candidate, QString::fromUtf8(exception.what()));
            }
        }
    }

    if (error && error->isEmpty()) {
        *error = QStringLiteral("Cannot extract archive %1").arg(normalizedArchivePath);
    }
    return false;
#else
    if (error) {
        *error = QStringLiteral("Archive backend is not available");
    }
    Q_UNUSED(progressCallback)
    Q_UNUSED(fileCallback)
    return false;
#endif
}

bool ArchiveFileProvider::extractArchiveEntryTo(const QString &archiveEntryPath,
                                                const QString &destinationFilePath,
                                                QString *error,
                                                const std::function<bool(uint64_t)> &progressCallback)
{
    if (error) {
        error->clear();
    }

    if (!ArchiveSupport::isArchivePath(archiveEntryPath) || archiveEntryPath.isEmpty() || destinationFilePath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Archive entry path or destination is invalid");
        }
        return false;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    const auto library = getGlobalLibrary();
    if (!library) {
        if (error) {
            *error = QStringLiteral("bit7z backend was not found or could not be loaded");
        }
        return false;
    }

    const QString normalizedEntryPath = ArchiveSupport::normalizeArchivePath(archiveEntryPath);
    const QFileInfo destinationInfo(destinationFilePath);
    const QString destinationParent = QDir::fromNativeSeparators(destinationInfo.absolutePath());
    ArchiveState state = buildStateFromScratch(normalizedEntryPath, library, {}, true, {}, destinationParent);
    if (!state.valid || !state.reader) {
        if (error) {
            *error = state.error.isEmpty()
                ? QStringLiteral("Cannot read archive entry %1").arg(normalizedEntryPath)
                : state.error;
        }
        return false;
    }

    const QString rel = archiveRelativeToken(state.browsePath);
    const int idx = state.pathIndex.value(rel, -1);
    if (idx < 0 || idx >= state.items.size() || state.items.at(idx).isDirectory) {
        if (error) {
            *error = QStringLiteral("Archive entry was not found or is not a file: %1").arg(normalizedEntryPath);
        }
        return false;
    }

    const ArchiveItemRecord &record = state.items.at(idx);
    if (!QDir().mkpath(destinationParent)) {
        if (error) {
            *error = QStringLiteral("Cannot create parent directory for %1").arg(destinationFilePath);
        }
        return false;
    }

    if (QFileInfo::exists(destinationFilePath) && !QFile::remove(destinationFilePath)) {
        if (error) {
            *error = QStringLiteral("Cannot replace temporary destination %1").arg(destinationFilePath);
        }
        return false;
    }

    cleanupStaleArchiveTemporaryDirs(destinationParent);
    QTemporaryDir tempDir(QDir(destinationParent).filePath(QStringLiteral(".fm-extract-XXXXXX")));
    if (!tempDir.isValid()) {
        if (error) {
            *error = QStringLiteral("Cannot create temporary extraction folder in %1").arg(destinationParent);
        }
        return false;
    }
    const QString tempRoot = QDir::fromNativeSeparators(tempDir.path());
    tempDir.setAutoRemove(false);
    QString extractLeaseId;
    CleanupSubsystem::instance().registerArtifact(
        CleanupArtifactKind::ArchiveExtract,
        tempDir.path(),
        destinationParent,
        true,
        &extractLeaseId);
    const auto cleanupTempRoot = qScopeGuard([&]() {
        if (!extractLeaseId.isEmpty()) {
            CleanupSubsystem::instance().scheduleDelete(extractLeaseId);
        } else {
            scheduleRecursiveRemove(tempRoot);
        }
    });

    {
        QMutexLocker readerLocker(&archiveReaderMutex());
        if (progressCallback) {
            state.reader->setProgressCallback(progressCallback);
        }
        try {
            state.reader->extractTo(toBit7zString(QDir::toNativeSeparators(tempRoot)), std::vector<uint32_t>{record.index});
            if (progressCallback) {
                state.reader->setProgressCallback(nullptr);
            }
        } catch (const std::exception &exception) {
            if (progressCallback) {
                state.reader->setProgressCallback(nullptr);
            }
            if (error) {
                *error = QStringLiteral("Extract failed for %1: %2")
                    .arg(normalizedEntryPath, QString::fromUtf8(exception.what()));
            }
            return false;
        }
    }

    const QString extractedPath = extractedArchiveItemPath(tempRoot, record.relativePath, record.name);
    if (extractedPath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Extracted archive entry was not found in temporary folder");
        }
        return false;
    }

    if (!QFile::rename(extractedPath, destinationFilePath)) {
        if (error) {
            *error = QStringLiteral("Cannot move extracted file to %1").arg(destinationFilePath);
        }
        QFile::remove(destinationFilePath);
        return false;
    }
    return true;
#else
    if (error) {
        *error = QStringLiteral("Archive backend is not available");
    }
    Q_UNUSED(progressCallback)
    return false;
#endif
}

bool ArchiveFileProvider::extractArchiveEntriesTo(const QStringList &archiveEntryPaths,
                                                  const QStringList &destinationFilePaths,
                                                  QString *error,
                                                  const std::function<bool(uint64_t)> &progressCallback)
{
    if (error) {
        error->clear();
    }

    if (archiveEntryPaths.isEmpty() || archiveEntryPaths.size() != destinationFilePaths.size()) {
        if (error) {
            *error = QStringLiteral("Archive entry selection is invalid");
        }
        return false;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    const auto library = getGlobalLibrary();
    if (!library) {
        if (error) {
            *error = QStringLiteral("bit7z backend was not found or could not be loaded");
        }
        return false;
    }

    const QFileInfo firstDestination(destinationFilePaths.constFirst());
    const QString destinationParent = QDir::fromNativeSeparators(firstDestination.absolutePath());
    if (!QDir().mkpath(destinationParent)) {
        if (error) {
            *error = QStringLiteral("Cannot create parent directory for %1").arg(destinationParent);
        }
        return false;
    }

    const QString firstEntryPath = ArchiveSupport::normalizeArchivePath(archiveEntryPaths.constFirst());
    ArchiveState state = buildStateFromScratch(firstEntryPath, library, {}, true, {}, destinationParent);
    if (!state.valid || !state.reader) {
        if (error) {
            *error = state.error.isEmpty()
                ? QStringLiteral("Cannot read archive entry %1").arg(firstEntryPath)
                : state.error;
        }
        return false;
    }

    const QString container = archiveContainerPart(firstEntryPath);
    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(archiveEntryPaths.size()));
    QStringList relativePaths;
    relativePaths.reserve(archiveEntryPaths.size());

    for (const QString &entryPath : archiveEntryPaths) {
        const QString normalizedEntryPath = ArchiveSupport::normalizeArchivePath(entryPath);
        if (archiveContainerPart(normalizedEntryPath) != container) {
            if (error) {
                *error = QStringLiteral("Selected archive entries belong to different archives");
            }
            return false;
        }

        const QString rel = archiveRelativeToken(archiveBrowsePathForPath(normalizedEntryPath));
        const int idx = state.pathIndex.value(rel, -1);
        if (idx < 0 || idx >= state.items.size() || state.items.at(idx).isDirectory) {
            if (error) {
                *error = QStringLiteral("Archive entry was not found or is not a file: %1").arg(normalizedEntryPath);
            }
            return false;
        }
        indices.push_back(state.items.at(idx).index);
        relativePaths.append(state.items.at(idx).relativePath);
    }

    for (const QString &destinationPath : destinationFilePaths) {
        const QFileInfo destinationInfo(destinationPath);
        if (QDir::fromNativeSeparators(destinationInfo.absolutePath()) != destinationParent) {
            if (error) {
                *error = QStringLiteral("Batch archive extraction requires a single destination folder");
            }
            return false;
        }
        if (QFileInfo::exists(destinationPath) && !QFile::remove(destinationPath)) {
            if (error) {
                *error = QStringLiteral("Cannot replace temporary destination %1").arg(destinationPath);
            }
            return false;
        }
    }

    cleanupStaleArchiveTemporaryDirs(destinationParent);
    QTemporaryDir tempDir(QDir(destinationParent).filePath(QStringLiteral(".fm-extract-XXXXXX")));
    if (!tempDir.isValid()) {
        if (error) {
            *error = QStringLiteral("Cannot create temporary extraction folder in %1").arg(destinationParent);
        }
        return false;
    }
    const QString tempRoot = QDir::fromNativeSeparators(tempDir.path());
    tempDir.setAutoRemove(false);
    QString extractLeaseId;
    CleanupSubsystem::instance().registerArtifact(
        CleanupArtifactKind::ArchiveExtract,
        tempDir.path(),
        destinationParent,
        true,
        &extractLeaseId);
    const auto cleanupTempRoot = qScopeGuard([&]() {
        if (!extractLeaseId.isEmpty()) {
            CleanupSubsystem::instance().scheduleDelete(extractLeaseId);
        } else {
            scheduleRecursiveRemove(tempRoot);
        }
    });

    {
        QMutexLocker readerLocker(&archiveReaderMutex());
        if (progressCallback) {
            state.reader->setProgressCallback(progressCallback);
        }
        try {
            state.reader->extractTo(toBit7zString(QDir::toNativeSeparators(tempRoot)), indices);
            if (progressCallback) {
                state.reader->setProgressCallback(nullptr);
            }
        } catch (const std::exception &exception) {
            if (progressCallback) {
                state.reader->setProgressCallback(nullptr);
            }
            if (error) {
                *error = QStringLiteral("Extract failed for selected archive entries: %1")
                    .arg(QString::fromUtf8(exception.what()));
            }
            return false;
        }
    }

    for (int i = 0; i < relativePaths.size(); ++i) {
        const QString extractedPath = extractedArchiveItemPath(
            tempRoot,
            relativePaths.at(i),
            QFileInfo(relativePaths.at(i)).fileName());
        if (extractedPath.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Extracted archive entry was not found in temporary folder");
            }
            return false;
        }
        if (!QFile::rename(extractedPath, destinationFilePaths.at(i))) {
            if (error) {
                *error = QStringLiteral("Cannot move extracted file to %1").arg(destinationFilePaths.at(i));
            }
            for (int cleanup = 0; cleanup <= i; ++cleanup) {
                QFile::remove(destinationFilePaths.at(cleanup));
            }
            return false;
        }
    }

    return true;
#else
    if (error) {
        *error = QStringLiteral("Archive backend is not available");
    }
    Q_UNUSED(progressCallback)
    return false;
#endif
}

bool ArchiveFileProvider::extractArchiveItemsTo(const QStringList &archiveEntryPaths,
                                                const QStringList &destinationPaths,
                                                QString *error,
                                                const std::function<bool(uint64_t, uint64_t)> &progressCallback)
{
    if (error) {
        error->clear();
    }

    if (archiveEntryPaths.isEmpty() || archiveEntryPaths.size() != destinationPaths.size()) {
        if (error) {
            *error = QStringLiteral("Archive item selection is invalid");
        }
        return false;
    }

    const QString firstEntryPath = ArchiveSupport::normalizeArchivePath(archiveEntryPaths.constFirst());
    if (!ArchiveSupport::isArchivePath(firstEntryPath)) {
        if (error) {
            *error = QStringLiteral("Archive item path is invalid: %1").arg(firstEntryPath);
        }
        return false;
    }
    const QString firstContainer = archiveContainerPart(firstEntryPath);
    if (!archiveNestedDepthAllowed(firstEntryPath, error)) {
        return false;
    }

    const QFileInfo firstDestination(destinationPaths.constFirst());
    const QString destinationParent = QDir::fromNativeSeparators(firstDestination.absolutePath());
    if (!QDir().mkpath(destinationParent)) {
        if (error) {
            *error = QStringLiteral("Cannot create destination folder %1").arg(destinationParent);
        }
        return false;
    }

    std::shared_ptr<ArchiveState> cachedContainerState;
    std::unique_ptr<ArchiveState> ownedContainerState;
    const ArchiveState *metadataState = nullptr;
    QString archivePath;
    if (const auto cachedState = cachedStateForKey(archiveCacheKey(firstEntryPath));
        cachedState
        && cachedState->valid
        && archiveContainerPart(cachedState->currentPath) == firstContainer
        && !cachedState->physicalContainerPath.isEmpty()
        && QFileInfo::exists(cachedState->physicalContainerPath)) {
        cachedContainerState = cachedState;
        metadataState = cachedContainerState.get();
        archivePath = cachedState->physicalContainerPath;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    if (archivePath.isEmpty()) {
        const auto library = getGlobalLibrary();
        if (library) {
            auto state = std::make_unique<ArchiveState>(buildStateFromScratch(
                firstEntryPath,
                library,
                {},
                true,
                {},
                destinationParent,
                [progressCallback](uint64_t processed, uint64_t total) {
                    if (progressCallback) {
                        progressCallback(processed, total);
                    }
                }));
            if (state->valid
                && archiveContainerPart(state->currentPath) == firstContainer
                && !state->physicalContainerPath.isEmpty()
                && QFileInfo::exists(state->physicalContainerPath)) {
                archivePath = state->physicalContainerPath;
                metadataState = state.get();
                ownedContainerState = std::move(state);
            }
        }
    }
#endif

    ResolvedArchiveContainer resolvedContainer;
    const auto cleanupResolvedContainer = qScopeGuard([&resolvedContainer]() {
        releaseTemporaryDirAsync(std::move(resolvedContainer.tempDir));
    });
    if (archivePath.isEmpty()) {
        QString resolveError;
        const QString materializationParent = archiveSourceTemporaryParentPath(firstEntryPath);
        if (!resolveArchiveContainerWithSevenZip(firstEntryPath,
                                                 materializationParent,
                                                 &resolvedContainer,
                                                 &resolveError,
                                                 {})) {
            if (error) {
                *error = resolveError.isEmpty()
                    ? QStringLiteral("7-Zip could not prepare archive container")
                    : resolveError;
            }
            return false;
        }
        archivePath = resolvedContainer.physicalPath;
    }

    QStringList relativePaths;
    QStringList itemPatterns;
    QList<std::optional<FileEntry>> entries;
    relativePaths.reserve(archiveEntryPaths.size());
    itemPatterns.reserve(archiveEntryPaths.size());
    entries.reserve(archiveEntryPaths.size());

    for (const QString &entryPath : archiveEntryPaths) {
        const QString normalizedEntryPath = ArchiveSupport::normalizeArchivePath(entryPath);
        if (!ArchiveSupport::isArchivePath(normalizedEntryPath)
            || archiveContainerPart(normalizedEntryPath) != firstContainer) {
            if (error) {
                *error = QStringLiteral("Selected archive items belong to different archives");
            }
            return false;
        }

        const QString rel = archiveRelativeToken(ArchiveSupport::archiveBrowsePath(normalizedEntryPath));
        if (rel.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Cannot extract archive root as a selected item");
            }
            return false;
        }

        std::optional<FileEntry> entry;
        if (metadataState) {
            const int absoluteIdx = metadataState->pathIndex.value(rel, -1);
            if (absoluteIdx >= 0 && absoluteIdx < metadataState->items.size()) {
                entry = fileEntryFromRecord(*metadataState, metadataState->items.at(absoluteIdx));
            }
        }
        if (!entry) {
            entry = cachedEntryInfo(normalizedEntryPath);
        }

        relativePaths.append(rel);
        if (entry && entry->isDirectory) {
            itemPatterns.append(rel + QStringLiteral("/*"));
        } else {
            itemPatterns.append(rel);
        }
        entries.append(std::move(entry));
    }

    for (const QString &destinationPath : destinationPaths) {
        const QFileInfo destinationInfo(destinationPath);
        if (QDir::fromNativeSeparators(destinationInfo.absolutePath()) != destinationParent) {
            if (error) {
                *error = QStringLiteral("Batch archive extraction requires a single destination folder");
            }
            return false;
        }
        if (QFileInfo::exists(destinationPath)) {
            if (error) {
                *error = QStringLiteral("Destination already exists: %1").arg(destinationPath);
            }
            return false;
        }
    }

    cleanupStaleArchiveTemporaryDirs(destinationParent);
    QTemporaryDir tempDir(QDir(destinationParent).filePath(QStringLiteral(".fm-7z-extract-XXXXXX")));
    if (!tempDir.isValid()) {
        if (error) {
            *error = QStringLiteral("Cannot create temporary extraction folder in %1").arg(destinationParent);
        }
        return false;
    }
    const QString tempRoot = QDir::fromNativeSeparators(tempDir.path());
    tempDir.setAutoRemove(false);
    QString extractLeaseId;
    CleanupSubsystem::instance().registerArtifact(
        CleanupArtifactKind::ArchiveExtract,
        tempDir.path(),
        destinationParent,
        true,
        &extractLeaseId);
    const auto cleanupTempRoot = qScopeGuard([&]() {
        if (!extractLeaseId.isEmpty()) {
            CleanupSubsystem::instance().scheduleDelete(extractLeaseId);
        } else {
            scheduleRecursiveRemove(tempRoot);
        }
    });

    QString fastPathError;
    if (!extractArchiveWithSevenZip(archivePath,
                                    tempRoot,
                                    [](uint64_t) {
                                        return !ArchiveOperationCallbacks::current().isAbortRequested();
                                    },
                                    &fastPathError,
                                    itemPatterns,
                                    [progressCallback](uint64_t processed, uint64_t total) {
                                        if (progressCallback) {
                                            progressCallback(processed, total);
                                        }
                                    },
                                    archivePasswordForPath(firstEntryPath))) {
        if (error) {
            *error = fastPathError.isEmpty()
                ? QStringLiteral("7-Zip could not extract selected archive items")
                : fastPathError;
        }
        return false;
    }

    for (int i = 0; i < relativePaths.size(); ++i) {
        if (ArchiveOperationCallbacks::current().isAbortRequested()) {
            if (error) {
                *error = QStringLiteral("Archive extraction was cancelled");
            }
            return false;
        }

        const QString extractedPath = QDir(tempRoot).filePath(relativePaths.at(i));
        const QString destinationPath = destinationPaths.at(i);
        const auto &entry = entries.at(i);
        if (!QFileInfo::exists(extractedPath)) {
            if (entry && entry->isDirectory && QDir().mkpath(destinationPath)) {
                continue;
            }
            if (error) {
                *error = QStringLiteral("Extracted archive item was not found in temporary folder: %1")
                    .arg(relativePaths.at(i));
            }
            return false;
        }

        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath())
            || !moveExtractedPath(extractedPath, destinationPath)) {
            if (error) {
                *error = QStringLiteral("Cannot move extracted item to %1").arg(destinationPath);
            }
            return false;
        }
    }

    return true;
}
