#include "ThumbnailController.h"

#include "../core/FileProviderPluginRegistry.h"

#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QMutexLocker>
#include <QTimer>

#include <algorithm>

namespace {
constexpr qsizetype kCacheLimitKb = 64 * 1024;
constexpr int kRetryInitialDelayMs = 500;
constexpr int kRetryMaxAttempts = 3;

bool diskCacheAllowedForPath(const QString &path)
{
    Q_UNUSED(path)
    // Remote thumbnails can contain private content. Keep the cache enabled by
    // default, but let privacy-sensitive installations opt out completely.
    return qEnvironmentVariableIntValue("FM_THUMBNAIL_DISK_CACHE_REMOTE") != 0
        || !qEnvironmentVariableIsSet("FM_THUMBNAIL_DISK_CACHE_REMOTE");
}

QImage decodeBytes(const QByteArray &bytes, const QSize &size)
{
    QBuffer buffer;
    buffer.setData(bytes);
    if (bytes.isEmpty() || !buffer.open(QIODevice::ReadOnly)) {
        return {};
    }
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const QSize sourceSize = reader.size();
    if (sourceSize.isValid()) {
        QSize scaled = sourceSize;
        scaled.scale(size, Qt::KeepAspectRatio);
        reader.setScaledSize(scaled);
    }
    return reader.read();
}

QImage decodeFile(const QString &path, const QSize &size)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize sourceSize = reader.size();
    if (sourceSize.isValid()) {
        QSize scaled = sourceSize;
        scaled.scale(size, Qt::KeepAspectRatio);
        reader.setScaledSize(scaled);
    }
    return reader.read();
}
}

ThumbnailController::ThumbnailController(QObject *parent)
    : QObject(parent)
    , m_cache(kCacheLimitKb)
{
    m_httpPool.setObjectName(QStringLiteral("ThumbnailHttpScheduler"));
    m_httpPool.setMaxThreadCount(3);
    m_sdkPool.setObjectName(QStringLiteral("ThumbnailSdkScheduler"));
    m_sdkPool.setMaxThreadCount(2);
    m_devicePool.setObjectName(QStringLiteral("ThumbnailDeviceScheduler"));
    m_devicePool.setMaxThreadCount(1);
}

ThumbnailController::Lane ThumbnailController::laneForPath(const QString &path) const
{
    if (path.startsWith(QStringLiteral("mega://"), Qt::CaseInsensitive)) {
        return Lane::Sdk;
    }
    if (path.startsWith(QStringLiteral("portable://"), Qt::CaseInsensitive)) {
        return Lane::Device;
    }
    return Lane::Http;
}

QThreadPool *ThumbnailController::poolForLane(Lane lane)
{
    switch (lane) {
    case Lane::Sdk: return &m_sdkPool;
    case Lane::Device: return &m_devicePool;
    case Lane::Http: return &m_httpPool;
    }
    return &m_httpPool;
}

ThumbnailController::LaneQueue &ThumbnailController::queueForLane(Lane lane)
{
    switch (lane) {
    case Lane::Sdk: return m_sdkQueue;
    case Lane::Device: return m_deviceQueue;
    case Lane::Http: return m_httpQueue;
    }
    return m_httpQueue;
}

bool ThumbnailController::enqueueJob(Lane lane, const QString &path, int priority, std::function<void()> work)
{
    constexpr qsizetype kMaxQueuedJobsPerLane = 128;
    {
        QMutexLocker locker(&m_mutex);
        LaneQueue &queue = queueForLane(lane);
        if (queue.pending.size() >= kMaxQueuedJobsPerLane) {
            ++m_metrics.queueDrops;
            return false;
        }
        queue.pending.push_back({path, priority, ++m_nextJobSequence,
                                 QDateTime::currentMSecsSinceEpoch(), std::move(work)});
        std::sort(queue.pending.begin(), queue.pending.end(), [](const QueuedJob &left, const QueuedJob &right) {
            return left.priority == right.priority ? left.sequence < right.sequence : left.priority > right.priority;
        });
    }
    startNextJob(lane);
    return true;
}

void ThumbnailController::startNextJob(Lane lane)
{
    QueuedJob job;
    {
        QMutexLocker locker(&m_mutex);
        LaneQueue &queue = queueForLane(lane);
        if (queue.pending.isEmpty() || queue.running >= poolForLane(lane)->maxThreadCount()) {
            return;
        }
        job = std::move(queue.pending.front());
        queue.pending.removeFirst();
        ++queue.running;
        m_metrics.queueWaitMs += qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - job.enqueuedAtMs);
    }
    poolForLane(lane)->start([this, lane, work = std::move(job.work)]() mutable {
        work();
        QMetaObject::invokeMethod(this, [this, lane]() { finishQueuedJob(lane); }, Qt::QueuedConnection);
    });
}

void ThumbnailController::finishQueuedJob(Lane lane)
{
    {
        QMutexLocker locker(&m_mutex);
        LaneQueue &queue = queueForLane(lane);
        if (queue.running > 0) {
            --queue.running;
        }
    }
    startNextJob(lane);
}

QSize ThumbnailController::bucketSize(const QSize &size)
{
    const auto bucket = [](int value) {
        constexpr int kStep = 64;
        return qBound(kStep, ((qMax(value, kStep) + kStep - 1) / kStep) * kStep, 2048);
    };
    return {bucket(size.width()), bucket(size.height())};
}

QString ThumbnailController::keyFor(const QString &path, const QString &identity, const QSize &size)
{
    return QStringLiteral("%1::%2::%3x%4")
        .arg(identity.isEmpty() ? path : identity)
        .arg(size.width())
        .arg(size.height());
}

QString ThumbnailController::stateName(State state)
{
    switch (state) {
    case State::Ready: return QStringLiteral("ready");
    case State::Pending: return QStringLiteral("pending");
    case State::TemporaryUnavailable: return QStringLiteral("temporary-unavailable");
    case State::Unavailable: return QStringLiteral("unavailable");
    case State::DecodeFailed: return QStringLiteral("decode-failed");
    }
    return QStringLiteral("unknown");
}

ThumbnailController::Lookup ThumbnailController::providerThumbnail(const QString &path,
                                                                    const QSize &requestedSize,
                                                                    int priority)
{
    const QSize size = bucketSize(requestedSize);
    const QString identity = FileProviderPluginRegistry::instance().thumbnailCacheIdentity(path);
    const QString key = keyFor(path, identity, size);
    bool startJob = false;
    quint64 generation = 0;

    {
        QMutexLocker locker(&m_mutex);
        m_cancelledPaths.remove(path);
        if (QImage *cached = m_cache.object(key)) {
            ++m_metrics.memoryHits;
            return {State::Ready, *cached};
        }
        if (diskCacheAllowedForPath(path)) {
            if (const QImage cached = m_diskCache.load(key); !cached.isNull()) {
                const int cost = qMax(1, int((cached.sizeInBytes() + 1023) / 1024));
                m_cache.insert(key, new QImage(cached), cost);
                ++m_metrics.diskHits;
                return {State::Ready, cached};
            }
        }

        generation = m_pathGenerations.value(path);
        const auto stateIt = m_states.constFind(key);
        if (stateIt != m_states.cend()) {
            if (stateIt->generation != generation
                || (stateIt->state == State::TemporaryUnavailable
                && stateIt->retryAfter <= QDateTime::currentDateTimeUtc())) {
                startJob = true;
            } else {
                ++m_metrics.coalesced;
                return {stateIt->state, {}};
            }
        } else {
            startJob = true;
        }

        if (startJob) {
            EntryState &entry = m_states[key];
            entry.state = State::Pending;
            entry.path = path;
            entry.identity = identity;
            entry.size = size;
            entry.generation = generation;
            entry.priority = priority;
        }
    }

    if (startJob) {
        startProviderJob(key, path, identity, size, generation, priority);
    }
    return {State::Pending, {}};
}

void ThumbnailController::requestThumbnail(const QString &path, int width, int height,
                                           int priority, const QString &reason)
{
    Q_UNUSED(priority)
    Q_UNUSED(reason)
    if (!path.trimmed().isEmpty()) {
        providerThumbnail(path, QSize(width, height), priority);
    }
}

void ThumbnailController::cancelThumbnail(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    m_cancelledPaths.insert(path);
    ++m_pathGenerations[path];
    const auto removePath = [&path](LaneQueue &queue) {
        queue.pending.erase(std::remove_if(queue.pending.begin(), queue.pending.end(),
                                           [&path](const QueuedJob &job) { return job.path == path; }),
                            queue.pending.end());
    };
    removePath(m_httpQueue);
    removePath(m_sdkQueue);
    removePath(m_deviceQueue);
}

void ThumbnailController::warmThumbnails(const QStringList &paths, int width, int height, int priority)
{
    for (const QString &path : paths) {
        requestThumbnail(path, width, height, priority, QStringLiteral("warm"));
    }
}

QString ThumbnailController::stateFor(const QString &path, int width, int height) const
{
    const QSize size = bucketSize(QSize(width, height));
    const QString identity = FileProviderPluginRegistry::instance().thumbnailCacheIdentity(path);
    const QString key = keyFor(path, identity, size);
    QMutexLocker locker(&m_mutex);
    if (m_cache.object(key)) {
        return stateName(State::Ready);
    }
    const auto it = m_states.constFind(key);
    return it == m_states.cend() ? QStringLiteral("unknown") : stateName(it->state);
}

QVariantMap ThumbnailController::thumbnailMetrics() const
{
    QMutexLocker locker(&m_mutex);
    return {
        {QStringLiteral("memoryHits"), QVariant::fromValue(m_metrics.memoryHits)},
        {QStringLiteral("diskHits"), QVariant::fromValue(m_metrics.diskHits)},
        {QStringLiteral("coalesced"), QVariant::fromValue(m_metrics.coalesced)},
        {QStringLiteral("queueDrops"), QVariant::fromValue(m_metrics.queueDrops)},
        {QStringLiteral("completed"), QVariant::fromValue(m_metrics.completed)},
        {QStringLiteral("temporaryUnavailable"), QVariant::fromValue(m_metrics.temporaryUnavailable)},
        {QStringLiteral("permanentUnavailable"), QVariant::fromValue(m_metrics.permanentUnavailable)},
        {QStringLiteral("decodeFailed"), QVariant::fromValue(m_metrics.decodeFailed)},
        {QStringLiteral("queueWaitMs"), QVariant::fromValue(m_metrics.queueWaitMs)},
    };
}

void ThumbnailController::startProviderJob(const QString &key, const QString &path,
                                           const QString &identity, const QSize &size, quint64 generation, int priority)
{
    const Lane lane = laneForPath(path);
    if (!enqueueJob(lane, path, priority, [this, key, path, identity, size, generation]() {
        {
            QMutexLocker locker(&m_mutex);
            if (m_cancelledPaths.contains(path)) {
                m_states.remove(key);
                return;
            }
        }
        QString error;
        const ProviderThumbnailResult result = FileProviderPluginRegistry::instance()
            .thumbnailForPath(path, size, &error);
        State state = State::Unavailable;
        QImage image;
        if (result.kind == ProviderThumbnailResult::Kind::TemporaryUnavailable) {
            state = State::TemporaryUnavailable;
        } else if (result.kind == ProviderThumbnailResult::Kind::EncodedBytes) {
            image = decodeBytes(result.encodedBytes, size);
            state = image.isNull() ? State::DecodeFailed : State::Ready;
        } else if (result.kind == ProviderThumbnailResult::Kind::LocalFile) {
            image = decodeFile(result.localFilePath, size);
            state = image.isNull() ? State::DecodeFailed : State::Ready;
        }
        if (state == State::Ready && diskCacheAllowedForPath(path)) {
            m_diskCache.store(key, image);
        }
        QMetaObject::invokeMethod(this, [this, key, path, identity, size, generation, image, state, error]() {
            finishProviderJob(key, path, identity, size, generation, image, state, error);
        }, Qt::QueuedConnection);
    })) {
        finishProviderJob(key, path, identity, size, generation, {}, State::TemporaryUnavailable,
                          QStringLiteral("thumbnail queue is full"));
    }
}

void ThumbnailController::finishProviderJob(const QString &key, const QString &path,
                                            const QString &identity, const QSize &size, quint64 generation,
                                            const QImage &image, State state,
                                            const QString &reason)
{
    bool cancelled = false;
    int revision = 0;
    int retryDelayMs = 0;
    {
        QMutexLocker locker(&m_mutex);
        cancelled = m_cancelledPaths.contains(path) || m_pathGenerations.value(path) != generation;
        if (cancelled) {
            m_states.remove(key);
        } else {
            EntryState &entry = m_states[key];
            entry.state = state;
            ++m_metrics.completed;
            if (state == State::TemporaryUnavailable) {
                ++m_metrics.temporaryUnavailable;
                ++entry.attempts;
                const int delayMs = kRetryInitialDelayMs * (1 << qMin(entry.attempts - 1, 3));
                entry.retryAfter = QDateTime::currentDateTimeUtc().addMSecs(delayMs);
                retryDelayMs = delayMs;
            } else {
                entry.retryAfter = {};
            }
            if (state == State::Unavailable) {
                ++m_metrics.permanentUnavailable;
            } else if (state == State::DecodeFailed) {
                ++m_metrics.decodeFailed;
            }
            revision = ++entry.revision;
            if (state == State::Ready) {
                const int cost = qMax(1, int((image.sizeInBytes() + 1023) / 1024));
                m_cache.insert(key, new QImage(image), cost);
            }
        }
    }
    if (cancelled) {
        return;
    }
    emit thumbnailStateChanged(path, stateName(state));
    if (state == State::Ready) {
        emit thumbnailReady(path, identity, size.width(), size.height(), revision);
    } else {
        emit thumbnailUnavailable(path, identity,
                                  state == State::Unavailable || state == State::DecodeFailed,
                                  reason);
    }
    if (state == State::TemporaryUnavailable) {
        QTimer::singleShot(retryDelayMs, this, [this, key, path, identity, size, generation]() {
            bool retry = false;
            int priority = 0;
            {
                QMutexLocker locker(&m_mutex);
                const auto it = m_states.constFind(key);
                if (it == m_states.cend() || it->generation != generation
                    || m_cancelledPaths.contains(path) || it->state != State::TemporaryUnavailable
                    || it->attempts >= kRetryMaxAttempts || it->retryAfter > QDateTime::currentDateTimeUtc()) {
                    return;
                }
                retry = true;
                priority = it->priority;
                m_states[key].state = State::Pending;
            }
            emit thumbnailStateChanged(path, stateName(State::Pending));
            if (retry) {
                startProviderJob(key, path, identity, size, generation, priority);
            }
        });
    }
}
