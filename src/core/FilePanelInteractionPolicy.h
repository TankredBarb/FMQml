#pragma once

#include <QString>
#include <QStringList>

namespace FilePanelInteractionPolicy {

struct AttentionGuard {
    QString expectedDirectory;
    QString currentDirectory;
    quint64 expectedNavigationGeneration = 0;
    quint64 currentNavigationGeneration = 0;
    quint64 expectedInteractionRevision = 0;
    quint64 currentInteractionRevision = 0;
    bool modelConverged = false;
};

QString nearestSurvivor(const QStringList &orderedPaths,
                        const QStringList &removedPaths,
                        int anchorRow);

QString currentAfterRemoval(const QStringList &orderedPaths,
                            const QStringList &removedPaths,
                            const QString &currentPath,
                            int anchorRow);

bool canApplyAttention(const AttentionGuard &guard);

} // namespace FilePanelInteractionPolicy
