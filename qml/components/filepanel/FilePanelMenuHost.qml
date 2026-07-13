import QtQuick
import QtQuick.Controls

Item {
    id: host

    required property var panelRoot
    readonly property var contextMenu: filePanelContextMenuLoader.item
    readonly property var emptyMenu: filePanelEmptyMenuLoader.item
    readonly property var oppositeDropMenu: oppositePanelDropMenuLoader.item
    function createContextMenu(parent) { return filePanelContextMenuComponent.createObject(parent) }
    function createEmptyMenu(parent) { return filePanelEmptyMenuComponent.createObject(parent) }

Component {
    id: filePanelContextMenuComponent
    FilePanelContextMenu {
        controller: host.panelRoot.controller
        workspaceController: host.panelRoot.workspaceController
        favoritesController: host.panelRoot.favoritesBackend
        windowObject: host.panelRoot.Window.window
        contextRowProvider: host.panelRoot.contextRow
        isCurrentPathArchive: host.panelRoot.isCurrentPathArchive
        isCurrentPathReadOnlyContainer: host.panelRoot.isCurrentPathReadOnlyContainer
        onRenameRequested: host.panelRoot.startRename()
        onMenuOpenChanged: (open) => host.panelRoot.contextMenuOpen = open
    }
}

Loader {
    id: filePanelContextMenuLoader
    active: !host.panelRoot.startupLazyPanelMenus
    sourceComponent: filePanelContextMenuComponent
}

Component {
    id: filePanelEmptyMenuComponent
    FilePanelEmptyMenu {
        controller: host.panelRoot.controller
        workspaceController: host.panelRoot.workspaceController
        propertiesController: host.panelRoot.propertiesController
        favoritesController: host.panelRoot.favoritesBackend
        windowObject: host.panelRoot.Window.window
        onMenuOpenChanged: (open) => host.panelRoot.contextMenuOpen = open
        isCurrentPathArchive: host.panelRoot.isCurrentPathArchive
        isCurrentPathReadOnlyContainer: host.panelRoot.isCurrentPathReadOnlyContainer
        onSelectAllRequested: host.panelRoot.selectAll()
    }
}

Loader {
    id: filePanelEmptyMenuLoader
    active: !host.panelRoot.startupLazyPanelMenus
    sourceComponent: filePanelEmptyMenuComponent
}

Loader {
    id: oppositePanelDropMenuLoader
    active: host.panelRoot.internalDragEnabled
    sourceComponent: OppositePanelDropMenu {
        workspaceController: host.panelRoot.workspaceController
        dragCoordinator: host.panelRoot.dragCoordinator
        onStatusMessageRequested: (message) => host.panelRoot.showStatusMessage(message)
    }
}

FilePanelDropOverlay {
    anchors.fill: parent
    workspaceController: host.panelRoot.workspaceController
    panelSide: host.panelRoot.panelSide
    currentPath: host.panelRoot.controller.currentPath
    externalDropSuppressed: host.panelRoot.internalDragEnabled && host.panelRoot.dragCoordinator && host.panelRoot.dragCoordinator.active
    onStatusMessageRequested: (message) => host.panelRoot.showStatusMessage(message)
}

}
