import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../style"

ToolbarSegment {
    id: root

    property var controller
    property var workspaceController

    segmentWidth: 32 * 3 + 2
    segmentHeight: 32

    IconButton {
        id: viewBtn
        iconSource: root.controller && root.controller.viewMode === 0
                    ? "../assets/lucide-toolbar/layout-grid.svg"
                    : (root.controller && root.controller.viewMode === 1
                       ? "../assets/lucide-toolbar/layout-list.svg"
                       : "../assets/lucide-toolbar/list.svg")
        iconTone: root.controller && root.controller.viewMode === 0
                  ? "view-grid"
                  : (root.controller && root.controller.viewMode === 1
                     ? "view-brief"
                     : "view-details")
        enabled: !!root.controller
        onClicked: root.controller.viewMode = (root.controller.viewMode + 1) % 3
        ToolTip.visible: hovered
        ToolTip.text: root.controller && root.controller.viewMode === 0
                      ? "Switch to Grid"
                      : (root.controller && root.controller.viewMode === 1
                         ? "Switch to Brief"
                         : "Switch to Details")
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            radius: Theme.radiusSm
            color: viewBtn.pressed ? Theme.surfaceActive : (viewBtn.hovered ? Theme.withAlpha(viewBtn.baseTone, themeController.isDark ? 0.20 : 0.14) : "transparent")
            anchors.fill: parent
            anchors.margins: 1
        }
    }

    Rectangle {
        width: 1
        Layout.fillHeight: true
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        color: Theme.border
        opacity: 0.35
    }

    IconButton {
        id: eyeBtn
        iconSource: root.controller && root.controller.directoryModel.showHidden
                    ? "../assets/lucide-toolbar/eye-off.svg"
                    : "../assets/lucide-toolbar/eye.svg"
        iconTone: "hidden"
        enabled: !!root.controller
        onClicked: {
            const newValue = !root.controller.directoryModel.showHidden
            root.controller.directoryModel.showHidden = newValue
            if (root.workspaceController) {
                root.workspaceController.treeModel.showHidden = newValue
            }
        }
        ToolTip.visible: hovered
        ToolTip.text: root.controller && root.controller.directoryModel.showHidden ? "Hide Hidden Files" : "Show Hidden Files"
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            radius: Theme.radiusSm
            color: eyeBtn.pressed ? Theme.surfaceActive : (eyeBtn.hovered ? Theme.withAlpha(Theme.categoryUtility, themeController.isDark ? 0.14 : 0.10) : "transparent")
            anchors.fill: parent
            anchors.margins: 1
        }
    }

    Rectangle {
        width: 1
        Layout.fillHeight: true
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        color: Theme.border
        opacity: 0.35
    }

    IconButton {
        id: refreshBtn
        iconSource: "../assets/lucide-toolbar/refresh-cw.svg"
        iconTone: "refresh"
        enabled: !!root.controller
        onClicked: root.controller.refresh()
        ToolTip.visible: hovered
        ToolTip.text: "Refresh (F5)"
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            radius: Theme.radiusSm
            color: refreshBtn.pressed ? Theme.surfaceActive : (refreshBtn.hovered ? Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.14 : 0.10) : "transparent")
            anchors.fill: parent
            anchors.margins: 1
        }
    }
}
