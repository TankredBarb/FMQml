#include "FileActionPolicyEvaluator.h"

#include <QDebug>

namespace {
int fail(const QString &message)
{
    qCritical().noquote() << message;
    return 1;
}
}

int main()
{
    if (!FileActionPolicyEvaluator::canUseLocalShellAction(QStringLiteral("/tmp/file"))
        || !FileActionPolicyEvaluator::canUseLocalShellAction(QStringLiteral("file:///tmp/file"))
        || FileActionPolicyEvaluator::canUseLocalShellAction(QStringLiteral("archive:///tmp/a.zip!/file"))
        || FileActionPolicyEvaluator::canUseLocalShellAction(QStringLiteral("gdrive://my-drive/file"))) {
        return fail(QStringLiteral("local shell action policy is incorrect"));
    }
    if (!FileActionPolicyEvaluator::canShowProperties(QStringLiteral("/tmp/file"))
        || !FileActionPolicyEvaluator::canShowProperties(QStringLiteral("gdrive://my-drive/file"))
        || FileActionPolicyEvaluator::canShowProperties(QStringLiteral("archive:///tmp/a.zip!/file"))
        || FileActionPolicyEvaluator::canShowProperties(QStringLiteral("devices://"))
        || FileActionPolicyEvaluator::canShowProperties(QStringLiteral("favorites://"))) {
        return fail(QStringLiteral("properties policy is incorrect"));
    }
    if (!FileActionPolicyEvaluator::canShowProperties({QStringLiteral("gdrive://my-drive/file")})
        || FileActionPolicyEvaluator::canShowProperties(QStringList{})
        || FileActionPolicyEvaluator::canShowProperties({QStringLiteral("/tmp/file"), QStringLiteral("archive:///tmp/a.zip!/file")})) {
        return fail(QStringLiteral("multi-path properties policy is inconsistent"));
    }
    if (!FileActionPolicyEvaluator::canAddToFavorites(QStringLiteral("/tmp/folder"))
        || !FileActionPolicyEvaluator::canAddToFavorites(QStringLiteral("file:///tmp/folder"))
        || FileActionPolicyEvaluator::canAddToFavorites(QStringLiteral("gdrive://my-drive"))
        || FileActionPolicyEvaluator::canAddToFavorites(QStringLiteral("favorites://"))) {
        return fail(QStringLiteral("favorites policy is incorrect"));
    }
    return 0;
}
