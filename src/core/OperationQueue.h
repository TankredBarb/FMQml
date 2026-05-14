#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QStringList>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>

class OperationQueue final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentLabel READ currentLabel NOTIFY currentLabelChanged)
    Q_PROPERTY(QString error READ error WRITE setError NOTIFY errorChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    enum class Type {
        Copy,
        Move,
        Delete
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
        KeepBoth
    };
    Q_ENUM(ConflictResolution)

    explicit OperationQueue(QObject *parent = nullptr);
    ~OperationQueue() override;

    bool busy() const;
    double progress() const;
    QString currentLabel() const;
    QString error() const;
    QString statusMessage() const;

    Q_INVOKABLE void copyTo(const QStringList &sources, const QString &destination);
    Q_INVOKABLE void moveTo(const QStringList &sources, const QString &destination);
    Q_INVOKABLE void deletePaths(const QStringList &paths);

    Q_INVOKABLE void resolveConflict(ConflictResolution resolution, bool applyToAll);
    Q_INVOKABLE void cancel();

signals:
    void busyChanged();
    void progressChanged();
    void currentLabelChanged();
    void errorChanged();
    void statusMessageChanged();
    void operationFinished(OperationQueue::Type type, const QStringList &sources, const QString &destination);
    void conflictDetected(const QString &source, const QString &destination);

private:
    void enqueue(Request request);
    void runNext();
    void finishCurrent();
    void setBusy(bool busy);
    void setProgress(double progress);
    void setCurrentLabel(const QString &label);
    void setError(const QString &error);
    void setStatusMessage(const QString &msg);

    void execute(const Request &request);
    qint64 totalBytesFor(const QStringList &sources) const;
    qint64 totalBytesForPath(const QString &path) const;
    void copyPath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes);
    void movePath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes);
    QString uniqueDestinationPath(const QString &path) const;

    ConflictResolution waitForResolution(const QString &source, const QString &destination);

    QList<Request> m_pending;
    QFutureWatcher<Request> m_watcher;
    std::atomic<bool> m_abort = false;
    bool m_busy = false;
    double m_progress = 0.0;
    QString m_currentLabel;
    QString m_error;
    QString m_statusMessage;

    QMutex m_mutex;
    QWaitCondition m_condition;
    ConflictResolution m_resolution = ConflictResolution::Pending;
    bool m_applyToAll = false;
    ConflictResolution m_lastResolution = ConflictResolution::Pending;
};
