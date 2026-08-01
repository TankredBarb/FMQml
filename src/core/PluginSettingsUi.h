#pragma once

#include <QString>
#include <QList>
#include <QtPlugin>

inline constexpr int FM_PLUGIN_SETTINGS_UI_API_VERSION = 1;

struct PluginSettingsUiDescriptor {
    QString pluginId;
    QString title;
    QString componentUrl;
    int order = 0;
};

class PluginSettingsUi
{
public:
    virtual ~PluginSettingsUi() = default;
    virtual int settingsUiApiVersion() const = 0;
    virtual QString settingsUiPluginId() const = 0;
    virtual QString settingsUiTitle() const = 0;
    virtual QString settingsUiComponentUrl() const = 0;
    virtual int settingsUiOrder() const = 0;
};

#define FM_PLUGIN_SETTINGS_UI_IID "FM.PluginSettingsUi/1.0"
Q_DECLARE_INTERFACE(PluginSettingsUi, FM_PLUGIN_SETTINGS_UI_IID)

bool pluginSettingsUiDescriptor(const PluginSettingsUi *settingsUi,
                                const QString &expectedPluginId,
                                PluginSettingsUiDescriptor *descriptor,
                                QString *error);
void sortPluginSettingsUiDescriptors(QList<PluginSettingsUiDescriptor> *descriptors);
