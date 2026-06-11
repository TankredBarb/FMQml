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
                    ? "../assets/toolbar-next/layout-grid.svg"
                    : (root.controller && root.controller.viewMode === 1
                       ? "../assets/toolbar-next/layout-list.svg"
                       : "../assets/toolbar-next/list.svg")
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
            radius: Theme.radiusForSide(Math.min(width, height))
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
        color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.28 : 0.20)
    }

    IconButton {
        id: eyeBtn
        iconSource: root.controller && root.controller.directoryModel.showHidden
                    ? "../assets/toolbar-next/eye-off.svg"
                    : "../assets/toolbar-next/eye.svg"
        iconTone: "hidden"
        enabled: !!root.controller
        onClicked: {
            const newValue = !root.controller.directoryModel.showHidden
            if (root.workspaceController) {
                root.workspaceController.leftPanel.directoryModel.showHidden = newValue
                root.workspaceController.rightPanel.directoryModel.showHidden = newValue
                root.workspaceController.treeModel.showHidden = newValue
            } else {
                root.controller.directoryModel.showHidden = newValue
            }
        }
        ToolTip.visible: hovered
        ToolTip.text: root.controller && root.controller.directoryModel.showHidden ? "Hide Hidden Files" : "Show Hidden Files"
        isHighlighted: root.controller && root.controller.directoryModel.showHidden
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            readonly property bool active: eyeBtn.isHighlighted

            radius: Theme.radiusForSide(Math.min(width, height))
            color: Theme.toolbarButtonFill(eyeBtn.baseTone, eyeBtn.hovered, eyeBtn.pressed, active)
            border.color: Theme.toolbarButtonBorder(eyeBtn.baseTone, eyeBtn.hovered, active)
            border.width: eyeBtn.hovered || eyeBtn.pressed || active ? 1 : 0
            anchors.fill: parent
            anchors.margins: 1

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
                width: parent.width - 13
                height: 2
                radius: 1
                visible: parent.active
                color: Theme.toolbarButtonIndicator(parent.active)
                opacity: eyeBtn.hovered ? 1.0 : 0.88
            }
        }
    }

    Rectangle {
        width: 1
        Layout.fillHeight: true
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.28 : 0.20)
    }

    IconButton {
        id: refreshBtn
        iconSource: "../assets/toolbar-next/refresh-cw.svg"
        iconTone: "refresh"
        enabled: !!root.controller
        onClicked: root.controller.refresh()
        ToolTip.visible: hovered
        ToolTip.text: "Refresh (Ctrl+R)"
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            radius: Theme.radiusForSide(Math.min(width, height))
            color: refreshBtn.pressed ? Theme.surfaceActive : (refreshBtn.hovered ? Theme.withAlpha(refreshBtn.baseTone, themeController.isDark ? 0.14 : 0.10) : "transparent")
            anchors.fill: parent
            anchors.margins: 1
        }
    }
}
