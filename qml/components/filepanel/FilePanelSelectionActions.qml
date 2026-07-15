import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../style"
import "../common"

AmbientPanelBackground {
    id: root

    property var controller
    property var workspaceController
    property var favoritesController
    property bool active: false
    property bool resizeOptimized: false
    property bool useNativeIcons: true
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
    readonly property bool canCopyToOtherPanel: actionPolicy.canCopySelectionToOtherPanel()
    readonly property bool canMoveToOtherPanel: actionPolicy.canMoveSelectionToOtherPanel()
    readonly property var selectedPaths: {
        root.selectionRevision
        return root.hasSelection && root.controller && root.controller.selectedPaths
            ? root.controller.selectedPaths()
            : []
    }
    readonly property bool canToggleFavorite: root.hasSelection
                                              && root.favoritesController
                                              && root.controller
                                              && !root.controller.isVirtualRoot
                                              && actionPolicy.pathsCanBeFavorited(root.selectedPaths)
    readonly property string singlePath: selectedPaths.length === 1 ? selectedPaths[0] : ""
    readonly property int singleIndex: root.singlePath.length > 0 && root.controller && root.controller.directoryModel
                                       ? root.controller.directoryModel.indexOfPath(root.singlePath)
                                       : -1
    readonly property bool singleIsDirectory: root.singleIndex >= 0 && root.controller && root.controller.directoryModel
                                              ? root.controller.directoryModel.isDirectoryAt(root.singleIndex)
                                              : false
    readonly property bool allSelectedPinned: {
        if (!root.favoritesController || selectedPaths.length === 0
                || !actionPolicy.pathsCanBeFavorited(root.selectedPaths)) {
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

    FilePanelActionPolicy {
        id: actionPolicy
        controller: root.controller
        workspaceController: root.workspaceController
    }

    implicitHeight: Math.max(44, Theme.controlHeight + 6)
    visible: root.visibleForSelection
    baseColor: Theme.panelSurfaceStrong
    strength: 0.28
    border.width: 0

    Connections {
        target: root.controller && root.controller.directoryModel ? root.controller.directoryModel : null
        function onSelectionChanged() {
            root.selectionRevision += 1
        }
        function onDataChanged(topLeft, bottomRight, roles) {
            if (root.singleIndex >= topLeft.row && root.singleIndex <= bottomRight.row) {
                root.selectionRevision += 1
            }
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
            useNativeIcons: root.useNativeIcons
            suffix: {
                root.selectionRevision
                if (root.singleIndex < 0 || !root.controller || !root.controller.directoryModel) return ""
                const model = root.controller.directoryModel
                return model.data(model.index(root.singleIndex, 0), Qt.UserRole + 12) || ""
            }
            mimeType: {
                root.selectionRevision
                if (root.singleIndex < 0 || !root.controller || !root.controller.directoryModel) return ""
                const model = root.controller.directoryModel
                return model.data(model.index(root.singleIndex, 0), Qt.UserRole + 20) || ""
            }
            iconName: {
                root.selectionRevision
                if (root.singleIndex < 0 || !root.controller || !root.controller.directoryModel) return ""
                const model = root.controller.directoryModel
                return model.data(model.index(root.singleIndex, 0), Qt.UserRole + 11) || ""
            }
            name: {
                root.selectionRevision
                if (root.singleIndex < 0 || !root.controller || !root.controller.directoryModel) return ""
                const model = root.controller.directoryModel
                return model.data(model.index(root.singleIndex, 0), Qt.UserRole + 1) || ""
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.invertSelectionActive
                  ? ("Inverted selection - " + root.selectedCount + (root.selectedCount === 1 ? " item" : " items"))
                  : (root.selectedCount === 1 && root.singlePath.length > 0 && root.controller
                  ? root.controller.fileNameForPath(root.singlePath)
                  : (root.selectedCount + " items"))
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.DemiBold
            elide: Text.ElideMiddle
        }

        IconButton {
            iconSource: "../assets/toolbar-next/copy-to-panel.svg"
            iconTone: "copy"
            iconSize: 16
            enabled: root.canCopyToOtherPanel
            opacity: enabled ? 1.0 : 0.4
            onClicked: root.copyToOtherPanelRequested()
            ToolTip.visible: hovered
            ToolTip.text: root.oppositePanelAvailable ? "Copy to other panel (F5)" : "Open split view to copy to other panel"
        }

        IconButton {
            iconSource: "../assets/toolbar-next/move-to-panel.svg"
            iconTone: "move"
            iconSize: 16
            visible: !actionPolicy.currentPathIsProvider() && !actionPolicy.oppositePathIsProvider()
            enabled: root.canMoveToOtherPanel
            opacity: enabled ? 1.0 : 0.4
            onClicked: root.moveRequested()
            ToolTip.visible: hovered
            ToolTip.text: root.oppositePanelAvailable
                          ? (root.controller && !root.controller.canDeleteSelection ? "Cannot move from this provider" : "Move to other panel (Shift+F5)")
                          : "Open split view to move to other panel"
        }

        IconButton {
            iconSource: "../assets/icons/rename.svg"
            iconTone: "rename"
            iconSize: 16
            visible: !actionPolicy.currentPathIsProvider()
            enabled: actionPolicy.canRenameSelection()
            onClicked: root.renameRequested()
            ToolTip.visible: hovered
            ToolTip.text: root.selectedCount > 1 ? "Batch Rename" : "Rename"
        }

        IconButton {
            iconSource: "../assets/icons/delete.svg"
            iconTone: "delete"
            iconSize: 16
            visible: actionPolicy.canDeleteSelection()
            enabled: actionPolicy.canDeleteSelection()
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
            ToolTip.text: !actionPolicy.pathsCanBeFavorited(root.selectedPaths)
                          ? "This location cannot be pinned"
                          : (root.allSelectedPinned ? "Unpin from Favorites" : "Pin to Favorites")
        }

        IconButton {
            iconSource: "../assets/icons/info.svg"
            iconTone: "info"
            iconSize: 16
            visible: actionPolicy.canShowSelectedProperties()
            enabled: visible
            onClicked: root.propertiesRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Properties"
        }

        IconButton {
            iconSource: "../assets/icons/clipboard-copy.svg"
            iconTone: "copy"
            iconSize: 16
            enabled: actionPolicy.canCopyToClipboard()
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
