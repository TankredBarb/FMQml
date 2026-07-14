#include "FileActionPolicyEvaluator.h"

#include "PathSemantics.h"

namespace {
template<typename Predicate>
bool allNonEmptyPathsMatch(const QStringList &paths, Predicate predicate)
{
    if (paths.isEmpty()) return false;
    for (const QString &path : paths) {
        if (!predicate(path)) return false;
    }
    return true;
}
}

bool FileActionPolicyEvaluator::canUseLocalShellAction(const QString &path)
{
    const PathKind kind = PathSemantics::describe(path).kind;
    return kind == PathKind::Local || kind == PathKind::FileUrl;
}

bool FileActionPolicyEvaluator::canShowProperties(const QString &path)
{
    const PathKind kind = PathSemantics::describe(path).kind;
    return kind == PathKind::Local || kind == PathKind::FileUrl || kind == PathKind::Provider;
}

bool FileActionPolicyEvaluator::canShowProperties(const QStringList &paths)
{
    return allNonEmptyPathsMatch(paths, [](const QString &path) { return canShowProperties(path); });
}

bool FileActionPolicyEvaluator::canAddToFavorites(const QString &path)
{
    const PathKind kind = PathSemantics::describe(path).kind;
    return kind == PathKind::Local || kind == PathKind::FileUrl;
}

bool FileActionPolicyEvaluator::canAddToFavorites(const QStringList &paths)
{
    return allNonEmptyPathsMatch(paths, [](const QString &path) { return canAddToFavorites(path); });
}
