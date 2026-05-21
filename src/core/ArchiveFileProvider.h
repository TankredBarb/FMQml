#pragma once

#include "FileProvider.h"
#include "ArchiveCache.h"

#include <QString>
#include <QDateTime>
#include <memory>
#include <optional>

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
    struct ArchiveContext {
        QString archiveDiskPath;
        QString internalPath;
    };
    
    std::optional<ArchiveContext> parsePath(const QString &fullPath) const;
    bool loadArchive(const QString &archiveDiskPath);
    ArchiveNode* findNode(const QString &internalPath) const;
    FileEntry entryFromNode(const ArchiveContext &ctx, const ArchiveNode *node) const;

    QString m_currentPath;
    int m_generation = 0;
    bool m_isRunning = false;
    bool m_showHidden = false;
    bool m_abort = false;
    
    QString m_loadedArchiveDiskPath;
    ArchiveNode* m_rootNode = nullptr;
    std::shared_ptr<bit7z::BitArchiveReader> m_reader;
};
