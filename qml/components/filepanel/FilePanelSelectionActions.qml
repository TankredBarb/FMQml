import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../style"

Rectangle {
    id: root

    property var controller
    property var workspaceController
    property var favoritesController
    property bool active: false
    property bool resizeOptimized: false
    property bool ultraLightMode: false
    property bool invertSelectionActive: false
    property bool canInvertSelection: false
    property int selectionRevision: 0

    readonly property int selectedCount: root.controller && root.controller.directoryModel
                                         ? root.controller.directoryModel.selectedCount
                                         : 0
    readonly property bool hasSelection: selectedCount > 0
    readonly property bool visibleForSelection: root.hasSelection || root.invertSelectionActive
    readonly property bool operationsBusy: root.workspaceController
                                           && root.workspaceController.operationQueue
                                           && root.workspaceController.operationQueue.busy
    readonly property bool oppositePanelAvailable: Boolean(root.workspaceController && root.workspaceController.splitEnabled)
    readonly property bool canCopyOrMoveToOtherPanel: root.hasSelection && !root.operationsBusy && root.oppositePanelAvailable
    readonly property var selectedPaths: {
        root.selectionRevision
        return root.hasSelection && root.controller && root.controller.selectedPaths
            ? root.controller.selectedPaths()
            : []
    }
    readonly property bool selectionContainsArchivePath: {
        for (let i = 0; i < selectedPaths.length; ++i) {
            if (String(selectedPaths[i]).toLowerCase().startsWith("archive://")) {
                return true
            }
        }
        return false
    }
    readonly property bool canToggleFavorite: root.hasSelection
                                              && !root.selectionContainsArchivePath
                                              && root.favoritesController
                                              && root.controller
                                              && !root.controller.isVirtualRoot
    readonly property string singlePath: selectedPaths.length === 1 ? selectedPaths[0] : ""
    readonly property int singleIndex: root.singlePath.length > 0 && root.controller && root.controller.directoryModel
                                       ? root.controller.directoryModel.indexOfPath(root.singlePath)
                                       : -1
    readonly property bool singleIsDirectory: root.singleIndex >= 0 && root.controller && root.controller.directoryModel
                                              ? root.controller.directoryModel.isDirectoryAt(root.singleIndex)
                                              : false
    readonly property bool allSelectedPinned: {
        if (!root.favoritesController || selectedPaths.length === 0 || root.selectionContainsArchivePath) {
            return false
        }
        const revision = root.favoritesController.pinnedCount
        for (let i = 0; i < selectedPaths.length; ++i) {
            if (!root.favoritesController.isPinned(selectedPaths[i])) {
                return false
            }
        }
        return revision >= 0
    }

    signal copyRequested()
    signal copyToOtherPanelRequested()
    signal moveRequested()
    signal renameRequested()
    signal deleteRequested()
    signal pinToggleRequested(var paths, bool allPinned)
    signal propertiesRequested()
    signal clearSelectionRequested()
    signal invertSelectionRequested()

    implicitHeight: 44
    visible: root.visibleForSelection
    color: Theme.panelSurfaceStrong
    border.width: 0

    Connections {
        target: root.controller && root.controller.directoryModel ? root.controller.directoryModel : null
        function onSelectionChanged() {
            root.selectionRevision += 1
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: root.active
               ? Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.40 : 0.56)
               : Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.26)
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: root.active
               ? Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.28 : 0.38)
               : Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.28 : 0.22)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        FileIconCell {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            iconSize: 20
            visible: root.selectedCount === 1
            path: root.singlePath
            isDirectory: root.singleIsDirectory
            useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
            iconSource: root.singlePath.length > 0
                        ? fileTypeIconResolver.iconForPathHint(root.singlePath, root.singleIsDirectory)
                        : ""
        }

        Label {
            Layout.fillWidth: true
            text: root.invertSelectionActive
                  ? ("Inverted selection - " + root.selectedCount + (root.selectedCount === 1 ? " item" : " items"))
                  : (root.selectedCount === 1 && root.singlePath.length > 0 && root.controller
                  ? root.controller.fileNameForPath(root.singlePath)
                  : (root.selectedCount + " items"))
            color: Theme.textPrimary
            font.pixelSize: 12
            font.weight: Font.DemiBold
            elide: Text.ElideMiddle
        }

        IconButton {
            iconSource: "../assets/lucide-toolbar/copy-to-panel.svg"
            iconTone: "copy"
            iconSize: 16
            enabled: root.canCopyOrMoveToOtherPanel
            opacity: enabled ? 1.0 : 0.4
            onClicked: root.copyToOtherPanelRequested()
            ToolTip.visible: hovered
            ToolTip.text: root.oppositePanelAvailable ? "Copy to other panel (F5)" : "Open split view to copy to other panel"
        }

        IconButton {
            iconSource: "../assets/lucide-toolbar/move-to-panel.svg"
            iconTone: "move"
            iconSize: 16
            enabled: root.canCopyOrMoveToOtherPanel
            opacity: enabled ? 1.0 : 0.4
            onClicked: root.moveRequested()
            ToolTip.visible: hovered
            ToolTip.text: root.oppositePanelAvailable ? "Move to other panel (Shift+F5)" : "Open split view to move to other panel"
        }

        IconButton {
            iconSource: "../assets/icons/rename.svg"
            iconTone: "rename"
            iconSize: 16
            enabled: root.hasSelection && root.controller && root.controller.canRenameSelection
            onClicked: root.renameRequested()
            ToolTip.visible: hovered
            ToolTip.text: root.selectedCount > 1 ? "Batch Rename" : "Rename"
        }

        IconButton {
            iconSource: "../assets/icons/delete.svg"
            iconTone: "delete"
            iconSize: 16
            enabled: root.hasSelection && !root.operationsBusy && root.controller && root.controller.canDeleteSelection
            onClicked: root.deleteRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Delete"
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.26)
        }

        IconButton {
            iconSource: "../assets/icons/star.svg"
            iconTone: "favorite"
            iconSize: 16
            enabled: root.canToggleFavorite
            isHighlighted: root.allSelectedPinned
            onClicked: root.pinToggleRequested(root.selectedPaths, root.allSelectedPinned)
            ToolTip.visible: hovered
            ToolTip.text: root.selectionContainsArchivePath
                          ? "Archive contents cannot be pinned"
                          : (root.allSelectedPinned ? "Unpin from Favorites" : "Pin to Favorites")
        }

        IconButton {
            iconSource: "../assets/icons/info.svg"
            iconTone: "info"
            iconSize: 16
            enabled: root.hasSelection
            onClicked: root.propertiesRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Properties"
        }

        IconButton {
            iconSource: "../assets/icons/clipboard-copy.svg"
            iconTone: "copy"
            iconSize: 16
            enabled: root.hasSelection && !root.operationsBusy
            onClicked: root.copyRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Copy to Clipboard"
        }

        IconButton {
            iconSource: "../assets/icons/invert-selection.svg"
            iconTone: "selection"
            iconSize: 16
            visible: root.canInvertSelection || root.invertSelectionActive
            enabled: root.canInvertSelection
            isHighlighted: root.invertSelectionActive
            onClicked: root.invertSelectionRequested()
            ToolTip.visible: hovered
            ToolTip.text: root.invertSelectionActive ? "Turn off inverted selection (Ctrl+I)" : "Invert Selection (Ctrl+I)"
        }

        IconButton {
            iconSource: "../assets/icons/select-all.svg"
            iconTone: "muted"
            iconSize: 16
            enabled: root.hasSelection && root.controller && root.controller.directoryModel
            onClicked: root.clearSelectionRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Clear Selection"
        }
    }
}
