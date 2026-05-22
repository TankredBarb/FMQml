#pragma once

#include "FileProvider.h"

#include <QHash>
#include <QSet>
#include <QBuffer>
#include <chrono>
#include <string>
#include <memory>

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
        QString sourcePath;
        QString currentPath;
        QString browsePath;
        QString archivePrefix;
        QList<ArchiveItemRecord> items;
        QHash<QString, int> pathIndex;
        QSet<QString> directories;
        std::unique_ptr<QTemporaryFile> tempFile;
        std::unique_ptr<bit7z::BitArchiveReader> reader;
        bool valid = false;
        QString error;
    };

    bool ensureLibrary() const;
    ArchiveState buildState(const QString &path) const;
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

    mutable std::shared_ptr<bit7z::Bit7zLibrary> m_library;
    bool m_showHidden = false;
    bool m_running = false;
    int m_generation = 0;
    QString m_currentPath;
    mutable ArchiveState m_state;
};
