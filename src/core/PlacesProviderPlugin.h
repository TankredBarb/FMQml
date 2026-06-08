#pragma once

#include <QList>
#include <QString>
#include <QtPlugin>

inline constexpr int FM_PLACES_PROVIDER_PLUGIN_API_VERSION = 1;

struct ProviderPlaceItem {
    QString name;
    QString path;
    QString icon;
    QString section;
    QString driveType;
    QString subtitle;
    bool isReady = true;
    bool canEject = false;
};

class PlacesProviderPlugin
{
public:
    virtual ~PlacesProviderPlugin() = default;

    virtual int placesApiVersion() const = 0;
    virtual QString placesPluginId() const = 0;
    virtual QString placesDisplayName() const = 0;
    virtual QList<ProviderPlaceItem> places() const = 0;
};

#define FM_PLACES_PROVIDER_PLUGIN_IID "FM.PlacesProviderPlugin/1.0"

Q_DECLARE_INTERFACE(PlacesProviderPlugin, FM_PLACES_PROVIDER_PLUGIN_IID)
