#pragma once

#include <QCache>
#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QSet>
#include <QSemaphore>
#include <QStringList>
#include <QThreadPool>
#include <QVariantMap>
#include <functional>

#include "../core/ThumbnailDiskCache.h"

class ThumbnailController final : public QObject {
    Q_OBJECT

public:
    enum class State {
        Ready,
        Pending,
        TemporaryUnavailable,
        Unavailable,
        DecodeFailed,
    };

    struct Lookup {
        State state = State::Unavailable;
        QImage image;
    };

    explicit ThumbnailController(QObject *parent = nullptr);

    Lookup providerThumbnail(const QString &path, const QSize &requestedSize, int priority = 0);

    Q_INVOKABLE void requestThumbnail(const QString &path, int width, int height,
                                      int priority = 0, const QString &reason = {});
    Q_INVOKABLE void cancelThumbnail(const QString &path);
    Q_INVOKABLE void warmThumbnails(const QStringList &paths, int width, int height, int priority = 0);
    Q_INVOKABLE QString stateFor(const QString &path, int width, int height) const;
    Q_INVOKABLE QVariantMap thumbnailMetrics() const;

signals:
    void thumbnailReady(const QString &path, const QString &identity,
                        int width, int height, int revision);
    void thumbnailUnavailable(const QString &path, const QString &identity,
                              bool permanent, const QString &reason);
    void thumbnailStateChanged(const QString &path, const QString &state);

private:
    enum class Lane { Http, Sdk, Device };

    struct EntryState {
        State state = State::Pending;
        QString path;
        QString identity;
        QSize size;
        QDateTime retryAfter;
        quint64 generation = 0;
        int revision = 0;
        int attempts = 0;
        int priority = 0;
    };

    struct QueuedJob {
        QString path;
        int priority = 0;
        quint64 sequence = 0;
        qint64 enqueuedAtMs = 0;
        std::function<void()> work;
    };

    struct LaneQueue {
        QVector<QueuedJob> pending;
        int running = 0;
    };

    static QSize bucketSize(const QSize &size);
    static QString keyFor(const QString &path, const QString &identity, const QSize &size);
    static QString stateName(State state);
    void startProviderJob(const QString &key, const QString &path,
                          const QString &identity, const QSize &size, quint64 generation, int priority);
    void finishProviderJob(const QString &key, const QString &path,
                           const QString &identity, const QSize &size, quint64 generation,
                           const QImage &image, State state, const QString &reason);
    Lane laneForPath(const QString &path) const;
    QThreadPool *poolForLane(Lane lane);
    LaneQueue &queueForLane(Lane lane);
    bool enqueueJob(Lane lane, const QString &path, int priority, std::function<void()> work);
    void startNextJob(Lane lane);
    void finishQueuedJob(Lane lane);

    QCache<QString, QImage> m_cache;
    ThumbnailDiskCache m_diskCache;
    QHash<QString, EntryState> m_states;
    QSet<QString> m_cancelledPaths;
    QHash<QString, quint64> m_pathGenerations;
    quint64 m_nextJobSequence = 0;
    LaneQueue m_httpQueue;
    LaneQueue m_sdkQueue;
    LaneQueue m_deviceQueue;
    mutable QMutex m_mutex;
    QThreadPool m_httpPool;
    QThreadPool m_sdkPool;
    QThreadPool m_devicePool;
    struct Metrics {
        quint64 memoryHits = 0;
        quint64 diskHits = 0;
        quint64 coalesced = 0;
        quint64 queueDrops = 0;
        quint64 completed = 0;
        quint64 temporaryUnavailable = 0;
        quint64 permanentUnavailable = 0;
        quint64 decodeFailed = 0;
        qint64 queueWaitMs = 0;
    };
    Metrics m_metrics;
};
