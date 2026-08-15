import QtQuick

Item {
    id: host

    required property var panelRoot
    readonly property bool folderHoverTarget: panelRoot.controller
                                               && panelRoot.controller.hoveredFileInfo
                                               && Number(panelRoot.controller.hoveredFileInfo.specialAction || 0) === 0
                                               && panelRoot.controller.hoveredFileInfo.isDirectory === true
    readonly property var hoverPreview: folderHoverTarget ? folderHoverPreviewCard : hoverPreviewCard
    property bool lastAdminModeActive: typeof adminController !== "undefined"
                                       && adminController
                                       && adminController.adminModeActive

    function syncAdminModeState() {
        const active = typeof adminController !== "undefined"
                       && adminController
                       && adminController.adminModeActive
        if (active === lastAdminModeActive) return
        lastAdminModeActive = active
        folderHoverPreviewCard.refreshForAdminModeChange()
        if (panelRoot.controller && panelRoot.controller.folderPeekController) {
            panelRoot.controller.folderPeekController.handleAdminModeChanged(active)
        }
    }

    Connections {
        target: typeof adminController !== "undefined" ? adminController : null
        function onAdminModeStateChanged() { host.syncAdminModeState() }
    }

FileHoverPreviewCard {
    id: hoverPreviewCard
    z: 17
    path: host.panelRoot.controller ? host.panelRoot.controller.hoveredPath : ""
    info: host.panelRoot.controller ? host.panelRoot.controller.hoveredFileInfo : ({})
    backdropSource: host.panelRoot.hoverPreviewBackdropSource
    controller: host.panelRoot.controller
    anchorRect: host.panelRoot.hoverPreviewAnchorRect
    boundaryTopInset: 0
    boundaryBottomInset: host.panelRoot.bottomChromeHeight
    onQuickLookRequested: (path) => host.panelRoot.openHoverPreviewQuickLook(path)
    onOpenRequested: (path) => host.panelRoot.openHoverPreviewPath(path)
    onPropertiesRequested: (path) => host.panelRoot.openHoverPreviewProperties(path)
    onWallpaperRequested: (path) => host.panelRoot.setHoverPreviewWallpaper(path)
    requested: host.panelRoot.showMediaHoverPreviews
               && !host.folderHoverTarget
               && host.panelRoot.effectiveShowThumbnails
               && !host.panelRoot.virtualRootMode
               && host.panelRoot.controller
               && String(host.panelRoot.controller.hoveredPath).length > 0
    suppressed: host.panelRoot.hoverSuppressed || host.panelRoot.applicationOverlayOpen
                || host.panelRoot.contextMenuOpen
                || host.panelRoot.rubberBandPressed
                || host.panelRoot.rubberBandActive
                || host.panelRoot.isRenaming
                || (host.panelRoot.controller && host.panelRoot.controller.directoryModel && host.panelRoot.controller.directoryModel.loading)
                || (host.panelRoot.dragCoordinator && host.panelRoot.dragCoordinator.active)
}

FolderHoverPreviewCard {
    id: folderHoverPreviewCard
    z: 17
    path: host.panelRoot.controller ? host.panelRoot.controller.hoveredPath : ""
    info: host.panelRoot.controller ? host.panelRoot.controller.hoveredFileInfo : ({})
    previewController: host.panelRoot.controller ? host.panelRoot.controller.folderPreviewController : null
    backdropSource: host.panelRoot.hoverPreviewBackdropSource
    anchorRect: host.panelRoot.hoverPreviewAnchorRect
    boundaryBottomInset: host.panelRoot.bottomChromeHeight
    peekEnabled: host.panelRoot.folderPeekEnabled
    panel: host.panelRoot
    viewMode: host.panelRoot.folderHoverViewMode
    requested: host.panelRoot.showFolderHoverPreviews
               && host.folderHoverTarget
               && !host.panelRoot.virtualRootMode
               && String(path).length > 0
    suppressed: host.panelRoot.hoverSuppressed || host.panelRoot.applicationOverlayOpen
                || host.panelRoot.contextMenuOpen
                || host.panelRoot.rubberBandPressed
                || host.panelRoot.rubberBandActive
                || host.panelRoot.isRenaming
                || (host.panelRoot.controller && host.panelRoot.controller.directoryModel && host.panelRoot.controller.directoryModel.loading)
                || (host.panelRoot.dragCoordinator && host.panelRoot.dragCoordinator.active)
    onOpenRequested: (path) => host.panelRoot.openHoverPreviewPath(path)
    onPeekRequested: (path) => {
        const showHidden = host.panelRoot.controller.directoryModel.showHidden
        host.panelRoot.controller.hoveredPath = ""
        host.panelRoot.controller.folderPeekController.openPath(path, showHidden)
    }
    onViewModeRequested: (mode) => host.panelRoot.folderHoverViewMode = mode
}

FolderPeekOverlay {
    id: folderPeekOverlay
    z: 19
    controller: host.panelRoot.controller.folderPeekController
    backdropSource: host.panelRoot.hoverPreviewBackdropSource
    viewMode: host.panelRoot.folderPeekViewMode
    panel: host.panelRoot
    onViewModeRequested: (mode) => host.panelRoot.folderPeekViewMode = mode
}

Connections {
    target: host.panelRoot.controller.folderPeekController
    function onOpenChanged() {
        if (!host.panelRoot.controller.folderPeekController.open) {
            Qt.callLater(host.panelRoot.focusContent)
        }
    }
}

Loader {
    anchors.fill: parent
    z: 18
    active: host.panelRoot.internalDragEnabled
            && host.panelRoot.dragCoordinator
            && host.panelRoot.dragCoordinator.active
            && host.panelRoot.dragCoordinator.isOppositePanel(host.panelRoot.panelSide)
    sourceComponent: FilePanelInternalDropOverlay {
        dropAllowed: host.panelRoot.dragCoordinator
                     && host.panelRoot.dragCoordinator.canDropOn(host.panelRoot.panelSide)
    }
}

}
