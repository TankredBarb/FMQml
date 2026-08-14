#include "FileProviderPluginRegistry.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
int fail(const QString &message)
{
    qCritical().noquote() << message;
    return 1;
}

bool hasSettings(const QList<PluginSettingsUiDescriptor> &descriptors,
                 const QString &pluginId)
{
    for (const PluginSettingsUiDescriptor &descriptor : descriptors) {
        if (descriptor.pluginId == pluginId) {
            return true;
        }
    }
    return false;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString pluginId = QStringLiteral("fm.code-highlighting");
    FileProviderPluginRegistry &registry = FileProviderPluginRegistry::instance();
    registry.loadPluginFile(QString::fromUtf8(FM_CODE_HIGHLIGHTING_PLUGIN_PATH));

    TextDecorationRequest request;
    request.fileName = QStringLiteral("sample.sh");
    request.content = QStringLiteral("if true; then # lifecycle\n  echo ok\nfi\n");
    if (!hasSettings(registry.settingsUiDescriptors(), pluginId)
        || !registry.decorateText(request).supported) {
        return fail(QStringLiteral("loaded plugin did not expose settings and decoration"));
    }

    if (!registry.unloadPlugin(pluginId)) {
        return fail(QStringLiteral("loaded plugin could not be unloaded"));
    }
    if (hasSettings(registry.settingsUiDescriptors(), pluginId)) {
        return fail(QStringLiteral("unloaded plugin still exposed its settings UI"));
    }
    if (registry.decorateText(request).supported) {
        return fail(QStringLiteral("unloaded plugin still decorated text"));
    }

    return 0;
}
