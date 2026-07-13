import QtQuick

Item {
    id: host

    required property var panelRoot
    readonly property var hoverPreview: hoverPreviewCard

FileHoverPreviewCard {
    id: hoverPreviewCard
    z: 17
    path: host.panelRoot.controller ? host.panelRoot.controller.hoveredPath : ""
    info: host.panelRoot.controller ? host.panelRoot.controller.hoveredFileInfo : ({})
    controller: host.panelRoot.controller
    anchorRect: host.panelRoot.hoverPreviewAnchorRect
    boundaryTopInset: 0
    boundaryBottomInset: host.panelRoot.bottomChromeHeight
    onQuickLookRequested: (path) => host.panelRoot.openHoverPreviewQuickLook(path)
    onOpenRequested: (path) => host.panelRoot.openHoverPreviewPath(path)
    onPropertiesRequested: (path) => host.panelRoot.openHoverPreviewProperties(path)
    onWallpaperRequested: (path) => host.panelRoot.setHoverPreviewWallpaper(path)
    requested: host.panelRoot.showHoverPreviews
               && host.panelRoot.effectiveShowThumbnails
               && !host.panelRoot.virtualRootMode
               && host.panelRoot.controller
               && String(host.panelRoot.controller.hoveredPath).length > 0
    suppressed: host.panelRoot.hoverSuppressed
                || host.panelRoot.contextMenuOpen
                || host.panelRoot.rubberBandPressed
                || host.panelRoot.rubberBandActive
                || host.panelRoot.isRenaming
                || (host.panelRoot.controller && host.panelRoot.controller.directoryModel && host.panelRoot.controller.directoryModel.loading)
                || (host.panelRoot.dragCoordinator && host.panelRoot.dragCoordinator.active)
}

}
