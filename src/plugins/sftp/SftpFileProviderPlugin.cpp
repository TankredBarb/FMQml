#include "SftpFileProviderPlugin.h"
#include "SftpConnectionController.h"
#include "SftpFileProvider.h"

#include <QQmlEngine>
#include <algorithm>

#ifdef FM_SFTP_PROVIDER_TESTING
namespace { QString testKnownHostsFile; }
int runSftpQueueRegression();
void setSftpPlacesFixture(bool enabled);
QVariantMap sftpCreationFixture(bool enabled, int mutationDelayMs);
void SftpFileProviderPlugin::setKnownHostsFileForTesting(const QString &path) { testKnownHostsFile = path; }
#endif

SftpFileProviderPlugin::SftpFileProviderPlugin()
{
    ssh_init();
    qmlRegisterType<SftpConnectionController>("FMSftp", 1, 0, "SftpConnectionController");
}

bool SftpFileProviderPlugin::canHandle(const QString &path) const { return SftpPath::isSchemePath(path); }
std::unique_ptr<FileProvider> SftpFileProviderPlugin::createProvider()
{
#ifdef FM_SFTP_PROVIDER_TESTING
    return std::make_unique<SftpFileProvider>(testKnownHostsFile);
#else
    return std::make_unique<SftpFileProvider>();
#endif
}
QString SftpFileProviderPlugin::preprocessPath(const QString &path) const
{
    if (!canHandle(path)) return path;
    const auto address = SftpPath::parse(path);
    return address.valid() ? address.encoded() : QString{};
}

QList<FileActionDescriptor> SftpFileProviderPlugin::actionsForContext(const FileActionContext &context) const
{
    const auto address = SftpPath::parse(context.currentPath);
    if (!SftpPath::isSchemePath(context.currentPath)) return {};
    FileActionDescriptor manage;
    manage.id = QStringLiteral("manageConnections");
    manage.text = QStringLiteral("SFTP connections…");
    manage.order = 900;
    QList<FileActionDescriptor> result{manage};
    if (!address.valid() || !SftpSessions::credentials(address)) return result;
    FileActionDescriptor action;
    action.id = QStringLiteral("disconnect");
    action.text = QStringLiteral("Disconnect from SFTP server");
    action.order = 910;
    result.append(action);
    return result;
}

QVariantMap SftpFileProviderPlugin::triggerAction(const QString &actionId, const FileActionContext &context)
{
#ifdef FM_SFTP_PROVIDER_TESTING
    if (actionId == QLatin1String("transferRegression"))
        return {{QStringLiteral("ok"), runSftpQueueRegression() == 0}};
    if (actionId == QLatin1String("placesFixture")) {
        setSftpPlacesFixture(context.parameters.value(QStringLiteral("enabled")).toBool());
        return {{QStringLiteral("ok"), true}, {QStringLiteral("refreshPlaces"), true}};
    }
    if (actionId == QLatin1String("creationFixture"))
        return sftpCreationFixture(context.parameters.value(QStringLiteral("enabled")).toBool(),
                                   context.parameters.value(QStringLiteral("mutationDelayMs")).toInt());
#endif
    if (actionId == QLatin1String("connectionState")) {
        const auto address = SftpPath::parse(context.currentPath);
        return {{"ok", true}, {"connected", address.valid() && SftpSessions::credentials(address).has_value()}};
    }
    if (actionId == QLatin1String("manageConnections")) {
        return {{"ok", true}, {"resultType", "pluginUi"}, {"title", "SFTP connections"},
            {"subtitle", "SSH file transfer"}, {"iconSource", "qrc:/sftp/settings/sftp.svg"},
            {"nativeIconPresentation", true},
            {"componentUrl", "qrc:/sftp/settings/SftpConnectionsUi.qml"},
            {"context", QVariantMap{{"currentPath", context.currentPath}}}};
    }
    if (actionId == QLatin1String("disconnect")) {
        const auto address = SftpPath::parse(context.currentPath);
        if (!address.valid()) return {{QStringLiteral("ok"), false}, {QStringLiteral("message"), address.error}};
        SftpSessions::forget(address.identity());
    } else if (actionId != QLatin1String("refreshPlaces")) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("Unknown SFTP action.")}};
    }
    return {{QStringLiteral("ok"), true}, {QStringLiteral("refreshPlaces"), true},
            {QStringLiteral("statusOnly"), true}, {QStringLiteral("message"), QStringLiteral("SFTP connections updated.")}};
}

QList<ProviderPlaceItem> SftpFileProviderPlugin::places() const
{
    QList<ProviderPlaceItem> result;
    for (const auto &value : SftpProfiles::connections()) {
        const auto profile = value.toMap();
        const auto address = SftpPath::parse(profile.value("url").toString());
        if (!profile.value("connected").toBool() || !SftpSessions::credentials(address)) continue;
        ProviderPlaceItem item;
        item.name = profile.value("name").toString();
        item.path = address.encoded();
        item.icon = QStringLiteral("sftp");
        item.section = QStringLiteral("cloud");
        item.driveType = QStringLiteral("network");
        item.subtitle = QStringLiteral("SFTP · connected");
        result.append(item);
    }
    if (result.isEmpty()) return {};
    ProviderPlaceItem root;
    root.name = QStringLiteral("SFTP");
    root.path = QStringLiteral("sftp://");
    root.icon = QStringLiteral("sftp");
    root.section = QStringLiteral("cloud");
    root.subtitle = QStringLiteral("Servers and saved connections");
    result.prepend(root);
    return result;
}
