import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property var workspaceController
    property int panelSide: -1
    property string currentPath: ""
    property bool externalDropSuppressed: false
    property var dropCapabilities: ({})
    property string dropCapabilityKey: ""
    signal statusMessageRequested(string message)

    readonly property bool dropAllowed: Boolean(root.dropCapabilities && root.dropCapabilities.canCopy)

    readonly property bool readOnlyDestination: {
        if (!root.currentPath) return false
        if (root.currentPath.toLowerCase().startsWith("archive://")) return true
        return root.workspaceController && root.workspaceController.isInsideManagedIsoMount(root.currentPath)
    }

    function urlsKey(urls) {
        if (!urls) {
            return root.currentPath + "|"
        }
        return root.currentPath + "|" + urls.join("\n")
    }

    function capabilitiesFor(drop) {
        if (!root.workspaceController || !drop || !drop.hasUrls || root.panelSide < 0) {
            root.dropCapabilities = ({ canCopy: false, reason: "" })
            root.dropCapabilityKey = ""
            return root.dropCapabilities
        }
        const key = root.urlsKey(drop.urls)
        if (key !== root.dropCapabilityKey) {
            root.dropCapabilities = root.workspaceController.externalDropCapabilities(
                        drop.urls, root.panelSide, root.currentPath)
            root.dropCapabilityKey = key
        }
        return root.dropCapabilities
    }

    function acceptIfAllowed(drop) {
        const capabilities = root.capabilitiesFor(drop)
        if (capabilities && capabilities.canCopy) {
            drop.accept(Qt.CopyAction)
        }
    }

    DropArea {
        anchors.fill: parent
        enabled: !root.readOnlyDestination && !root.externalDropSuppressed
        onEntered: (drop) => root.acceptIfAllowed(drop)
        onPositionChanged: (drop) => root.acceptIfAllowed(drop)
        onExited: {
            root.dropCapabilities = ({})
            root.dropCapabilityKey = ""
        }
        onDropped: (drop) => {
            if (drop.hasUrls) {
                const capabilities = root.capabilitiesFor(drop)
                if (capabilities && capabilities.canCopy) {
                    if (root.workspaceController.copyExternalUrlsToPanel(
                                drop.urls, root.panelSide, root.currentPath)) {
                        drop.accept(Qt.CopyAction)
                    }
                } else if (capabilities && capabilities.reason) {
                    root.statusMessageRequested(capabilities.reason)
                }
            }
            root.dropCapabilities = ({})
            root.dropCapabilityKey = ""
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Theme.innerRadius(Theme.panelRadius, 1)
            color: root.dropAllowed
                   ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.075 : 0.055)
                   : Theme.withAlpha(Theme.danger, themeController.isDark ? 0.075 : 0.055)
            opacity: parent.containsDrag ? 1 : 0
            visible: parent.containsDrag
            border.color: root.dropAllowed
                          ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.46 : 0.36)
                          : Theme.withAlpha(Theme.danger, themeController.isDark ? 0.46 : 0.36)
            border.width: 1
            antialiasing: true
        }
    }
}
