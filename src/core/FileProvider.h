#pragma once

#include <QObject>
#include <QDateTime>
#include <QIODevice>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <functional>
#include <memory>
#include <optional>

struct LocalFileCopyItem {
    QString sourceFilePath;
    QString destinationPath;
    qint64 size = 0;
};

struct LocalFileMaterializeItem {
    QString sourcePath;
    QString destinationFilePath;
    qint64 size = 0;
};

struct FileEntry {
    QString name;
    QString path;
    QString suffix;
    qint64 size = 0;
    QString sizeText;
    QString modifiedText;
    QString createdText;
    QString attributesText;
    QString providerCapabilitiesText;
    QString iconName;
    QString mimeType;
    QString shortcutOpenPath;
    QString shortcutTargetPath;
    QString shortcutTargetMimeType;
    QString shortcutTargetResourceKey;
    QDateTime modified;
    QDateTime created;
    bool isDirectory = false;
    bool isHidden = false;
    bool isSelected = false;
    bool isImage = false;
    bool hasThumbnail = false;
    bool isReadOnly = false;
    bool isSystem = false;
    bool isShortcut = false;
    bool shortcutTargetIsDirectory = false;
};
Q_DECLARE_METATYPE(FileEntry)

class FileProvider : public QObject {
    Q_OBJECT

public:
    enum Capability {
        Browse = 0x01,
        ReadMetadata = 0x02,
        Create = 0x04,
        Rename = 0x08,
        Remove = 0x10,
        Transfer = 0x20,
        Watch = 0x40
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    explicit FileProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~FileProvider() override = default;

    virtual QString scheme() const = 0;
    virtual bool canHandle(const QString &path) const = 0;
    virtual Capabilities capabilities() const = 0;
    virtual bool canCreateChildren(const QString &path) const
    {
        Q_UNUSED(path)
        return capabilities() & Create;
    }
    virtual bool canRemovePath(const QString &path) const
    {
        Q_UNUSED(path)
        return capabilities() & Remove;
    }
    virtual bool canCopyPath(const QString &path) const
    {
        Q_UNUSED(path)
        return capabilities() & Transfer;
    }
    virtual bool isReadOnlyContainer(const QString &path) const
    {
        Q_UNUSED(path)
        return false;
    }

    virtual void scan(const QString &path) = 0;
    virtual void refresh(const QString &path)
    {
        scan(path);
    }
    virtual void cancel() = 0;
    virtual void setShowHidden(bool show) = 0;
    virtual bool isRunning() const = 0;
    virtual QString currentPath() const = 0;
    virtual int currentGeneration() const = 0;
    virtual bool pathExists(const QString &path) const = 0;
    virtual bool isDirectory(const QString &path) const = 0;
    virtual bool isSymLink(const QString &path) const = 0;
    virtual QString normalizedPath(const QString &path) const = 0;
    virtual QString fileName(const QString &path) const = 0;
    virtual QString localCopyFileName(const QString &path) const { return fileName(path); }
    virtual QString thumbnailUrl(const QString &path) const
    {
        Q_UNUSED(path)
        return {};
    }
    virtual QString absolutePath(const QString &path) const = 0;
    virtual QString parentPath(const QString &path) const = 0;
    virtual QString childPath(const QString &parentPath, const QString &name) const = 0;
    virtual std::optional<FileEntry> entryInfo(const QString &path) const = 0;
    virtual bool ensureParentDirectory(const QString &path) const = 0;
    virtual bool makePath(const QString &path) const = 0;
    virtual bool removePath(const QString &path) const = 0;
    virtual QStringList childPaths(const QString &path, bool includeHidden = true) const = 0;
    virtual bool movePath(const QString &sourcePath, const QString &destinationPath) const = 0;
    virtual std::unique_ptr<QIODevice> openRead(const QString &path) const = 0;
    virtual std::unique_ptr<QIODevice> openRead(const QString &path, const QString &stagingParentPath) const
    {
        Q_UNUSED(stagingParentPath)
        return openRead(path);
    }
    virtual bool copyToLocalFile(const QString &sourcePath,
                                 const QString &destinationFilePath,
                                 const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                 QString *error) const
    {
        Q_UNUSED(sourcePath)
        Q_UNUSED(destinationFilePath)
        Q_UNUSED(progress)
        Q_UNUSED(error)
        return false;
    }
    virtual bool supportsLocalFileBatchMaterialize() const { return false; }
    virtual bool copyToLocalFiles(const QVector<LocalFileMaterializeItem> &items,
                                  const std::function<bool(const QString &currentSourcePath, qint64 processedBytes, qint64 totalBytes)> &progress,
                                  QString *error) const
    {
        Q_UNUSED(items)
        Q_UNUSED(progress)
        Q_UNUSED(error)
        return false;
    }
    virtual bool copyFromLocalFile(const QString &sourceFilePath,
                                   const QString &destinationPath,
                                   const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                   QString *error) const
    {
        Q_UNUSED(sourceFilePath)
        Q_UNUSED(destinationPath)
        Q_UNUSED(progress)
        Q_UNUSED(error)
        return false;
    }
    virtual bool supportsLocalFileBatchCopy() const { return false; }
    virtual bool copyFromLocalFiles(const QVector<LocalFileCopyItem> &items,
                                    const std::function<bool(const QString &currentFilePath, qint64 processedBytes, qint64 totalBytes)> &progress,
                                    QString *error) const
    {
        Q_UNUSED(items)
        Q_UNUSED(progress)
        Q_UNUSED(error)
        return false;
    }
    virtual std::unique_ptr<QIODevice> openWrite(const QString &path, bool truncate = true) const = 0;
    virtual bool renamePath(const QString &oldPath, const QString &newName) = 0;
    virtual bool createFolder(const QString &parentPath, const QString &name, QString *createdPath = nullptr) = 0;
    virtual bool createFile(const QString &parentPath, const QString &name, QString *createdPath = nullptr) = 0;
    virtual QVariantMap storageInfo(const QString &path) const
    {
        Q_UNUSED(path)
        return {};
    }
    virtual void flushPendingStorageInfoRefresh() const {}
    virtual QString lastErrorString() const { return {}; }
    virtual void clearLastError() const {}

signals:
    void started();
    void batchReady(const QList<FileEntry> &entries, int generation);
    void progress(qint64 processedBytes, qint64 totalBytes, const QString &message, int generation);
    void statusMessage(const QString &message);
    void finished(const QString &path, bool success, int generation, const QString &error = {});
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileProvider::Capabilities)
