#include "FilePanelInteractionPolicy.h"

#include <QSet>

namespace FilePanelInteractionPolicy {

QString nearestSurvivor(const QStringList &orderedPaths,
                        const QStringList &removedPaths,
                        int anchorRow)
{
    if (orderedPaths.isEmpty()) {
        return {};
    }

    const QSet<QString> removed(removedPaths.cbegin(), removedPaths.cend());
    const int pathCount = static_cast<int>(orderedPaths.size());
    const int forwardStart = qBound(0, anchorRow, pathCount);
    for (int row = forwardStart; row < pathCount; ++row) {
        if (!removed.contains(orderedPaths.at(row))) {
            return orderedPaths.at(row);
        }
    }

    for (int row = qMin(anchorRow - 1, pathCount - 1); row >= 0; --row) {
        if (!removed.contains(orderedPaths.at(row))) {
            return orderedPaths.at(row);
        }
    }

    return {};
}

QString currentAfterRemoval(const QStringList &orderedPaths,
                            const QStringList &removedPaths,
                            const QString &currentPath,
                            int anchorRow)
{
    if (!currentPath.isEmpty()
        && orderedPaths.contains(currentPath)
        && !removedPaths.contains(currentPath)) {
        return currentPath;
    }
    return nearestSurvivor(orderedPaths, removedPaths, anchorRow);
}

bool canApplyAttention(const AttentionGuard &guard)
{
    return guard.modelConverged
        && guard.expectedDirectory == guard.currentDirectory
        && guard.expectedNavigationGeneration == guard.currentNavigationGeneration
        && guard.expectedInteractionRevision == guard.currentInteractionRevision;
}

} // namespace FilePanelInteractionPolicy
