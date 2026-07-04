#include "InstagramInternal.h"

#include "InstagramAuth.h"

#include <QDebug>

namespace InstagramProviderInternal {

QMutex &cacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, InstagramPost> &postCache()
{
    static QHash<QString, InstagramPost> cache;
    return cache;
}

QHash<QString, InstagramPost> &storyCache()
{
    static QHash<QString, InstagramPost> cache;
    return cache;
}

bool instagramTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_INSTAGRAM_TRACE");
    return enabled;
}

bool instagramGraphqlFallbackEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_INSTAGRAM_GRAPHQL_FALLBACK");
    return enabled;
}

void traceInstagram(const QString &message)
{
    if (!instagramTraceEnabled()) {
        return;
    }

    qInfo().noquote() << "[FM_INSTAGRAM]" << message;
}

} // namespace InstagramProviderInternal
