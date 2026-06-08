#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>

#include "FileProviderPlugin.h"
#include "PlacesProviderPlugin.h"

class PortableDeviceFileProviderPlugin final : public QObject, public FileProviderPlugin, public PlacesProviderPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FM_FILE_PROVIDER_PLUGIN_IID)
    Q_INTERFACES(FileProviderPlugin PlacesProviderPlugin)

public:
    int apiVersion() const override;
    QString pluginId() const override;
    QString displayName() const override;
    QStringList schemes() const override;
    bool canHandle(const QString &path) const override;
    std::unique_ptr<FileProvider> createProvider() override;

    int placesApiVersion() const override;
    QString placesPluginId() const override;
    QString placesDisplayName() const override;
    QList<ProviderPlaceItem> places() const override;
};
