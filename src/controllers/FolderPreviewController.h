#pragma once

#include <QObject>
#include <QThreadPool>
#include <QVariantMap>

#include <atomic>

class FolderPreviewController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY snapshotChanged)
    Q_PROPERTY(quint64 requestCount READ requestCount NOTIFY statisticsChanged)
    Q_PROPERTY(quint64 cancellationCount READ cancellationCount NOTIFY statisticsChanged)
    Q_PROPERTY(quint64 failureCount READ failureCount NOTIFY statisticsChanged)
    Q_PROPERTY(quint64 cacheHitCount READ cacheHitCount NOTIFY statisticsChanged)

public:
    explicit FolderPreviewController(QObject *parent = nullptr);
    ~FolderPreviewController() override;

    QVariantMap snapshot() const;
    bool loading() const;
    quint64 requestCount() const { return m_requestCount; }
    quint64 cancellationCount() const { return m_cancellationCount; }
    quint64 failureCount() const { return m_failureCount; }
    quint64 cacheHitCount() const { return 0; }

    Q_INVOKABLE quint64 request(const QString &path, bool showHidden, int maxEntries = 9);
    Q_INVOKABLE quint64 requestWithSort(const QString &path, bool showHidden, int maxEntries,
                                        int sortRole, int sortOrder, bool mixFilesAndFolders);
    Q_INVOKABLE void cancel();

signals:
    void snapshotChanged();
    void statisticsChanged();

private:
    void publish(quint64 requestId, const QVariantMap &snapshot);

    QThreadPool m_pool;
    std::atomic<quint64> m_generation{0};
    QVariantMap m_snapshot;
    quint64 m_requestCount = 0;
    quint64 m_cancellationCount = 0;
    quint64 m_failureCount = 0;
};
