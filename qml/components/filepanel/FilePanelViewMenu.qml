import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../style"

Item {
    id: root

    property var controller
    property bool showActionBar: true
    readonly property var directoryModel: root.controller ? root.controller.directoryModel : null
    signal actionBarVisibilityRequested(bool visible)

    implicitWidth: 70
    implicitHeight: 32
    visible: root.controller ? !root.controller.isDeviceRoot && !root.controller.isFavoritesRoot : false

    FilePanelFilterPopover {
        id: filterPopover
        controller: root.controller
    }

    Row {
        anchors.fill: parent
        spacing: 6

        IconButton {
            id: panelViewToggle
            width: 32
            height: 32
            visible: root.controller ? !root.controller.isDeviceRoot && !root.controller.isFavoritesRoot : false
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
                    iconColor: Theme.actionIconColor("view-details")
                    onTriggered: root.controller.viewMode = 0
                }
                ThemedMenuItem {
                    text: "Grid"
                    icon.source: "../assets/lucide-toolbar/layout-grid.svg"
                    iconColor: Theme.actionIconColor("view-grid")
                    onTriggered: root.controller.viewMode = 1
                }
                ThemedMenuItem {
                    text: "Brief"
                    icon.source: "../assets/lucide-toolbar/layout-list.svg"
                    iconColor: Theme.actionIconColor("view-brief")
                    onTriggered: root.controller.viewMode = 2
                }
                ThemedMenuSeparator {}
                ThemedMenuItem {
                    text: root.controller && root.controller.directoryModel && root.controller.directoryModel.mixFilesAndFolders
                          ? "Separate Folders"
                          : "Mix Files & Folders"
                    icon.source: "../assets/icons/list.svg"
                    iconColor: Theme.actionIconColor("sort")
                    onTriggered: {
                        const newValue = !root.controller.directoryModel.mixFilesAndFolders
                        root.controller.directoryModel.mixFilesAndFolders = newValue
                    }
                }
                ThemedMenuSeparator {}
                ThemedMenuItem {
                    text: root.showActionBar ? "Hide Action Bar" : "Show Action Bar"
                    icon.source: root.showActionBar ? "../assets/icons/eye-off.svg" : "../assets/icons/eye.svg"
                    iconColor: Theme.actionIconColor("hidden")
                    onTriggered: root.actionBarVisibilityRequested(!root.showActionBar)
                }
            }
        }

        IconButton {
            id: filterButton
            width: 32
            height: 32
            iconSource: "../assets/lucide-toolbar/funnel.svg"
            iconTone: "filter"
            isHighlighted: root.controller && root.controller.categoryFilterActive
            onClicked: filterPopover.openAt(filterButton)
            ToolTip.visible: hovered
            ToolTip.text: root.controller && root.controller.categoryFilterActive
                          ? "Filters - " + root.controller.categoryFilterSummary
                            + (root.controller.categoryFilterSuspended ? " (Suspended)" : "")
                          : "Open Filters"

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 2
                anchors.topMargin: 2
                width: 8
                height: 8
                radius: 4
                visible: root.controller && root.controller.categoryFilterActive
                color: root.controller && root.controller.categoryFilterSuspended
                       ? Theme.warning
                       : Theme.categoryUtility
                border.color: Theme.panelSurface
                border.width: 1
            }
        }
    }
}
