import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../filepanel"
import "../../style"

RowLayout {
    id: root

    property var controller
    property var workspaceController
    property var appRoot
    property bool previewVisible: false

    signal previewToggleRequested(bool visible)
    signal helpRequested()

    function openThemeSelector() {
        themeMenu.openAt(themeBtn)
    }

    FilePanelActionPolicy {
        id: actionPolicy
        controller: root.controller
        workspaceController: root.workspaceController
    }

    spacing: 6

    ToolbarSegment {
        segmentWidth: 32 * 2 + 1
        segmentHeight: 32
        visible: root.workspaceController ? root.workspaceController.splitEnabled : false

        IconButton {
            id: copyBtn
            iconSource: "../assets/toolbar-next/copy-to-panel.svg"
            iconTone: "copy"
            enabled: actionPolicy.canCopySelectionToOtherPanel()
            onClicked: root.workspaceController.copyActiveSelectionToOpposite()
            isHighlighted: enabled && hovered
            ToolTip.visible: hovered
            ToolTip.text: "Copy to other panel (F5)"
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: Theme.radiusForSide(Math.min(width, height))
                color: copyBtn.pressed ? Theme.surfaceActive : (copyBtn.hovered ? Theme.withAlpha(copyBtn.baseTone, themeController.isDark ? 0.14 : 0.10) : "transparent")
                anchors.fill: parent
                anchors.margins: 1
            }
        }

        Rectangle {
            width: 1
            Layout.fillHeight: true
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            visible: !actionPolicy.currentPathIsProvider() && !actionPolicy.oppositePathIsProvider()
            color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.28 : 0.20)
        }

        IconButton {
            id: moveBtn
            iconSource: "../assets/toolbar-next/move-to-panel.svg"
            iconTone: "move"
            visible: !actionPolicy.currentPathIsProvider() && !actionPolicy.oppositePathIsProvider()
            enabled: actionPolicy.canMoveSelectionToOtherPanel()
            onClicked: root.workspaceController.moveActiveSelectionToOpposite()
            isHighlighted: enabled && hovered
            ToolTip.visible: hovered
            ToolTip.text: "Move to other panel (Shift+F5)"
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: Theme.radiusForSide(Math.min(width, height))
                color: moveBtn.pressed ? Theme.surfaceActive : (moveBtn.hovered ? Theme.withAlpha(moveBtn.baseTone, themeController.isDark ? 0.14 : 0.10) : "transparent")
                anchors.fill: parent
                anchors.margins: 1
            }
        }
    }

    IconButton {
        iconSource: "../assets/toolbar-next/folder-plus.svg"
        iconTone: "folder"
        visible: !actionPolicy.currentPathIsProvider()
        enabled: actionPolicy.canCreateManualItem()
        onClicked: {
            if (root.controller) {
                root.controller.createFolder("New Folder")
            }
        }
        ToolTip.visible: hovered
        ToolTip.text: "Create Folder"
    }

    ToolbarSegment {
        segmentWidth: 32 * 3 + 2
        segmentHeight: 32

        IconButton {
            id: layoutSplitBtn
            iconSource: "../assets/toolbar-next/columns-2.svg"
            iconTone: "split"
            isHighlighted: root.workspaceController && root.workspaceController.splitEnabled
            enabled: root.workspaceController !== null && root.workspaceController !== undefined
            onClicked: {
                if (root.appRoot) {
                    root.appRoot.toggleSplitView()
                } else if (root.workspaceController) {
                    root.workspaceController.toggleSplit()
                }
            }
            ToolTip.visible: hovered
            ToolTip.text: "Toggle Split View (F3)"
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                readonly property bool active: layoutSplitBtn.isHighlighted

                radius: Theme.radiusForSide(Math.min(width, height))
                color: Theme.toolbarButtonFill(layoutSplitBtn.baseTone, layoutSplitBtn.hovered, layoutSplitBtn.pressed, active)
                border.color: Theme.toolbarButtonBorder(layoutSplitBtn.baseTone, layoutSplitBtn.hovered, active)
                border.width: layoutSplitBtn.hovered || layoutSplitBtn.pressed || active ? 1 : 0
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
                    opacity: layoutSplitBtn.hovered ? 1.0 : 0.88
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
            id: mirrorPanelBtn
            iconSource: "../assets/toolbar-next/panel-open.svg"
            iconTone: "split"
            enabled: root.workspaceController !== null && root.workspaceController !== undefined
            onClicked: {
                if (root.appRoot) {
                    root.appRoot.mirrorActivePanelToOpposite()
                } else if (root.workspaceController) {
                    root.workspaceController.mirrorActivePanelToOpposite()
                }
            }
            ToolTip.visible: hovered
            ToolTip.text: "Mirror active panel (F4)"
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: Theme.radiusForSide(Math.min(width, height))
                color: mirrorPanelBtn.pressed ? Theme.surfaceActive : (mirrorPanelBtn.hovered ? Theme.withAlpha(mirrorPanelBtn.baseTone, themeController.isDark ? 0.14 : 0.10) : "transparent")
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
            id: layoutPreviewBtn
            iconSource: "../assets/toolbar-next/panel-right.svg"
            iconTone: "info"
            isHighlighted: root.previewVisible
            onClicked: root.previewToggleRequested(!root.previewVisible)
            ToolTip.visible: hovered
            ToolTip.text: root.previewVisible ? "Hide Preview" : "Show Preview"
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                readonly property bool active: layoutPreviewBtn.isHighlighted

                radius: Theme.radiusForSide(Math.min(width, height))
                color: Theme.toolbarButtonFill(layoutPreviewBtn.baseTone, layoutPreviewBtn.hovered, layoutPreviewBtn.pressed, active)
                border.color: Theme.toolbarButtonBorder(layoutPreviewBtn.baseTone, layoutPreviewBtn.hovered, active)
                border.width: layoutPreviewBtn.hovered || layoutPreviewBtn.pressed || active ? 1 : 0
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
                    opacity: layoutPreviewBtn.hovered ? 1.0 : 0.88
                }
            }
        }
    }

    ToolbarSegment {
        segmentWidth: 32 * 2 + 1
        segmentHeight: 32

        IconButton {
            id: themeBtn
            iconSource: "../assets/icons/settings.svg"
            iconTone: "theme"
            onClicked: root.openThemeSelector()
            ToolTip.visible: hovered
            ToolTip.text: themeController.customThemeLoaded ? "Theme Schemes - Custom" : ("Theme Schemes - " + themeController.schemeName)
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: Theme.radiusForSide(Math.min(width, height))
                color: themeBtn.pressed ? Theme.surfaceActive : (themeBtn.hovered ? Theme.withAlpha(themeBtn.baseTone, themeController.isDark ? 0.14 : 0.10) : "transparent")
                anchors.fill: parent
                anchors.margins: 1
            }
        }

        ThemeSelectorMenu {
            id: themeMenu
        }

        Rectangle {
            width: 1
            Layout.fillHeight: true
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.28 : 0.20)
        }

        IconButton {
            id: helpBtn
            iconSource: "../assets/toolbar-next/info.svg"
            iconTone: "info"
            onClicked: root.helpRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Help (F1)"
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: Theme.radiusForSide(Math.min(width, height))
                color: helpBtn.pressed ? Theme.surfaceActive : (helpBtn.hovered ? Theme.withAlpha(helpBtn.baseTone, themeController.isDark ? 0.14 : 0.10) : "transparent")
                anchors.fill: parent
                anchors.margins: 1
            }
        }
    }
}
