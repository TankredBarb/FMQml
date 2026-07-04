#include "InstagramFileProviderPlugin.h"

#include "InstagramAuth.h"
#include "InstagramInternal.h"

#include <QFile>
#include <QMutexLocker>
#include <QUrl>

using namespace InstagramProviderInternal;

int InstagramFileProviderPlugin::apiVersion() const
{
    return FM_FILE_PROVIDER_PLUGIN_API_VERSION;
}

QString InstagramFileProviderPlugin::pluginId() const
{
    return QStringLiteral("fm.instagram-provider");
}

QString InstagramFileProviderPlugin::displayName() const
{
    return QStringLiteral("Instagram Public Media Provider");
}

QStringList InstagramFileProviderPlugin::schemes() const
{
    return {QStringLiteral("instagram")};
}

bool InstagramFileProviderPlugin::canHandle(const QString &path) const
{
    return parseInstagramPath(path).valid;
}

std::unique_ptr<FileProvider> InstagramFileProviderPlugin::createProvider()
{
    return InstagramProviderInternal::createInstagramFileProvider();
}

QString InstagramFileProviderPlugin::preprocessPath(const QString &path) const
{
    const QString converted = instagramUrlToPath(path);
    return converted.isEmpty() ? path : converted;
}

QString InstagramFileProviderPlugin::thumbnailUrlForPath(const QString &path) const
{
    const std::optional<InstagramMediaItem> item = cachedMediaItemForPath(path);
    return item ? item->thumbnailUrl : QString{};
}

int InstagramFileProviderPlugin::actionApiVersion() const
{
    return FM_FILE_ACTION_PLUGIN_API_VERSION;
}

QString InstagramFileProviderPlugin::actionPluginId() const
{
    return pluginId();
}

QString InstagramFileProviderPlugin::actionDisplayName() const
{
    return displayName();
}

QList<FileActionDescriptor> InstagramFileProviderPlugin::actionsForContext(const FileActionContext &context) const
{
    Q_UNUSED(context)
    QList<FileActionDescriptor> actions;

    FileActionDescriptor status;
    status.id = QStringLiteral("authStatus");
    status.text = QStringLiteral("Instagram session status");
    status.iconSource = QStringLiteral("../assets/icons/info.svg");
    status.order = 900;
    actions.append(status);

    const bool signedIn = InstagramAuth::hasRequiredSessionMarkers(InstagramAuth::sessionCookieHeader());
    FileActionDescriptor authAction;
    authAction.id = signedIn ? QStringLiteral("signOut") : QStringLiteral("importSession");
    authAction.text = signedIn ? QStringLiteral("Sign out from Instagram") : QStringLiteral("Import Instagram session");
    authAction.iconSource = signedIn ? QStringLiteral("../assets/icons/exit.svg") : QStringLiteral("../assets/icons/plugin.svg");
    authAction.order = 905;
    actions.append(authAction);

    return actions;
}

QVariantMap InstagramFileProviderPlugin::triggerAction(const QString &actionId, const FileActionContext &context)
{
    if (actionId == QLatin1String("authStatus")) {
        const QByteArray activeCookie = InstagramAuth::sessionCookieHeader();
        const bool signedIn = InstagramAuth::hasRequiredSessionMarkers(activeCookie);
        const bool envOverride = !qgetenv("FM_INSTAGRAM_COOKIE").trimmed().isEmpty()
            || !qgetenv("FM_INSTAGRAM_COOKIE_FILE").trimmed().isEmpty();
        const QString userId = QString::fromUtf8(InstagramAuth::cookieValue(activeCookie, "ds_user_id"));
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Instagram")},
            {QStringLiteral("subtitle"), QStringLiteral("Session cookie authorization")},
            {QStringLiteral("message"), signedIn
                ? QStringLiteral("Instagram session is active.")
                : QStringLiteral("Instagram session is not configured.")},
            {QStringLiteral("signedIn"), signedIn},
            {QStringLiteral("savedSession"), InstagramAuth::hasSavedSession()},
            {QStringLiteral("envOverride"), envOverride},
            {QStringLiteral("accountLabel"), signedIn
                ? (userId.isEmpty() ? QStringLiteral("Session active") : QStringLiteral("Instagram user %1").arg(userId))
                : QStringLiteral("Not signed in")},
        };
    }

    if (actionId == QLatin1String("importSession")) {
        QString cookieHeader = context.parameters.value(QStringLiteral("cookieHeader")).toString();
        if (cookieHeader.trimmed().isEmpty()) {
            QString fileName = context.parameters.value(QStringLiteral("fileUrl")).toString().trimmed();
            if (fileName.isEmpty()) {
                fileName = context.parameters.value(QStringLiteral("filePath")).toString().trimmed();
            }
            const QUrl fileUrl(fileName);
            const QString localFile = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileName;
            QFile file(localFile);
            if (localFile.isEmpty() || !file.open(QIODevice::ReadOnly)) {
                return {
                    {QStringLiteral("ok"), false},
                    {QStringLiteral("title"), QStringLiteral("Instagram")},
                    {QStringLiteral("message"), QStringLiteral("Could not read Instagram session cookie file.")},
                };
            }
            cookieHeader = QString::fromUtf8(file.readAll());
        }

        if (!InstagramAuth::rememberSessionCookieHeader(cookieHeader)) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Instagram")},
                {QStringLiteral("message"), QStringLiteral("Could not save Instagram session cookie. The file must contain sessionid, ds_user_id, and csrftoken cookies.")},
            };
        }

        QMutexLocker locker(&cacheMutex());
        postCache().clear();
        storyCache().clear();
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Instagram")},
            {QStringLiteral("message"), QStringLiteral("Instagram session was imported.")},
            {QStringLiteral("refreshCurrentPath"), true},
        };
    }

    if (actionId == QLatin1String("signOut")) {
        const bool ok = InstagramAuth::clearSavedSession();
        {
            QMutexLocker locker(&cacheMutex());
            postCache().clear();
            storyCache().clear();
        }
        const bool envOverride = !qgetenv("FM_INSTAGRAM_COOKIE").trimmed().isEmpty()
            || !qgetenv("FM_INSTAGRAM_COOKIE_FILE").trimmed().isEmpty();
        return {
            {QStringLiteral("ok"), ok},
            {QStringLiteral("title"), QStringLiteral("Instagram")},
            {QStringLiteral("message"), envOverride
                ? QStringLiteral("Saved Instagram session was removed. Environment cookie override is still active.")
                : (ok ? QStringLiteral("Instagram session was removed.") : QStringLiteral("Could not remove Instagram session."))},
            {QStringLiteral("refreshCurrentPath"), ok},
        };
    }

    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("title"), QStringLiteral("Instagram")},
        {QStringLiteral("message"), QStringLiteral("Unknown Instagram action.")},
    };
}
