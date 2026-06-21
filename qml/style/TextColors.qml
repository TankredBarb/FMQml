pragma Singleton
import QtQuick

QtObject {
    readonly property color fileNameText: resolveRole("fileNameText", Theme.textPrimary)
    readonly property color folderNameText: resolveRole("folderNameText", Theme.textPrimary)
    readonly property color fileExtensionText: resolveRole("fileExtensionText", Theme.textSecondary)
    readonly property color fileSecondaryText: resolveRole("fileSecondaryText", Theme.textSecondary)
    readonly property color filePathText: resolveRole("filePathText", Theme.textSecondary)
    readonly property color sidebarText: resolveRole("sidebarText", Theme.textPrimary)
    readonly property color thisPcText: resolveRole("thisPcText", Theme.textPrimary)
    readonly property color statusText: resolveRole("statusText", Theme.textSecondary)
    readonly property color dialogSecondaryText: resolveRole("dialogSecondaryText", Theme.textSecondary)
    readonly property color commandPaletteText: resolveRole("commandPaletteText", Theme.textPrimary)

    function resolveRole(roleId, fallbackColor) {
        if (typeof appSettings === "undefined" || !appSettings) {
            return fallbackColor
        }
        var map = appSettings.textColorOverrides
        if (map && map[roleId]) {
            var entry = map[roleId]
            if (entry.enabled && entry.color) {
                return entry.color
            }
        }
        return fallbackColor
    }
}
