#include "FileMetadataCoordinator.h"

#ifndef FM_FILE_METADATA_COORDINATOR_TEST
#include "../core/MetadataExtractor.h"
#endif

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>

#include <utility>

namespace {
constexpr int kMetadataCacheEntries = 512;
constexpr int kMetadataWorkerCount = 2;

bool metadataTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_METADATA_TRACE");
    return enabled;
}

#ifndef FM_FILE_METADATA_COORDINATOR_TEST
QVariantMap extractMetadata(const QString &path)
{
    const QVariantList properties = MetadataExtractor::extract(path);
    QVariantMap metadata;
    for (const QVariant &property : properties) {
        const QVariantMap pair = property.toMap();
        const QString label = pair.value(QStringLiteral("label")).toString();
        const QString value = pair.value(QStringLiteral("value")).toString();
        if (label == QLatin1String("Dimensions")) {
            metadata.insert(QStringLiteral("dimensions"), value);
            metadata.insert(QStringLiteral("resolution"), value);
        } else if (label == QLatin1String("Duration")) {
            metadata.insert(QStringLiteral("duration"), value);
        } else if (label == QLatin1String("Artist")) {
            metadata.insert(QStringLiteral("artist"), value);
        } else if (label == QLatin1String("Album")) {
            metadata.insert(QStringLiteral("album"), value);
        } else if (label == QLatin1String("Bitrate")) {
            metadata.insert(QStringLiteral("bitrate"), value);
        }
    }
    return metadata;
}
#endif
}

FileMetadataCoordinator::FileMetadataCoordinator(QObject *parent)
#ifdef FM_FILE_METADATA_COORDINATOR_TEST
    : FileMetadataCoordinator(Extractor{}, parent)
#else
    : FileMetadataCoordinator(extractMetadata, parent)
#endif
{
}

FileMetadataCoordinator::FileMetadataCoordinator(Extractor extractor, QObject *parent)
    : QObject(parent)
    , m_extractor(std::move(extractor))
    , m_cache(kMetadataCacheEntries)
{
    m_pool.setMaxThreadCount(kMetadataWorkerCount);
    m_pool.setExpiryTimeout(30000);
}

FileMetadataCoordinator::~FileMetadataCoordinator()
{
    m_pool.clear();
    m_pool.waitForDone();
}

QString FileMetadataCoordinator::cacheKeyForPath(const QString &path) const
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return {};
    }

    return QDir::cleanPath(info.absoluteFilePath())
        + QLatin1Char('|') + QString::number(info.size())
        + QLatin1Char('|') + QString::number(info.lastModified().toMSecsSinceEpoch());
}

void FileMetadataCoordinator::request(const QString &path)
{
    const QString cacheKey = cacheKeyForPath(path);
    if (cacheKey.isEmpty()) {
        QMetaObject::invokeMethod(this, [this, path]() { emit metadataReady(path, {}); }, Qt::QueuedConnection);
        return;
    }

    if (const QVariantMap *cached = m_cache.object(cacheKey)) {
        if (metadataTraceEnabled()) {
            qInfo() << "[FileMetadata] cache-hit" << QFileInfo(path).fileName();
        }
        const QVariantMap metadata = *cached;
        QMetaObject::invokeMethod(this, [this, path, metadata]() { emit metadataReady(path, metadata); },
                                  Qt::QueuedConnection);
        return;
    }

    auto waiting = m_waitingPaths.find(cacheKey);
    if (waiting != m_waitingPaths.end()) {
        waiting->insert(path);
        if (metadataTraceEnabled()) {
            qInfo() << "[FileMetadata] coalesced" << QFileInfo(path).fileName();
        }
        return;
    }

    m_waitingPaths.insert(cacheKey, QSet<QString>{path});
    if (metadataTraceEnabled()) {
        qInfo() << "[FileMetadata] started" << QFileInfo(path).fileName();
    }

    QPointer<FileMetadataCoordinator> self(this);
    const Extractor extractor = m_extractor;
    m_pool.start([self, extractor, path, cacheKey]() {
        QElapsedTimer timer;
        timer.start();
        const QVariantMap metadata = extractor ? extractor(path) : QVariantMap{};
        const qint64 elapsedMs = timer.elapsed();
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, path, cacheKey, metadata, elapsedMs]() {
            if (self) {
                self->finishRequest(path, cacheKey, metadata, elapsedMs);
            }
        }, Qt::QueuedConnection);
    });
}

void FileMetadataCoordinator::finishRequest(const QString &path,
                                            const QString &cacheKey,
                                            const QVariantMap &metadata,
                                            qint64 elapsedMs)
{
    const QSet<QString> waitingPaths = m_waitingPaths.take(cacheKey);
    m_cache.insert(cacheKey, new QVariantMap(metadata));
    if (metadataTraceEnabled()) {
        qInfo() << "[FileMetadata] finished" << QFileInfo(path).fileName()
                << "elapsedMs" << elapsedMs;
    }
    if (waitingPaths.isEmpty()) {
        emit metadataReady(path, metadata);
        return;
    }
    for (const QString &waitingPath : waitingPaths) {
        emit metadataReady(waitingPath, metadata);
    }
}
