#pragma once

#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>

namespace FileSpaceUsage {

struct Result {
    // Regular-file footprint, with hardlinks/shared Btrfs extents counted once.
    // -1 means unmeasured (unavailable or pending). With skipped paths, bytes is
    // a lower bound, never an unmarked partial sum or a logical-size fallback.
    qint64 bytes = -1;
    QString explanation;
    int skippedPaths = 0;
    QString firstSkippedPath = {};
    int pendingPaths = 0;
    bool partial() const { return bytes >= 0 && skippedPaths > 0; }
};

struct Progress {
    qint64 bytes = 0;
    qint64 files = 0;
    int skippedPaths = 0;
    QString currentPath;
    qint64 helperRequests = 0;
};
using ProgressCallback = std::function<void(const Progress &)>;

// The caller remains unprivileged. Btrfs metadata is obtained from the installed
// read-only descriptor service, without pkexec/polkit authentication. Excludes
// filesystem metadata and symlink targets. This is not a deletion forecast.
Result measure(const QStringList &paths, const std::atomic_bool &cancelled,
               const ProgressCallback &progress = {});

} // namespace FileSpaceUsage
