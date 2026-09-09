#include "RemotePreviewCache.h"
#include "../core/CleanupSubsystem.h"

#include <QFileInfo>
#include <QMutexLocker>

namespace PreviewInternal {

RemotePreviewArtifact::~RemotePreviewArtifact()
{
    if (!leaseId.isEmpty()) CleanupSubsystem::instance().scheduleDelete(leaseId);
}

RemotePreviewCache &RemotePreviewCache::instance()
{
    // Construct cleanup first so cached artifacts can release their leases at exit.
    (void)CleanupSubsystem::instance();
    static RemotePreviewCache cache;
    return cache;
}

void RemotePreviewCache::prune()
{
    // Age is measured since insertion, not since access: repeated visits must not
    // keep an externally changed file alive indefinitely with old metadata.
    m_entries.removeIf([](const Entry &entry) {
        return entry.age.elapsed() >= 5 * 60 * 1000;
    });
    qint64 bytes = 0;
    for (const Entry &entry : std::as_const(m_entries)) bytes += entry.artifact->bytes;
    while (!m_entries.isEmpty() && (m_entries.size() > 8 || bytes > 128LL * 1024 * 1024)) {
        bytes -= m_entries.first().artifact->bytes;
        m_entries.removeFirst();
    }
}

std::shared_ptr<RemotePreviewArtifact> RemotePreviewCache::find(const QString &identity)
{
    if (identity.isEmpty()) return {};
    QMutexLocker locker(&m_mutex);
    prune();
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).identity != identity) continue;
        Entry entry = m_entries.takeAt(i);
        const QFileInfo info(entry.artifact->path);
        if (!info.isFile() || info.size() != entry.artifact->bytes) return {};
        const auto artifact = entry.artifact;
        m_entries.append(std::move(entry));
        return artifact;
    }
    return {};
}

void RemotePreviewCache::insert(const QString &identity, const std::shared_ptr<RemotePreviewArtifact> &artifact)
{
    if (identity.isEmpty() || !artifact || artifact->bytes <= 0) return;
    QMutexLocker locker(&m_mutex);
    m_entries.removeIf([&](const Entry &entry) { return entry.identity == identity; });
    Entry entry{identity, artifact, {}};
    entry.age.start();
    m_entries.append(std::move(entry));
    prune();
}

void RemotePreviewCache::clear()
{
    QMutexLocker locker(&m_mutex);
    m_entries.clear();
}

} // namespace PreviewInternal
