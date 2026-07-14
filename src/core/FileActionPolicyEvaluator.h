#pragma once

#include <QString>
#include <QStringList>

class FileActionPolicyEvaluator final {
public:
    static bool canUseLocalShellAction(const QString &path);
    static bool canShowProperties(const QString &path);
    static bool canShowProperties(const QStringList &paths);
    static bool canAddToFavorites(const QString &path);
    static bool canAddToFavorites(const QStringList &paths);
};
