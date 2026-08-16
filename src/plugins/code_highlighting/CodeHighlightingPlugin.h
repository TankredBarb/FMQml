#pragma once

#include "../../core/PluginSettingsUi.h"
#include "../../core/TextPreviewDecorationPlugin.h"

#include <QObject>

class CodeHighlightingPlugin final : public QObject,
                                     public TextPreviewDecorationPlugin,
                                     public PluginSettingsUi
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FM_TEXT_PREVIEW_DECORATION_PLUGIN_IID)
    Q_INTERFACES(TextPreviewDecorationPlugin PluginSettingsUi)

public:
    CodeHighlightingPlugin();

    int textDecorationApiVersion() const override;
    QString textDecorationPluginId() const override;
    QString textDecorationDisplayName() const override;
    TextDecorationResult decorateText(const TextDecorationRequest &request) const override;

    int settingsUiApiVersion() const override;
    QString settingsUiPluginId() const override;
    QString settingsUiTitle() const override;
    QString settingsUiComponentUrl() const override;
    int settingsUiOrder() const override;
};
