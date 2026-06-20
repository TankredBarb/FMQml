import QtQuick
import QtQuick.Controls
import ".."
import "../../style"

Item {
    id: root

    property var workspaceController
    property var dragCoordinator
    property int sourcePanelSide: -1
    property int destinationPanelSide: -1
    property var paths: []
    property int itemCount: 0
    property string destinationPath: ""
    property bool canCopy: false
    property bool canMove: false
    property string copyReason: ""
    property string moveReason: ""
    property int activePanelAtPopup: -1
    readonly property bool menuOpen: dropMenu.opened || dropMenu.visible

    signal statusMessageRequested(string message)

    function itemLabel(action) {
        return action + " " + root.itemCount + " items"
    }

    function captureSnapshot() {
        if (!root.dragCoordinator || !root.dragCoordinator.active) {
            return false
        }
        root.sourcePanelSide = root.dragCoordinator.sourcePanelSide
        root.destinationPanelSide = root.dragCoordinator.destinationPanelSide
        root.paths = Array.from(root.dragCoordinator.paths)
        root.itemCount = root.dragCoordinator.itemCount
        root.destinationPath = root.dragCoordinator.destinationPath
        root.canCopy = root.dragCoordinator.canCopy
        root.canMove = root.dragCoordinator.canMove
        root.copyReason = root.dragCoordinator.copyReason
        root.moveReason = root.dragCoordinator.moveReason
        root.activePanelAtPopup = root.workspaceController ? root.workspaceController.activePanel : -1
        return root.paths.length > 0 && root.destinationPanelSide >= 0
    }

    function popupDropMenu(anchorItem, x, y) {
        if (!root.captureSnapshot()) {
            return false
        }
        dropMenu.popup(anchorItem, x, y)
        return true
    }

    function finishDrag(reason) {
        if (root.dragCoordinator && root.dragCoordinator.active) {
            root.dragCoordinator.cancelDrag(reason)
        }
        root.activePanelAtPopup = -1
    }

    function cancelDropMenu() {
        if (!root.menuOpen) {
            return false
        }
        root.finishDrag("Drop canceled.")
        dropMenu.close()
        return true
    }

    function executeCopy() {
        const ok = root.workspaceController
                && root.workspaceController.copyDroppedSelectionToPanel(
                    root.sourcePanelSide, root.paths,
                    root.destinationPanelSide, root.destinationPath)
        root.finishDrag(ok ? "Drop copy started." : "Drop copy rejected.")
        if (!ok) {
            root.statusMessageRequested(root.copyReason || "Copy operation was rejected.")
        }
    }

    function executeMove() {
        const ok = root.workspaceController
                && root.workspaceController.moveDroppedSelectionToPanel(
                    root.sourcePanelSide, root.paths,
                    root.destinationPanelSide, root.destinationPath)
        root.finishDrag(ok ? "Drop move started." : "Drop move rejected.")
        if (!ok) {
            root.statusMessageRequested(root.moveReason || "Move operation was rejected.")
        }
    }

    Connections {
        target: root.workspaceController

        function onActivePanelChanged() {
            if (dropMenu.visible && root.workspaceController
                    && root.activePanelAtPopup >= 0
                    && root.workspaceController.activePanel !== root.activePanelAtPopup) {
                dropMenu.close()
            }
        }
    }

    ThemedContextMenu {
        id: dropMenu

        onClosed: root.finishDrag("Drop menu closed.")

        ThemedMenuItem {
            text: root.itemLabel("Copy")
            icon.source: "../assets/icons/copy.svg"
            iconColor: Theme.actionIconColor("copy")
            enabled: root.canCopy
            onTriggered: root.executeCopy()
        }
        ThemedMenuItem {
            text: root.itemLabel("Move")
            icon.source: "../assets/icons/move.svg"
            iconColor: Theme.actionIconColor("move")
            enabled: root.canMove
            onTriggered: root.executeMove()
        }
        ThemedMenuItem {
            text: "Cancel operation"
            icon.source: "../assets/icons/exit.svg"
            iconColor: Theme.textSecondary
            onTriggered: root.cancelDropMenu()
        }
    }
}
