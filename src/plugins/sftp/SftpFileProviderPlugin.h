#pragma once

#include "FileActionPlugin.h"
#include "FileProviderPlugin.h"
#include "PlacesProviderPlugin.h"
#include "PluginSettingsUi.h"

class SftpFileProviderPlugin final : public QObject, public FileProviderPlugin,
                                    public FileActionPlugin, public PlacesProviderPlugin,
                                    public PluginSettingsUi {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FM_FILE_PROVIDER_PLUGIN_IID)
    Q_INTERFACES(FileProviderPlugin FileActionPlugin PlacesProviderPlugin PluginSettingsUi)
public:
    SftpFileProviderPlugin();
    int apiVersion() const override { return FM_FILE_PROVIDER_PLUGIN_API_VERSION; }
    QString pluginId() const override { return QStringLiteral("fm.sftp-provider"); }
    QString displayName() const override { return QStringLiteral("SFTP"); }
    QStringList schemes() const override { return {QStringLiteral("sftp")}; }
    bool canHandle(const QString &path) const override;
    std::unique_ptr<FileProvider> createProvider() override;
    QString preprocessPath(const QString &path) const override;
    int actionApiVersion() const override { return FM_FILE_ACTION_PLUGIN_API_VERSION; }
    QString actionPluginId() const override { return pluginId(); }
    QString actionDisplayName() const override { return displayName(); }
    QList<FileActionDescriptor> actionsForContext(const FileActionContext &context) const override;
    QVariantMap triggerAction(const QString &actionId, const FileActionContext &context) override;
    int placesApiVersion() const override { return FM_PLACES_PROVIDER_PLUGIN_API_VERSION; }
    QString placesPluginId() const override { return pluginId(); }
    QString placesDisplayName() const override { return displayName(); }
    QList<ProviderPlaceItem> places() const override;
    int settingsUiApiVersion() const override { return FM_PLUGIN_SETTINGS_UI_API_VERSION; }
    QString settingsUiPluginId() const override { return pluginId(); }
    QString settingsUiTitle() const override { return displayName(); }
    QString settingsUiComponentUrl() const override { return QStringLiteral("qrc:/sftp/settings/SftpSettingsCard.qml"); }
    int settingsUiOrder() const override { return 250; }
#ifdef FM_SFTP_PROVIDER_TESTING
    static void setKnownHostsFileForTesting(const QString &path);
#endif
};
