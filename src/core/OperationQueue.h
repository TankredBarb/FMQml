#pragma once

#include <QFutureWatcher>
#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QMutex>
#include <QWaitCondition>
#include <QtQml>
#include <QElapsedTimer>
#include <memory>
#include <atomic>

#include "LocalFileProvider.h"

class OperationQueue : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Enums and signals only")
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentLabel READ currentLabel NOTIFY currentLabelChanged)
    Q_PROPERTY(QString error READ error WRITE setError NOTIFY errorChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int completedItems READ completedItems NOTIFY progressChanged)
    Q_PROPERTY(int totalItems READ totalItems NOTIFY progressChanged)
    Q_PROPERTY(QString speedText READ speedText NOTIFY speedChanged)
    Q_PROPERTY(QString remainingTimeText READ remainingTimeText NOTIFY speedChanged)

public:
    enum class Type {
        Copy,
        Move,
        Delete
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
    QString statusMessage() const;
    QString speedText() const;
    QString remainingTimeText() const;

    Q_INVOKABLE void copyTo(const QStringList &sources, const QString &destination);
    Q_INVOKABLE void moveTo(const QStringList &sources, const QString &destination);
    Q_INVOKABLE void deletePaths(const QStringList &paths);

    Q_INVOKABLE void resolveConflict(ConflictResolution resolution, bool applyToAll);
    Q_INVOKABLE void cancel();

    void setStatusMessage(const QString &msg);
    
    // Public helpers
    void setProgress(double progress);
    void updateMetrics(qint64 currentBytes, qint64 totalBytes);
    bool isAborted() const { return m_abort; }

signals:
    void busyChanged();
    void progressChanged();
    void currentLabelChanged();
    void errorChanged();
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
    void setCompletedItems(int completed);
    void setTotalItems(int total);

    void execute(const Request &request);
    qint64 totalBytesFor(const QStringList &sources) const;
    qint64 totalBytesForPath(const QString &path) const;
    void copyPath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes);
    void movePath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes);
    QString uniqueDestinationPath(FileProvider &destProvider, const QString &path) const;
    bool pathExists(const QString &path) const;
    bool isRealDirectory(const QString &path) const;
    bool removePathIfExists(const QString &path) const;
    bool removeSourcePath(const QString &path) const;
    bool ensureParentDirectory(const QString &path) const;
    bool makePath(const QString &path) const;
    QStringList childPaths(const QString &path) const;

    ConflictResolution waitForResolution(const QString &source, const QString &destination);

    QList<Request> m_pending;
    QFutureWatcher<Request> m_watcher;
    std::atomic<bool> m_abort = false;
    bool m_busy = false;
    double m_progress = 0.0;
    int m_completedItems = 0;
    int m_totalItems = 0;
    QString m_currentLabel;
    QString m_error;
    QString m_statusMessage;
    QString m_speedText;
    QString m_remainingTimeText;
    QElapsedTimer m_operationTimer;
    qint64 m_lastBytes = 0;
    qint64 m_lastTime = 0;
    double m_currentSpeed = 0.0;

    QMutex m_mutex;
    QWaitCondition m_condition;
    ConflictResolution m_resolution = ConflictResolution::Pending;
    bool m_applyToAll = false;
    ConflictResolution m_lastResolution = ConflictResolution::Pending;
};
