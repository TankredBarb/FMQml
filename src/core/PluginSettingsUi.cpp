#include "PluginSettingsUi.h"

#include <algorithm>

bool pluginSettingsUiDescriptor(const PluginSettingsUi *settingsUi,
                                const QString &expectedPluginId,
                                PluginSettingsUiDescriptor *descriptor,
                                QString *error)
{
    if (!settingsUi) {
        if (error) *error = QStringLiteral("settings UI capability is missing");
        return false;
    }
    if (settingsUi->settingsUiApiVersion() != FM_PLUGIN_SETTINGS_UI_API_VERSION) {
        if (error) {
            *error = QStringLiteral("unsupported settings UI API version %1")
                         .arg(settingsUi->settingsUiApiVersion());
        }
        return false;
    }

    const QString pluginId = settingsUi->settingsUiPluginId().trimmed();
    if (pluginId.isEmpty() || pluginId != expectedPluginId.trimmed()) {
        if (error) *error = QStringLiteral("plugin interface ids do not match");
        return false;
    }

    const QString componentUrl = settingsUi->settingsUiComponentUrl().trimmed();
    if (!componentUrl.startsWith(QStringLiteral("qrc:/"))) {
        if (error) *error = QStringLiteral("settings UI component URL must use qrc:/");
        return false;
    }

    if (descriptor) {
        *descriptor = {pluginId,
                       settingsUi->settingsUiTitle().trimmed(),
                       componentUrl,
                       settingsUi->settingsUiOrder()};
    }
    if (error) error->clear();
    return true;
}

void sortPluginSettingsUiDescriptors(QList<PluginSettingsUiDescriptor> *descriptors)
{
    if (!descriptors) return;
    std::sort(descriptors->begin(), descriptors->end(), [](const auto &lhs, const auto &rhs) {
        return lhs.order == rhs.order ? lhs.pluginId < rhs.pluginId : lhs.order < rhs.order;
    });
}
