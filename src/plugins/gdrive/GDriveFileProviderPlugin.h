#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>

#include "FileActionPlugin.h"
#include "FileProviderPlugin.h"
#include "PluginSettingsUi.h"

class GDriveFileProviderPlugin final : public QObject, public FileProviderPlugin, public FileActionPlugin, public PluginSettingsUi
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FM_FILE_PROVIDER_PLUGIN_IID)
    Q_INTERFACES(FileProviderPlugin FileActionPlugin PluginSettingsUi)

public:
    int apiVersion() const override;
    QString pluginId() const override;
    QString displayName() const override;
    QStringList schemes() const override;
    bool canHandle(const QString &path) const override;
    std::unique_ptr<FileProvider> createProvider() override;

    int actionApiVersion() const override;
    QString actionPluginId() const override;
    QString actionDisplayName() const override;
    QList<FileActionDescriptor> actionsForContext(const FileActionContext &context) const override;
    QVariantMap triggerAction(const QString &actionId, const FileActionContext &context) override;
    int settingsUiApiVersion() const override;
    QString settingsUiPluginId() const override;
    QString settingsUiTitle() const override;
    QString settingsUiComponentUrl() const override;
    int settingsUiOrder() const override;
};
