#pragma once

#include <QCache>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QThreadPool>
#include <QVariantMap>

#include <functional>

class FileMetadataCoordinator final : public QObject
{
    Q_OBJECT

public:
    using Extractor = std::function<QVariantMap(const QString &)>;

    explicit FileMetadataCoordinator(QObject *parent = nullptr);
    explicit FileMetadataCoordinator(Extractor extractor, QObject *parent = nullptr);
    ~FileMetadataCoordinator() override;

    void request(const QString &path);

signals:
    void metadataReady(const QString &path, const QVariantMap &metadata);

private:
    QString cacheKeyForPath(const QString &path) const;
    void finishRequest(const QString &path, const QString &cacheKey,
                       const QVariantMap &metadata, qint64 elapsedMs);

    Extractor m_extractor;
    QThreadPool m_pool;
    QCache<QString, QVariantMap> m_cache;
    QHash<QString, QSet<QString>> m_waitingPaths;
};
