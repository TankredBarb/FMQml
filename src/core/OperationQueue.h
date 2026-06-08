#pragma once

#include <QFutureWatcher>
#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QMutex>
#include <QWaitCondition>
#include <QtQml>
#include <QElapsedTimer>
#include <memory>
#include <atomic>
#include <QHash>
#include <functional>

#include "LocalFileProvider.h"

class OperationQueue : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Enums and signals only")
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentLabel READ currentLabel NOTIFY currentLabelChanged)
    Q_PROPERTY(QString error READ error WRITE setError NOTIFY errorChanged)
    Q_PROPERTY(QVariantMap lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int completedItems READ completedItems NOTIFY progressChanged)
    Q_PROPERTY(int totalItems READ totalItems NOTIFY progressChanged)
    Q_PROPERTY(QString speedText READ speedText NOTIFY speedChanged)
    Q_PROPERTY(QString remainingTimeText READ remainingTimeText NOTIFY speedChanged)

public:
    enum class Type {
        Copy,
        Duplicate,
        Move,
        Delete,
        Extract,
        Compress
    };

    // Not a best place but let it will be here for now :)
    enum class DriveStorageType {
        Unknown,
        HDD,
        SATA_SSD,
        NVME_SSD,
        USB_Flash
    };

    struct Request {
        Type type = Type::Copy;
        QStringList sources;
        QString destination;
    };

    struct OperationResult {
        Request request;
        QString error;
        QString errorPath;
        QStringList failedPaths;
        int failedCount = 0;
        int succeededCount = 0;
        bool aborted = false;
    };

    enum class ConflictResolution {
        Pending,
        Replace,
        Skip,
        KeepBoth,
        Cancel
    };
    Q_ENUM(ConflictResolution)

    explicit OperationQueue(QObject *parent = nullptr);
    ~OperationQueue() override;

    bool busy() const;
    double progress() const;
    QString currentLabel() const;
    QString error() const;
    QVariantMap lastError() const;
    QString statusMessage() const;
    QString speedText() const;
    QString remainingTimeText() const;

    Q_INVOKABLE void copyTo(const QStringList &sources, const QString &destination);
    Q_INVOKABLE void duplicateInPlace(const QStringList &sources, const QString &destinationHint = {});
    Q_INVOKABLE void moveTo(const QStringList &sources, const QString &destination);
    Q_INVOKABLE void extractTo(const QStringList &sources, const QString &destination);
    Q_INVOKABLE void compressToArchive(const QStringList &sources, const QString &archivePath);
    Q_INVOKABLE void compressToSevenZip(const QStringList &sources, const QString &archivePath);
    Q_INVOKABLE void deletePaths(const QStringList &paths);

    Q_INVOKABLE void resolveConflict(ConflictResolution resolution, bool applyToAll);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void retryLastOperation();

    void setStatusMessage(const QString &msg);
    void reportError(const QString &message,
                     const QString &path,
                     const QString &operation,
                     bool retryable = false);
    
    // Public helpers
    void setProgress(double progress);
    void updateMetrics(qint64 currentBytes, qint64 totalBytes);
    bool isAborted() const { return m_abort; }
    static bool isCurrentThreadAborted();
    static void setCurrentThreadAbortChecker(std::function<bool()> checker);
    static void reportCurrentThreadProgressBytes(qint64 bytes);
    static void setCurrentThreadProgressReporter(std::function<void(qint64)> reporter);

signals:
    void busyChanged();
    void progressChanged();
    void currentLabelChanged();
    void errorChanged();
    void lastErrorChanged();
    void statusMessageChanged();
    void speedChanged();
    void operationFinished(OperationQueue::Type type, const QStringList &sources, const QString &destination);
    void conflictDetected(const QString &source, const QString &destination,
                          qint64 sourceSize, const QDateTime &sourceModified,
                          qint64 destSize, const QDateTime &destModified);

private:

    //TODO move to another place when it will be available
    DriveStorageType getDriveTypeByPath(const QString &filePath);

    void enqueue(Request request);
    void runNext();
    void finishCurrent();
    void setBusy(bool busy);
    int completedItems() const;
    int totalItems() const;

    void setCurrentLabel(const QString &label);
    void setError(const QString &error);
    void setLastError(const QVariantMap &error);
    void setCompletedItems(int completed);
    void setTotalItems(int total);

    OperationResult execute(const Request &request);
    qint64 totalBytesFor(const QStringList &sources) const;
    qint64 totalBytesForExtraction(const QStringList &sources) const;
    qint64 totalBytesForPath(const QString &path) const;
    qint64 totalEntryCountForPath(const QString &path) const;
    void copyPath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes);
    void movePath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes);
    void extractArchiveContents(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes);
    void compressPathsToSevenZip(const QStringList &sources, const QString &archivePath, qint64 totalBytes);
    QString uniqueDestinationPath(const QString &path) const;
    QString duplicateDestinationPath(const QString &path) const;
    bool pathExists(const QString &path) const;
    bool isRealDirectory(const QString &path) const;
    bool removePathIfExists(const QString &path) const;
    bool removeSourcePath(const QString &path) const;
    bool ensureParentDirectory(const QString &path) const;
    bool makePath(const QString &path) const;
    QStringList childPaths(const QString &path) const;
    QString destinationNameForCopy(FileProvider *sourceProvider, const QString &sourcePath) const;

    ConflictResolution waitForResolution(const QString &source, const QString &destination);

    QList<Request> m_pending;
    Request m_lastRequest;
    bool m_hasLastRequest = false;
    QFutureWatcher<OperationResult> m_watcher;
    std::atomic<bool> m_abort = false;
    bool m_busy = false;
    double m_progress = 0.0;
    int m_completedItems = 0;
    int m_totalItems = 0;
    QString m_currentLabel;
    QString m_error;
    QVariantMap m_lastError;
    QString m_statusMessage;
    QString m_speedText;
    QString m_remainingTimeText;
    QElapsedTimer m_operationTimer;
    qint64 m_lastBytes = 0;
    qint64 m_lastTime = 0;
    double m_currentSpeed = 0.0;
    mutable QHash<QString, std::shared_ptr<FileProvider>> m_providerCache;
    mutable QMutex m_providerMutex;
    FileProvider* getProviderForPath(const QString &path) const;

    QMutex m_mutex;
    QWaitCondition m_condition;
    ConflictResolution m_resolution = ConflictResolution::Pending;
    bool m_applyToAll = false;
    ConflictResolution m_lastResolution = ConflictResolution::Pending;
};
