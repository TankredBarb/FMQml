import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../style"

RowLayout {
    id: root

    property var controller
    property var workspaceController
    property var appRoot
    property bool previewVisible: false

    signal previewToggleRequested(bool visible)
    signal helpRequested()

    function isReadOnlyContainerPath(path) {
        if (!path) return false
        if (path.toLowerCase().startsWith("archive://")) return true
        return root.workspaceController && root.workspaceController.isInsideManagedIsoMount(path)
    }

    function openThemeSelector() {
        themeMenu.openAt(themeBtn)
    }

    spacing: 6

    ToolbarSegment {
        segmentWidth: 32 * 2 + 1
        segmentHeight: 32
        visible: root.workspaceController ? root.workspaceController.splitEnabled : false

        IconButton {
            id: copyBtn
            iconSource: "../assets/lucide-toolbar/copy.svg"
            iconTone: "copy"
            enabled: root.workspaceController && root.controller
                     ? root.workspaceController.splitEnabled
                       && root.controller.directoryModel.selectedCount > 0
                       && (root.workspaceController.activePanel === 0
                           ? root.workspaceController.rightPanel.canCreateInCurrentPath
                           : root.workspaceController.leftPanel.canCreateInCurrentPath)
                       && !root.workspaceController.operationQueue.busy
                     : false
            onClicked: root.workspaceController.copyActiveSelectionToOpposite()
            isHighlighted: enabled && hovered
            ToolTip.visible: hovered
            ToolTip.text: "Copy to other panel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: Theme.radiusSm
                color: copyBtn.pressed ? Theme.surfaceActive : (copyBtn.hovered ? Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.14 : 0.10) : "transparent")
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
            id: moveBtn
            iconSource: "../assets/lucide-toolbar/move.svg"
            iconTone: "move"
            enabled: root.workspaceController && root.controller
                     ? root.workspaceController.splitEnabled
                       && root.controller.canDeleteSelection
                       && (root.workspaceController.activePanel === 0
                           ? root.workspaceController.rightPanel.canCreateInCurrentPath
                           : root.workspaceController.leftPanel.canCreateInCurrentPath)
                       && !root.workspaceController.operationQueue.busy
                     : false
            onClicked: root.workspaceController.moveActiveSelectionToOpposite()
            isHighlighted: enabled && hovered
            ToolTip.visible: hovered
            ToolTip.text: "Move to other panel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: Theme.radiusSm
                color: moveBtn.pressed ? Theme.surfaceActive : (moveBtn.hovered ? Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.14 : 0.10) : "transparent")
                anchors.fill: parent
                anchors.margins: 1
            }
        }
    }

    IconButton {
        iconSource: "../assets/lucide-toolbar/folder-plus.svg"
        iconTone: "folder"
        enabled: root.controller && root.controller.canCreateInCurrentPath
        onClicked: {
            if (root.controller) {
                root.controller.createFolder("New Folder")
            }
        }
        ToolTip.visible: hovered
        ToolTip.text: "Create Folder"
    }

    ToolbarSegment {
        segmentWidth: 32 * 2 + 1
        segmentHeight: 32

        IconButton {
            id: layoutSplitBtn
            iconSource: "../assets/lucide-toolbar/columns-2.svg"
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
                radius: Theme.radiusSm
                color: layoutSplitBtn.pressed ? Theme.surfaceActive : (layoutSplitBtn.hovered || layoutSplitBtn.isHighlighted ? Theme.withAlpha(Theme.categoryNavigation, themeController.isDark ? 0.16 : 0.12) : "transparent")
                border.color: layoutSplitBtn.isHighlighted ? Theme.accent : "transparent"
                border.width: layoutSplitBtn.isHighlighted ? 1 : 0
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
            id: layoutPreviewBtn
            iconSource: "../assets/lucide-toolbar/panel-right.svg"
            iconTone: "info"
            isHighlighted: root.previewVisible
            onClicked: root.previewToggleRequested(!root.previewVisible)
            ToolTip.visible: hovered
            ToolTip.text: root.previewVisible ? "Hide Preview" : "Show Preview"
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: Theme.radiusSm
                color: layoutPreviewBtn.pressed ? Theme.surfaceActive : (layoutPreviewBtn.hovered || layoutPreviewBtn.isHighlighted ? Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.16 : 0.12) : "transparent")
                border.color: layoutPreviewBtn.isHighlighted ? Theme.accent : "transparent"
                border.width: layoutPreviewBtn.isHighlighted ? 1 : 0
                anchors.fill: parent
                anchors.margins: 1
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
                radius: Theme.radiusSm
                color: themeBtn.pressed ? Theme.surfaceActive : (themeBtn.hovered ? Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.14 : 0.10) : "transparent")
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
            color: Theme.border
            opacity: 0.35
        }

        IconButton {
            id: helpBtn
            iconSource: "../assets/lucide-toolbar/info.svg"
            iconTone: "info"
            onClicked: root.helpRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Help (F1)"
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: Theme.radiusSm
                color: helpBtn.pressed ? Theme.surfaceActive : (helpBtn.hovered ? Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.14 : 0.10) : "transparent")
                anchors.fill: parent
                anchors.margins: 1
            }
        }
    }
}
