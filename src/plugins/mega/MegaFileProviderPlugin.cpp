#include "MegaFileProviderPlugin.h"
#include "MegaFileProvider.h"
#include "MegaProviderActions.h"
#include "MegaProviderRuntime.h"
#include "MegaPath.h"
#include "MegaCache.h"
#include "MegaAuth.h"
#include "MegaClientInterface.h"
#include <QDebug>
using namespace MegaProviderRuntime;

MegaFileProviderPlugin::MegaFileProviderPlugin()
{
    // Force initialization of the active MEGA client in the main thread.
    MegaClientInterface &client = megaClient();
    connect(&client, &MegaClientInterface::accountAuthorizationChanged,
            this, [](bool signedIn, const QString &accountEmail, const QString &session) {
                if (signedIn) {
                    if (!MegaAuth::rememberAuthorization(session, accountEmail)) {
                        qWarning() << "[MegaFileProvider] Could not persist MEGA authorization change";
                    }
                } else {
                    MegaAuth::clearSavedAuthorization();
                }
            });

#ifndef FM_MEGA_PROVIDER_TESTING
    const QString session = MegaAuth::savedSession();
    if (!session.isEmpty() && !client.isAccountAuthenticated()) {
        client.resumeAccountSession(session);
    }
#endif
}

#ifdef FM_MEGA_PROVIDER_TESTING
void MegaFileProviderPlugin::setClientForTesting(MegaClientInterface *client)
{
    setMegaClientForTesting(client);
}
#endif

int MegaFileProviderPlugin::apiVersion() const
{
    return FM_FILE_PROVIDER_PLUGIN_API_VERSION;
}

QString MegaFileProviderPlugin::pluginId() const
{
    return QStringLiteral("mega");
}

QString MegaFileProviderPlugin::displayName() const
{
    return QStringLiteral("MEGA");
}

QStringList MegaFileProviderPlugin::schemes() const
{
    return { QStringLiteral("mega") };
}

bool MegaFileProviderPlugin::canHandle(const QString &path) const
{
    if (MegaPath::isSchemePath(path)) {
        return true;
    }
    QString linkId, linkKey; bool isFolder;
    return !MegaPath::fromUserInput(path, linkId, linkKey, isFolder).isEmpty();
}

std::unique_ptr<FileProvider> MegaFileProviderPlugin::createProvider()
{
    return createMegaFileProvider();
}

QString MegaFileProviderPlugin::preprocessPath(const QString &path) const
{
    QString linkId, linkKey;
    bool isFolder = false;
    QString result = MegaPath::fromUserInput(path, linkId, linkKey, isFolder);
    if (!result.isEmpty()) {
        MegaCache::storeKey(linkId, linkKey, isFolder);
        return result;
    }
    return path;
}

int MegaFileProviderPlugin::actionApiVersion() const
{
    return FM_FILE_ACTION_PLUGIN_API_VERSION;
}

QString MegaFileProviderPlugin::actionPluginId() const
{
    return pluginId();
}

QString MegaFileProviderPlugin::actionDisplayName() const
{
    return displayName();
}

QList<FileActionDescriptor> MegaFileProviderPlugin::actionsForContext(const FileActionContext &context) const
{
    return megaActionsForContext(context);
}

QVariantMap MegaFileProviderPlugin::triggerAction(const QString &actionId, const FileActionContext &context)
{
    return triggerMegaAction(actionId, context);
}

int MegaFileProviderPlugin::settingsUiApiVersion() const
{
    return FM_PLUGIN_SETTINGS_UI_API_VERSION;
}

QString MegaFileProviderPlugin::settingsUiPluginId() const
{
    return pluginId();
}

QString MegaFileProviderPlugin::settingsUiTitle() const
{
    return QStringLiteral("MEGA");
}

QString MegaFileProviderPlugin::settingsUiComponentUrl() const
{
    return QStringLiteral("qrc:/mega/settings/MegaSettingsCard.qml");
}

int MegaFileProviderPlugin::settingsUiOrder() const
{
    return 200;
}
