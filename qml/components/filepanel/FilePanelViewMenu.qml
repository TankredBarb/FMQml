import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../style"

Item {
    id: root

    property var controller

    implicitWidth: 32
    implicitHeight: 32
    visible: root.controller ? !root.controller.isDeviceRoot : false

    IconButton {
        id: panelViewToggle
        anchors.fill: parent
        visible: root.controller ? !root.controller.isDeviceRoot : false
        iconSource: root.controller && root.controller.viewMode === 0
                    ? "../assets/lucide-toolbar/list.svg"
                    : (root.controller && root.controller.viewMode === 1
                       ? "../assets/lucide-toolbar/layout-grid.svg"
                       : "../assets/lucide-toolbar/layout-list.svg")
        iconTone: root.controller && root.controller.viewMode === 0
                  ? "view-details"
                  : (root.controller && root.controller.viewMode === 1
                     ? "view-grid"
                     : "view-brief")
        onClicked: viewMenu.popup()
        ToolTip.visible: hovered
        ToolTip.text: "Change View Mode"

        ThemedContextMenu {
            id: viewMenu
            ThemedMenuItem {
                text: "Details"
                icon.source: "../assets/lucide-toolbar/list.svg"
                iconColor: "#10b981"
                onTriggered: root.controller.viewMode = 0
            }
            ThemedMenuItem {
                text: "Grid"
                icon.source: "../assets/lucide-toolbar/layout-grid.svg"
                iconColor: "#8b5cf6"
                onTriggered: root.controller.viewMode = 1
            }
            ThemedMenuItem {
                text: "Brief"
                icon.source: "../assets/lucide-toolbar/layout-list.svg"
                iconColor: "#3b82f6"
                onTriggered: root.controller.viewMode = 2
            }
            ThemedMenuSeparator {}
            ThemedMenuItem {
                text: root.controller && root.controller.directoryModel && root.controller.directoryModel.mixFilesAndFolders
                      ? "Separate Folders"
                      : "Mix Files & Folders"
                icon.source: "../assets/icons/list.svg"
                iconColor: "#64748b"
                onTriggered: {
                    const newValue = !root.controller.directoryModel.mixFilesAndFolders
                    root.controller.directoryModel.mixFilesAndFolders = newValue
                }
            }
        }
    }
}
