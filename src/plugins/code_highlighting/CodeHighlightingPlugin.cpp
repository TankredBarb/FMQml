#include "CodeHighlightingPlugin.h"

#include "CodeHighlighter.h"

#include <QColor>
#include <QSettings>

namespace {
void migrateDocumentPaletteColor(QSettings &settings, const QString &key,
                                 const QString &legacyDefault, const QString &newDefault)
{
    const QVariant stored = settings.value(key);
    if (!stored.isValid() || QColor(stored.toString()) == QColor(legacyDefault)) {
        settings.setValue(key, newDefault);
    }
}
}

CodeHighlightingPlugin::CodeHighlightingPlugin()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("CodeHighlighting"));
    if (settings.value(QStringLiteral("documentPaletteVersion"), 0).toInt() < 1) {
        migrateDocumentPaletteColor(settings, QStringLiteral("keywordColor"),
                                    QStringLiteral("#c678dd"), QStringLiteral("#7c3aed"));
        migrateDocumentPaletteColor(settings, QStringLiteral("stringColor"),
                                    QStringLiteral("#98c379"), QStringLiteral("#18794e"));
        migrateDocumentPaletteColor(settings, QStringLiteral("commentColor"),
                                    QStringLiteral("#7f848e"), QStringLiteral("#667085"));
        migrateDocumentPaletteColor(settings, QStringLiteral("literalColor"),
                                    QStringLiteral("#d19a66"), QStringLiteral("#b45309"));
        settings.setValue(QStringLiteral("documentPaletteVersion"), 1);
    }
}

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
