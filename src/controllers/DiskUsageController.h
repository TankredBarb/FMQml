#pragma once

#include "../core/DiskUsageScanner.h"
#include "../models/DiskUsageModel.h"

#include <QObject>
#include <QPointer>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <memory>

class DiskUsageController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool contentsBusy READ contentsBusy NOTIFY stateChanged)
    Q_PROPERTY(bool onDiskBusy READ onDiskBusy NOTIFY spaceInfoChanged)
    Q_PROPERTY(bool onDiskStopped READ onDiskStopped NOTIFY spaceInfoChanged)
    Q_PROPERTY(QString onDiskProgressText READ onDiskProgressText NOTIFY spaceInfoChanged)
    Q_PROPERTY(QString rootPath READ rootPath NOTIFY rootPathChanged)
    Q_PROPERTY(QString displayRootPath READ displayRootPath NOTIFY rootPathChanged)
    Q_PROPERTY(QVariantList breadcrumbEntries READ breadcrumbEntries NOTIFY rootPathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationChanged)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY rootPathChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY progressChanged)
    Q_PROPERTY(QString currentDisplayPath READ currentDisplayPath NOTIFY progressChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY progressChanged)
    Q_PROPERTY(bool cached READ cached NOTIFY cacheStateChanged)
    Q_PROPERTY(QString cacheStatusText READ cacheStatusText NOTIFY cacheStateChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY progressChanged)
    Q_PROPERTY(QString totalBytesText READ totalBytesText NOTIFY progressChanged)
    Q_PROPERTY(QString storageUsedText READ storageUsedText NOTIFY rootPathChanged)
    Q_PROPERTY(QString storageTotalText READ storageTotalText NOTIFY rootPathChanged)
    Q_PROPERTY(QString fileSystemType READ fileSystemType NOTIFY rootPathChanged)
    Q_PROPERTY(QString onDiskText READ onDiskText NOTIFY spaceInfoChanged)
    Q_PROPERTY(QString onDiskNote READ onDiskNote NOTIFY spaceInfoChanged)
    Q_PROPERTY(int onDiskSkippedPaths READ onDiskSkippedPaths NOTIFY spaceInfoChanged)
    Q_PROPERTY(int scannedFiles READ scannedFiles NOTIFY progressChanged)
    Q_PROPERTY(int scannedFolders READ scannedFolders NOTIFY progressChanged)
    Q_PROPERTY(int inaccessiblePaths READ inaccessiblePaths NOTIFY progressChanged)
    Q_PROPERTY(QString coverageStatusText READ coverageStatusText NOTIFY progressChanged)
    Q_PROPERTY(QVariantList skippedDetailEntries READ skippedDetailEntries NOTIFY progressChanged)
    Q_PROPERTY(QVariantMap sunburstRoot READ sunburstRoot NOTIFY progressChanged)
    Q_PROPERTY(DiskUsageModel *summaryModel READ summaryModel CONSTANT)
    Q_PROPERTY(DiskUsageModel *rootChildrenModel READ rootChildrenModel CONSTANT)
    Q_PROPERTY(DiskUsageModel *largestFoldersModel READ largestFoldersModel CONSTANT)
    Q_PROPERTY(DiskUsageModel *largestFilesModel READ largestFilesModel CONSTANT)

public:
    enum class State {
        Idle,
        Scanning,
        Canceling,
        Finished,
        Failed
    };
    Q_ENUM(State)

    explicit DiskUsageController(QObject *parent = nullptr);
    ~DiskUsageController() override;

    State state() const;
    bool busy() const;
    bool contentsBusy() const;
    bool onDiskBusy() const;
    bool onDiskStopped() const;
    QString onDiskProgressText() const;
    QString rootPath() const;
    QString displayRootPath() const;
    QVariantList breadcrumbEntries() const;
    bool canGoBack() const;
    bool canGoUp() const;
    QString currentPath() const;
    QString currentDisplayPath() const;
    QString error() const;
    QString lastError() const;
    bool cached() const;
    QString cacheStatusText() const;
    qint64 totalBytes() const;
    QString totalBytesText() const;
    QString storageUsedText() const;
    QString storageTotalText() const;
    QString fileSystemType() const;
    QString onDiskText() const;
    QString onDiskNote() const;
    int onDiskSkippedPaths() const;
    int scannedFiles() const;
    int scannedFolders() const;
    int inaccessiblePaths() const;
    QString coverageStatusText() const;
    QVariantList skippedDetailEntries() const;
    QVariantMap sunburstRoot() const;
    DiskUsageModel *summaryModel();
    DiskUsageModel *rootChildrenModel();
    DiskUsageModel *largestFoldersModel();
    DiskUsageModel *largestFilesModel();

    Q_INVOKABLE bool canAnalyzePath(const QString &path) const;
    Q_INVOKABLE void scan(const QString &path);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void navigateTo(const QString &path);
    Q_INVOKABLE void navigateBack();
    Q_INVOKABLE void navigateUp();
    Q_INVOKABLE bool revealPath(const QString &path) const;
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clear();

signals:
    void stateChanged();
    void rootPathChanged();
    void progressChanged();
    void errorChanged();
    void cacheStateChanged();
    void navigationChanged();
    void spaceInfoChanged();

private:
    friend class DiskUsageControllerTestAccess;
    struct CachedScan {
        int generation = -1;
        QString rootPath;
        QList<DiskUsageEntry> folders;
        QList<DiskUsageEntry> files;
        QList<DiskUsageEntry> rootChildren;
        QVariantMap sunburstRoot;
        qint64 totalBytes = 0;
        int scannedFiles = 0;
        int scannedFolders = 0;
        int skippedPaths = 0;
        int inaccessiblePaths = 0;
        int reparsePaths = 0;
        QStringList inaccessiblePathDetails;
        QStringList reparsePathDetails;
        QString lastError;
        QDateTime timestamp;
        QString onDiskText = {};
        QString onDiskNote = {};
        int onDiskSkippedPaths = 0;
        bool onDiskStopped = false;
    };

    QString cacheKeyForPath(const QString &path) const;
    bool tryLoadCache(const QString &path);
    void storeCache(const QList<DiskUsageEntry> &folders,
                    const QList<DiskUsageEntry> &files,
                    const QList<DiskUsageEntry> &rootChildren,
                    const QVariantMap &sunburstRoot,
                    qint64 totalBytes,
                    int scannedFiles,
                    int scannedFolders,
                    int skippedPaths,
                    int inaccessiblePaths,
                    int reparsePaths,
                    const QStringList &inaccessiblePathDetails,
                    const QStringList &reparsePathDetails);
    void setCached(bool cached, const QDateTime &timestamp = {});
    void startScan(const QString &path, bool forceRescan);
    void setState(State state);
    void setError(const QString &error);
    void resetProgress();
    void measureDiskUsage();
    void cacheSpaceInfo();
    void applySnapshot(const QList<DiskUsageEntry> &folders,
                       const QList<DiskUsageEntry> &files,
                       const QList<DiskUsageEntry> &rootChildren,
                       qint64 totalBytes,
                       int scannedFiles,
                       int scannedFolders,
                       int skippedPaths,
                       int inaccessiblePaths,
                       int reparsePaths,
                       const QStringList &inaccessiblePathDetails,
                       const QStringList &reparsePathDetails,
                       const QString &currentPath,
                       const QString &lastError);

    State m_state = State::Idle;
    QString m_rootPath;
    QString m_currentPath;
    QString m_error;
    QString m_lastError;
    QString m_onDiskText;
    QString m_onDiskNote;
    QString m_onDiskProgress;
    int m_onDiskSkippedPaths = 0;
    bool m_spaceBusy = false;
    bool m_spaceStopped = false;
    qint64 m_spaceFiles = 0;
    std::shared_ptr<std::atomic_bool> m_spaceCancelled;
    qint64 m_totalBytes = 0;
    int m_scannedFiles = 0;
    int m_scannedFolders = 0;
    int m_skippedPaths = 0;
    int m_inaccessiblePaths = 0;
    int m_reparsePaths = 0;
    QStringList m_inaccessiblePathDetails;
    QStringList m_reparsePathDetails;
    QVariantMap m_sunburstRoot;
    int m_generation = 0;
    bool m_cached = false;
    QDateTime m_cacheTimestamp;
    QStringList m_backStack;
    QHash<QString, CachedScan> m_cache;
    QPointer<DiskUsageScanner> m_scanner;
    QThreadPool m_scanPool;
    DiskUsageModel m_rootChildrenModel;
    DiskUsageModel m_summaryModel;
    DiskUsageModel m_largestFoldersModel;
    DiskUsageModel m_largestFilesModel;
};
