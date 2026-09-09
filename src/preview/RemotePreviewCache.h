#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QMutex>
#include <QString>
#include <memory>

namespace PreviewInternal {

// The cache and visible previews share ownership of a cleanup-managed artifact.
// Eviction never deletes a file that is still being displayed.
struct RemotePreviewArtifact {
    QString path;
    QString directory;
    QString leaseId;
    qint64 bytes = 0;
    ~RemotePreviewArtifact();
};

class RemotePreviewCache {
public:
    static RemotePreviewCache &instance();
    std::shared_ptr<RemotePreviewArtifact> find(const QString &identity);
    void insert(const QString &identity, const std::shared_ptr<RemotePreviewArtifact> &artifact);
    void clear();

private:
    struct Entry {
        QString identity;
        std::shared_ptr<RemotePreviewArtifact> artifact;
        QElapsedTimer age;
    };
    void prune();
    QMutex m_mutex;
    QList<Entry> m_entries;
};

} // namespace PreviewInternal
