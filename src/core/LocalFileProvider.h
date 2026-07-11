#pragma once

#include "FileProvider.h"

#include <QFuture>
#include <QList>
#include <memory>
#include <atomic>

class LocalFileProvider final : public FileProvider {
    Q_OBJECT

public:
    explicit LocalFileProvider(QObject *parent = nullptr);
    ~LocalFileProvider() override;

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
    bool copyToLocalFileForPreview(
        const QString &sourcePath,
        const QString &destinationFilePath,
        const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
        QString *error) const override;
    std::unique_ptr<QIODevice> openWrite(const QString &path, bool truncate = true) const override;
    bool renamePath(const QString &oldPath, const QString &newName) override;
    bool createFolder(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override;
    bool createFile(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override;
    QString lastErrorString() const override;
    void clearLastError() const override;

private:
    void pruneFinishedScans();
    void setLastError(const QString &error) const;

    QList<QFuture<void>> m_scanFutures;
    QString m_currentPath;
    std::atomic<int> m_scanGeneration{0};
    bool m_showHidden = false;
    mutable QString m_lastError;
};
