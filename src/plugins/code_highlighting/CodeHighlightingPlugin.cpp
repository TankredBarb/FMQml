#include "CodeHighlightingPlugin.h"

#include "CodeHighlighter.h"

int CodeHighlightingPlugin::textDecorationApiVersion() const
{
    return FM_TEXT_PREVIEW_DECORATION_PLUGIN_API_VERSION;
}

QString CodeHighlightingPlugin::textDecorationPluginId() const
{
    return QStringLiteral("fm.code-highlighting");
}

QString CodeHighlightingPlugin::textDecorationDisplayName() const
{
    return QStringLiteral("Code Highlighting");
}

TextDecorationResult CodeHighlightingPlugin::decorateText(
    const TextDecorationRequest &request) const
{
    return CodeHighlighter::decorate(request);
}

int CodeHighlightingPlugin::settingsUiApiVersion() const
{
    return FM_PLUGIN_SETTINGS_UI_API_VERSION;
}

QString CodeHighlightingPlugin::settingsUiPluginId() const
{
    return textDecorationPluginId();
}

QString CodeHighlightingPlugin::settingsUiTitle() const
{
    return textDecorationDisplayName();
}

QString CodeHighlightingPlugin::settingsUiComponentUrl() const
{
    return QStringLiteral("qrc:/code_highlighting/CodeHighlightingSettings.qml");
}

int CodeHighlightingPlugin::settingsUiOrder() const
{
    return 600;
}
