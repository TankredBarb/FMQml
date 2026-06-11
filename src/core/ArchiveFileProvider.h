#pragma once

#include "FileProvider.h"

#include <QHash>
#include <QSet>
#include <QBuffer>
#include <QFuture>
#include <QMutex>
#include <chrono>
#include <string>
#include <memory>
#include <atomic>
#include <functional>

#include <QTemporaryDir>
#include <QTemporaryFile>

namespace bit7z {
class Bit7zLibrary;
class BitArchiveReader;
class BitArchiveItemInfo;
}

class ArchiveFileProvider final : public FileProvider {
    Q_OBJECT

public:
    explicit ArchiveFileProvider(QObject *parent = nullptr);
    ~ArchiveFileProvider() override;

    QString scheme() const override;
    bool canHandle(const QString &path) const override;
    Capabilities capabilities() const override;
    void scan(const QString &path) override;
    void cancel() override;
    void setShowHidden(bool show) override;
    bool isRunning() const override;
    QString currentPath() const override;
    int currentGeneration() const override;
    bool pathExists(const QString &path) const override;
    bool isDirectory(const QString &path) const override;
    bool isSymLink(const QString &path) const override;
    QString normalizedPath(const QString &path) const override;
    QString fileName(const QString &path) const override;
    QString absolutePath(const QString &path) const override;
    QString parentPath(const QString &path) const override;
    QString childPath(const QString &parentPath, const QString &name) const override;
    std::optional<FileEntry> entryInfo(const QString &path) const override;
    bool ensureParentDirectory(const QString &path) const override;
    bool makePath(const QString &path) const override;
    bool removePath(const QString &path) const override;
    QStringList childPaths(const QString &path, bool includeHidden = true) const override;
    bool movePath(const QString &sourcePath, const QString &destinationPath) const override;
    std::unique_ptr<QIODevice> openRead(const QString &path) const override;
    std::unique_ptr<QIODevice> openWrite(const QString &path, bool truncate = true) const override;
    bool renamePath(const QString &oldPath, const QString &newName) override;
    bool createFolder(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override;
    bool createFile(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override;

    static std::optional<FileEntry> cachedEntryInfo(const QString &path);
    static std::optional<FileEntry> entryInfoForPath(const QString &path);
    static QByteArray readCachedFilePrefix(const QString &path, qint64 maxEntrySize, qint64 maxBytes, bool *tooLarge = nullptr);
    static void setCurrentThreadTemporaryParent(const QString &path);
    static void invalidateCacheForPath(const QString &path);
    static bool hasCachedContainerForPath(const QString &path);
    static bool needsPasswordForPath(const QString &path);
    static bool errorNeedsPassword(const QString &error);
    static void setPasswordForPath(const QString &path, const QString &password);
    static void clearPasswordForPath(const QString &path);
    static QString archivePasswordForPath(const QString &path);
    static bool extractArchiveFileTo(const QString &archivePath,
                                     const QString &destinationPath,
                                     QString *error = nullptr,
                                     const std::function<bool(uint64_t)> &progressCallback = {},
                                     const std::function<void(const QString &)> &fileCallback = {});
    static bool extractArchiveEntryTo(const QString &archiveEntryPath,
                                      const QString &destinationFilePath,
                                      QString *error = nullptr,
                                      const std::function<bool(uint64_t)> &progressCallback = {});
    static bool extractArchiveEntriesTo(const QStringList &archiveEntryPaths,
                                        const QStringList &destinationFilePaths,
                                        QString *error = nullptr,
                                        const std::function<bool(uint64_t)> &progressCallback = {});
    static bool extractArchiveItemsTo(const QStringList &archiveEntryPaths,
                                      const QStringList &destinationPaths,
                                      QString *error = nullptr,
                                      const std::function<bool(uint64_t)> &progressCallback = {});

private:
    struct ArchiveItemRecord {
        QString relativePath;
        QString absolutePath;
        QString name;
        QString suffix;
        qint64 size = 0;
        QDateTime modified;
        QDateTime created;
        bool isDirectory = false;
        bool isHidden = false;
        bool isSymLink = false;
        bool isArchive = false;
        uint32_t index = 0;
    };

    struct ArchiveState {
        ArchiveState() = default;
        ~ArchiveState();
        ArchiveState(ArchiveState &&) noexcept = default;
        ArchiveState &operator=(ArchiveState &&) noexcept = default;
        ArchiveState(const ArchiveState &) = delete;
        ArchiveState &operator=(const ArchiveState &) = delete;

        QString sourcePath;
        QString physicalContainerPath;
        QString currentPath;
        QString browsePath;
        QString archivePrefix;
        QList<ArchiveItemRecord> items;
        QHash<QString, int> pathIndex;
        QSet<QString> directories;
        std::unique_ptr<QTemporaryDir> tempDir;
        std::unique_ptr<QTemporaryFile> tempFile;
        std::unique_ptr<bit7z::BitArchiveReader> reader;
        bool valid = false;
        QString error;
    };

    bool ensureLibrary() const;
    static ArchiveState buildStateFromScratch(const QString &path,
                                              const std::shared_ptr<bit7z::Bit7zLibrary> &library,
                                              const std::function<void(const QList<FileEntry> &)> &batchCallback = {},
                                              bool showHidden = true,
                                              const std::shared_ptr<std::atomic_bool> &cancelled = {},
                                              const QString &temporaryParentPath = {},
                                              const std::function<void(uint64_t, uint64_t)> &progressReporter = {});
    ArchiveState stateForPath(const QString &path) const;
    std::shared_ptr<ArchiveState> cachedStateForPath(const QString &path, QString *browsePath = nullptr) const;
    static std::unique_ptr<QIODevice> openReadFromState(const ArchiveState &state, const QString &browsePath);
    static QList<FileEntry> visibleEntriesForState(const ArchiveState &state, bool showHidden);
    static QList<FileEntry> visibleEntriesForBrowse(const ArchiveState &state, const QString &browsePath, bool showHidden);
    static QString archiveContainerPart(const QString &path);
    static QString archiveBrowsePathForPath(const QString &path);
    static QString archiveCacheKey(const QString &path);
    static QString archivePasswordCacheKey(const QString &path);
    static QHash<QString, std::shared_ptr<ArchiveState>> &archiveCache();
    static QStringList &archiveCacheOrder();
    static QMutex &archiveCacheMutex();
    static QHash<QString, QString> &archivePasswords();
    static QMutex &archivePasswordMutex();
    static std::shared_ptr<ArchiveState> cachedStateForKey(const QString &key);
    static void storeStateInCache(const QString &key, const std::shared_ptr<ArchiveState> &state);
    static QString toArchiveToken(const QString &path);
    static QString normalizeRelativePath(QString path);
    static QString parentRelativePath(const QString &path);
    static QString joinRelativePath(const QString &parent, const QString &child);
    static bool isArchiveLike(const QString &suffix);
    static std::string toBit7zString(const QString &path);
    static QDateTime toDateTime(const std::chrono::time_point<std::chrono::system_clock> &timePoint);
    static QString itemAbsolutePath(const QString &archivePrefix, const QString &relativePath);
    static FileEntry fileEntryFromRecord(const ArchiveState &state, const ArchiveItemRecord &record);
    static QString currentBrowsePathFromPath(const QString &path);
    void cancelCurrentScan(bool invalidateCache);

    mutable std::shared_ptr<bit7z::Bit7zLibrary> m_library;
    QFuture<void> m_scanFuture;
    bool m_showHidden = false;
    std::atomic<bool> m_running{false};
    std::atomic<int> m_generation{0};
    std::shared_ptr<std::atomic_bool> m_cancelled;
    QString m_currentPath;
    mutable std::shared_ptr<ArchiveState> m_state;
};
