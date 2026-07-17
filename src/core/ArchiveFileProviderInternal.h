#pragma once

#include "CleanupSubsystem.h"

#include <QMutex>
#include <QTemporaryDir>

#include <functional>
#include <memory>

#ifdef HAS_UNOFFICIAL_BIT7Z
#include <bit7z/bittypes.hpp>
#endif

namespace bit7z {
class Bit7zLibrary;
class BitInFormat;
}

namespace ArchiveFileProviderInternal {

inline constexpr qsizetype kMaxCachedArchiveStates = 8;
inline constexpr qsizetype kMaxCachedArchiveItems = 250000;

struct ResolvedArchiveContainer {
    QString physicalPath;
    std::unique_ptr<QTemporaryDir> tempDir;
};

std::shared_ptr<bit7z::Bit7zLibrary> getGlobalLibrary();
QMutex &archiveReaderMutex();
QString extractedArchiveItemPath(const QString &rootPath, const QString &relativePath, const QString &itemName);
QStringList sampledExtractedFiles(const QString &rootPath, int limit = 12);
bool extractArchiveWithSevenZip(const QString &archivePath,
                                const QString &destinationPath,
                                const std::function<bool(uint64_t)> &progressCallback,
                                QString *error,
                                const QStringList &itemPaths = {},
                                const std::function<void(uint64_t, uint64_t)> &progressReporter = {},
                                const QString &passwordOverride = {},
                                uint64_t expectedOutputBytes = 0);
bool moveExtractedPath(const QString &sourcePath, const QString &destinationPath);
QString registerArchiveTemporaryDirectory(const QString &path,
                                          const QString &allowedRoot,
                                          CleanupArtifactKind kind);
void scheduleArchiveTemporaryDirectoryCleanup(const QString &path);
void scheduleRecursiveRemove(QString path);
void releaseTemporaryDirAsync(std::unique_ptr<QTemporaryDir> tempDir);
void cleanupStaleArchiveTemporaryDirs(const QString &parentPath);
bool isCompressedTarArchivePath(const QString &archivePath);
QString archiveTemporaryParentPath(const QString &temporaryParentPath);
QString archiveSourceTemporaryParentPath(const QString &archivePath);
bool archiveNestedDepthAllowed(const QString &path, QString *error = nullptr);
bool resolveArchiveContainerWithSevenZip(const QString &archivePath,
                                         const QString &temporaryParentPath,
                                         ResolvedArchiveContainer *resolved,
                                         QString *error,
                                         const std::function<bool(uint64_t)> &progressCallback = {},
                                         const std::function<void(uint64_t, uint64_t)> &progressReporter = {});
QString archiveTokenPath(const QString &path);
QString archiveRelativeToken(const QString &token);
QString archiveSuffixFromName(const QString &name);
const bit7z::BitInFormat &archiveFormatForSuffix(const QString &suffix);
QStringList archiveFormatCandidatesForSuffix(const QString &suffix);
QString rarFormatCandidateForFile(const QString &path);
#ifdef HAS_UNOFFICIAL_BIT7Z
QString toQString(const bit7z::tstring &value);
#endif

} // namespace ArchiveFileProviderInternal
