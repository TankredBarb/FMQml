#include "PortableDeviceFileProviderPlugin.h"

#include "PortableDevicePath.h"
#include "PortableDeviceProvider.h"

int PortableDeviceFileProviderPlugin::apiVersion() const
{
    return FM_FILE_PROVIDER_PLUGIN_API_VERSION;
}

QString PortableDeviceFileProviderPlugin::pluginId() const
{
    return QStringLiteral("fm.portable-device-provider");
}

QString PortableDeviceFileProviderPlugin::displayName() const
{
    return QStringLiteral("Portable Device Provider");
}

QStringList PortableDeviceFileProviderPlugin::schemes() const
{
    return {QStringLiteral("portable")};
}

bool PortableDeviceFileProviderPlugin::canHandle(const QString &path) const
{
    return !PortableDevicePath::normalized(path).isEmpty();
}

std::unique_ptr<FileProvider> PortableDeviceFileProviderPlugin::createProvider()
{
    return createPortableDeviceProvider();
}

int PortableDeviceFileProviderPlugin::placesApiVersion() const
{
    return FM_PLACES_PROVIDER_PLUGIN_API_VERSION;
}

QString PortableDeviceFileProviderPlugin::placesPluginId() const
{
    return pluginId();
}

QString PortableDeviceFileProviderPlugin::placesDisplayName() const
{
    return displayName();
}

QList<ProviderPlaceItem> PortableDeviceFileProviderPlugin::places() const
{
    return portableDevicePlaces();
}
