#pragma once

#include "FileProvider.h"
#include "SftpClient.h"

#include <QFuture>
#include <QHash>
#include <QMutex>
#include <atomic>

class SftpFileProvider final : public FileProvider {
    Q_OBJECT
public:
    explicit SftpFileProvider(const QString &knownHostsFile = {});
    ~SftpFileProvider() override;
    QString scheme() const override { return QStringLiteral("sftp"); }
    bool canHandle(const QString &path) const override;
    Capabilities capabilities() const override { return Browse | ReadMetadata | Transfer | Create | Rename | Remove | PreserveDestinationOnTransfer; }
    bool canCreateChildren(const QString &path) const override;
    void scan(const QString &path) override;
    void cancel() override;
    void setShowHidden(bool show) override { m_showHidden = show; }
    bool isRunning() const override;
    QString currentPath() const override { return m_currentPath; }
    int currentGeneration() const override { return m_generation.load(); }
    bool pathExists(const QString &path) const override;
    bool isDirectory(const QString &path) const override;
    bool isSymLink(const QString &path) const override;
    QString normalizedPath(const QString &path) const override;
    QString fileName(const QString &path) const override;
    QString absolutePath(const QString &path) const override { return normalizedPath(path); }
    QString parentPath(const QString &path) const override;
    QString childPath(const QString &parentPath, const QString &name) const override;
    std::optional<FileEntry> entryInfo(const QString &path) const override;
    bool ensureParentDirectory(const QString &path) const override;
    bool makePath(const QString &path) const override;
    bool removePath(const QString &path) const override;
    QStringList childPaths(const QString &path, bool includeHidden = true) const override;
    bool movePath(const QString &source, const QString &destination) const override;
    std::unique_ptr<QIODevice> openRead(const QString &) const override { unsupported(); return nullptr; }
    std::unique_ptr<QIODevice> openWrite(const QString &, bool = true) const override { unsupported(); return nullptr; }
    bool copyToLocalFile(const QString &source, const QString &destination,
                         const std::function<bool(qint64, qint64)> &progress, QString *error) const override;
    bool copyFromLocalFile(const QString &source, const QString &destination,
                           const std::function<bool(qint64, qint64)> &progress, QString *error) const override;
    bool renamePath(const QString &path, const QString &newName) override;
    bool createFolder(const QString &, const QString &, QString *createdPath = nullptr) override;
    bool createFile(const QString &, const QString &, QString *createdPath = nullptr) override;
    QString lastErrorString() const override;
    void clearLastError() const override;

private:
    bool unsupported() const;
    void setError(const QString &error) const;
    std::unique_ptr<SftpClient> connectedClient(const QString &path) const;
    void invalidateEntries() const;
    bool m_showHidden = false;
    QString m_currentPath;
    QString m_knownHostsFile;
    std::atomic_int m_generation{0};
    QList<QFuture<void>> m_scans;
    mutable QMutex m_mutex;
    mutable QHash<QString, FileEntry> m_entries;
    mutable std::shared_ptr<std::atomic_bool> m_cachedSession;
    mutable QString m_error;
};
