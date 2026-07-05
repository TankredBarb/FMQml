#include "TelegramFileProviderPlugin.h"

#include "TelegramAuth.h"
#include "TelegramCache.h"
#include "TelegramClient.h"
#include "TelegramFileProvider.h"
#include "TelegramPath.h"
#include "CleanupSubsystem.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

using namespace TelegramProviderInternal;

namespace {

void scheduleLegacyThumbnailCleanup()
{
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        return;
    }

    const QString legacyRoot = QDir(cacheRoot).filePath(QStringLiteral("telegram-thumbnails"));
    if (!QFileInfo::exists(legacyRoot)) {
        return;
    }

    QString leaseId;
    CleanupSubsystem::instance().registerArtifact(
        CleanupArtifactKind::ThumbnailAdapter,
        legacyRoot,
        cacheRoot,
        true,
        &leaseId);
    if (!leaseId.isEmpty()) {
        CleanupSubsystem::instance().scheduleDelete(leaseId);
    }
}

QString thumbnailPathForEntry(TelegramEntry entry)
{
    if (!entry.thumbnailLocalPath.isEmpty() && QFileInfo::exists(entry.thumbnailLocalPath)) {
        return entry.thumbnailLocalPath;
    }
    if (entry.thumbnailFileId > 0) {
        QString error;
        const QString localPath = sharedTelegramClient().downloadFile(
            entry.thumbnailFileId,
            std::function<bool(qint64, qint64)>(),
            &error,
            15000);
        if (!localPath.isEmpty() && QFileInfo::exists(localPath)) {
            entry.thumbnailLocalPath = localPath;
            entry.hasThumbnail = true;
            storeEntry(entry);
            return localPath;
        }
    }
    if (entry.thumbnailData.isEmpty()) {
        return {};
    }

    static const QString thumbnailRoot = []() {
        scheduleLegacyThumbnailCleanup();

        const QString cleanupRoot = StagingLocationPolicy::defaultCleanupRoot();
        if (cleanupRoot.isEmpty()) {
            return QString{};
        }

        const QString parent = QDir(cleanupRoot).filePath(QStringLiteral("telegram-thumbnails"));
        QString leaseId;
        return CleanupSubsystem::instance().allocateStagingDirectory(
            CleanupArtifactKind::ThumbnailAdapter,
            parent,
            QStringLiteral("session-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)),
            &leaseId);
    }();
    if (thumbnailRoot.isEmpty()) {
        return {};
    }

    const QByteArray key = QCryptographicHash::hash(entry.path.toUtf8() + entry.thumbnailData, QCryptographicHash::Sha256).toHex();
    const QString path = QDir(thumbnailRoot).filePath(QString::fromLatin1(key) + QStringLiteral(".jpg"));
    if (QFileInfo::exists(path)) {
        return path;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(entry.thumbnailData) != entry.thumbnailData.size()) {
        QFile::remove(path);
        return {};
    }
    return path;
}

} // namespace

int TelegramFileProviderPlugin::apiVersion() const
{
    return FM_FILE_PROVIDER_PLUGIN_API_VERSION;
}

QString TelegramFileProviderPlugin::pluginId() const
{
    return QStringLiteral("fm.telegram-provider");
}

QString TelegramFileProviderPlugin::displayName() const
{
    return QStringLiteral("Telegram Shared Files Provider");
}

QStringList TelegramFileProviderPlugin::schemes() const
{
    return {QStringLiteral("telegram")};
}

bool TelegramFileProviderPlugin::canHandle(const QString &path) const
{
    return parseTelegramPath(path).valid;
}

std::unique_ptr<FileProvider> TelegramFileProviderPlugin::createProvider()
{
    return createTelegramFileProvider();
}

QString TelegramFileProviderPlugin::preprocessPath(const QString &path) const
{
    const QString converted = telegramPathFromUserInput(path);
    return converted.isEmpty() ? path : converted;
}

QString TelegramFileProviderPlugin::thumbnailUrlForPath(const QString &path) const
{
    const std::optional<TelegramEntry> entry = cachedEntry(normalizedTelegramPath(path));
    if (!entry || (entry->thumbnailLocalPath.isEmpty() && entry->thumbnailFileId <= 0 && entry->thumbnailData.isEmpty())) {
        return {};
    }

    const QString thumbnailPath = thumbnailPathForEntry(*entry);
    return thumbnailPath.isEmpty() ? QString{} : QUrl::fromLocalFile(thumbnailPath).toString();
}

int TelegramFileProviderPlugin::actionApiVersion() const
{
    return FM_FILE_ACTION_PLUGIN_API_VERSION;
}

QString TelegramFileProviderPlugin::actionPluginId() const
{
    return pluginId();
}

QString TelegramFileProviderPlugin::actionDisplayName() const
{
    return displayName();
}

QList<FileActionDescriptor> TelegramFileProviderPlugin::actionsForContext(const FileActionContext &context) const
{
    if (!context.currentPath.startsWith(QStringLiteral("telegram://"), Qt::CaseInsensitive)
        && !context.targetPath.startsWith(QStringLiteral("telegram://"), Qt::CaseInsensitive)) {
        return {};
    }

    FileActionDescriptor status;
    status.id = QStringLiteral("authStatus");
    status.text = QStringLiteral("Telegram status");
    status.iconSource = QStringLiteral("../assets/icons/info.svg");
    status.order = 900;
    QList<FileActionDescriptor> actions{status};

    FileActionDescriptor signIn;
    signIn.id = QStringLiteral("setPhoneNumber");
    signIn.text = QStringLiteral("Telegram sign in");
    signIn.iconSource = QStringLiteral("../assets/icons/plugin.svg");
    signIn.order = 905;
    actions.append(signIn);

    FileActionDescriptor signOut;
    signOut.id = QStringLiteral("signOut");
    signOut.text = QStringLiteral("Sign out from Telegram");
    signOut.iconSource = QStringLiteral("../assets/icons/exit.svg");
    signOut.order = 910;
    actions.append(signOut);

    FileActionDescriptor forgetLocalData;
    forgetLocalData.id = QStringLiteral("forgetLocalData");
    forgetLocalData.text = QStringLiteral("Forget Telegram local data");
    forgetLocalData.iconSource = QStringLiteral("../assets/icons/delete.svg");
    forgetLocalData.order = 915;
    actions.append(forgetLocalData);

    return actions;
}

QVariantMap TelegramFileProviderPlugin::triggerAction(const QString &actionId, const FileActionContext &context)
{
    if (actionId == QLatin1String("authStatus")) {
        const TelegramAuthState auth = currentAuthState();
        QString accountLabel = auth.accountLabel;
        if (accountLabel.isEmpty() && auth.signedIn && sharedTelegramClient().state() == TelegramClient::State::Ready) {
            QString labelError;
            accountLabel = sharedTelegramClient().currentUserAccountLabel(&labelError);
            Q_UNUSED(labelError)
            const TelegramApiCredentials credentials = savedApiCredentials();
            if (!accountLabel.isEmpty()
                && credentials.apiId > 0
                && !credentials.apiHash.trimmed().isEmpty()) {
                rememberApiCredentials(credentials.apiId, credentials.apiHash, accountLabel);
            }
        }
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Telegram")},
            {QStringLiteral("subtitle"), QStringLiteral("Shared files provider")},
            {QStringLiteral("message"), auth.statusLabel},
            {QStringLiteral("signedIn"), auth.signedIn},
            {QStringLiteral("hasApiCredentials"), auth.hasApiCredentials},
            {QStringLiteral("accountLabel"), accountLabel.isEmpty() ? auth.statusLabel : accountLabel},
        };
    }

    TelegramClient &client = sharedTelegramClient();
    QString error;
    bool ok = false;
    QString message;
    if (actionId == QLatin1String("setPhoneNumber")) {
        ok = client.setPhoneNumber(context.parameters.value(QStringLiteral("phoneNumber")).toString(),
                                   context.parameters.value(QStringLiteral("apiId")).toInt(),
                                   context.parameters.value(QStringLiteral("apiHash")).toString(),
                                   &error);
        message = ok ? client.sanitizedStatus() : error;
    } else if (actionId == QLatin1String("checkCode")) {
        ok = client.checkCode(context.parameters.value(QStringLiteral("code")).toString(), &error);
        message = ok ? client.sanitizedStatus() : error;
    } else if (actionId == QLatin1String("checkPassword")) {
        ok = client.checkPassword(context.parameters.value(QStringLiteral("password")).toString(), &error);
        message = ok ? client.sanitizedStatus() : error;
    } else if (actionId == QLatin1String("signOut")) {
        ok = client.logOut(&error);
        message = ok ? client.sanitizedStatus() : error;
    } else if (actionId == QLatin1String("forgetLocalData")) {
        ok = forgetLocalTelegramData(&error);
        message = ok ? QStringLiteral("Telegram local data was removed.") : error;
    } else if (actionId == QLatin1String("openChat")) {
        const QString target = context.parameters.value(QStringLiteral("target")).toString();
        const QString path = telegramPathFromUserInput(target);
        ok = !path.isEmpty() && parseTelegramPath(path).kind != TelegramPathKind::Root;
        message = ok ? QStringLiteral("Telegram source resolved.") : QStringLiteral("Enter a Telegram chat id, @username, or t.me link.");
        return {
            {QStringLiteral("ok"), ok},
            {QStringLiteral("title"), QStringLiteral("Telegram")},
            {QStringLiteral("message"), message},
            {QStringLiteral("openPath"), path},
        };
    }

    if (actionId == QLatin1String("setPhoneNumber")
        || actionId == QLatin1String("checkCode")
        || actionId == QLatin1String("checkPassword")
        || actionId == QLatin1String("signOut")
        || actionId == QLatin1String("forgetLocalData")) {
        const bool hasCredentials = hasSavedApiCredentials();
        return {
            {QStringLiteral("ok"), ok},
            {QStringLiteral("title"), QStringLiteral("Telegram")},
            {QStringLiteral("message"), message},
            {QStringLiteral("signedIn"), hasCredentials},
            {QStringLiteral("hasApiCredentials"), hasCredentials},
            {QStringLiteral("refreshCurrentPath"), ok},
            {QStringLiteral("refreshPlaces"), ok},
        };
    }

    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("title"), QStringLiteral("Telegram")},
        {QStringLiteral("message"), QStringLiteral("Unknown Telegram action.")},
    };
}
