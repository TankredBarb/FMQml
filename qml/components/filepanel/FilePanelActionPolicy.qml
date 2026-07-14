import QtQuick

QtObject {
    id: root

    property var controller: null
    property var workspaceController: null

    function directoryModel() {
        return root.controller ? root.controller.directoryModel : null
    }

    function selectedCount() {
        const model = directoryModel()
        return model ? model.selectedCount : 0
    }

    function hasSelection() {
        return selectedCount() > 0
    }

    function operationsBusy() {
        return Boolean(root.workspaceController
                       && root.workspaceController.operationQueue
                       && root.workspaceController.operationQueue.busy)
    }

    function operationAvailable() {
        return !operationsBusy()
    }

    function hasExplicitNonLocalScheme(path) {
        return Boolean(root.controller && root.controller.pathHasExplicitNonLocalScheme(path))
    }

    function isProviderPath(path) {
        return Boolean(root.controller && root.controller.pathIsProvider(path))
    }

    function currentPathIsProvider() {
        return root.controller && isProviderPath(root.controller.currentPath)
    }

    function oppositePathIsProvider() {
        const destination = oppositePanel()
        return destination && isProviderPath(destination.currentPath)
    }

    function canUseLocalShellAction(path) {
        return Boolean(root.controller
               && root.controller.pathCanUseLocalShellAction(path)
               && !(root.workspaceController && root.workspaceController.isInsideManagedIsoMount(path))
        )
    }

    function canOpenTerminal() {
        return root.controller
               && canUseLocalShellAction(root.controller.currentPath)
    }

    function canRevealPath(path) {
        return canUseLocalShellAction(path)
    }

    function canSetWallpaperPath(path, isDirectory) {
        if (isDirectory || !canUseLocalShellAction(path)) {
            return false
        }
        return root.controller.canSetWallpaperPath(path)
    }

    function canAnalyzePath(path, isDirectory) {
        return Boolean(isDirectory
                       && canUseLocalShellAction(path)
                       && typeof diskUsageController !== "undefined"
                       && diskUsageController)
    }

    function canShowPropertiesPath(path) {
        return Boolean(root.controller && root.controller.pathCanShowProperties(path))
    }

    function pathsCanShowProperties(paths) {
        return Boolean(root.controller && root.controller.pathsCanShowProperties(paths || []))
    }

    function canShowSelectedProperties() {
        return root.controller
               && hasSelection()
               && pathsCanShowProperties(root.controller.selectedPaths ? root.controller.selectedPaths() : [])
    }

    function canCreateManualItem() {
        return Boolean(root.controller
                       && !isProviderPath(root.controller.currentPath)
                       && root.controller.canCreateInCurrentPath)
    }

    function canMutateSelection() {
        return Boolean(root.controller
                       && hasSelection()
                       && operationAvailable()
                       && !currentPathIsProvider())
    }

    function canOfferMoveSelectionToOtherPanel() {
        return Boolean(root.controller
                       && oppositePanel()
                       && hasSelection()
                       && operationAvailable()
                       && !currentPathIsProvider()
                       && !oppositePathIsProvider())
    }

    function canRenameSelection() {
        return canMutateSelection()
               && root.controller.canRenameSelection
    }

    function canDeleteSelection() {
        return Boolean(root.controller
               && hasSelection()
               && operationAvailable()
               && root.controller.canDeleteSelection
        )
    }

    function canDuplicateSelection() {
        return canMutateSelection()
               && root.controller.canDuplicateSelection
    }

    function canCompressSelection() {
        return canMutateSelection()
               && root.controller.canCompressSelection
    }

    function pathCanBeFavorited(path) {
        return Boolean(root.controller && root.controller.pathCanBeFavorited(path))
    }

    function pathsCanBeFavorited(paths) {
        return Boolean(root.controller && root.controller.pathsCanBeFavorited(paths || []))
    }

    function oppositePanel() {
        if (!root.workspaceController || !root.workspaceController.splitEnabled || !root.controller) {
            return null
        }
        if (root.workspaceController.leftPanel === root.controller) {
            return root.workspaceController.rightPanel
        }
        if (root.workspaceController.rightPanel === root.controller) {
            return root.workspaceController.leftPanel
        }
        return root.workspaceController.activePanel === 0
               ? root.workspaceController.rightPanel
               : root.workspaceController.leftPanel
    }

    function canCopySelectionToOtherPanel() {
        const destination = oppositePanel()
        return Boolean(root.controller
                       && destination
                       && hasSelection()
                       && operationAvailable()
                       && !root.controller.isVirtualRoot
                       && root.controller.canCopySelection
                       && !destination.isVirtualRoot
                       && destination.canCreateInCurrentPath)
    }

    function canMoveSelectionToOtherPanel() {
        const destination = oppositePanel()
        return canOfferMoveSelectionToOtherPanel()
               && canCopySelectionToOtherPanel()
               && root.controller
               && destination
               && root.controller.canDeleteSelection
    }

    function canCopyToClipboard() {
        return Boolean(root.controller
                       && hasSelection()
                       && operationAvailable()
                       && root.controller.canCopySelection)
    }

    function canCutToClipboard() {
        return hasSelection()
               && operationAvailable()
               && root.controller
               && !currentPathIsProvider()
               && root.controller.canDeleteSelection
    }

    function canPasteFromClipboard() {
        return Boolean(root.workspaceController
                       && root.workspaceController.hasClipboard
                       && operationAvailable()
                       && root.controller
                       && root.controller.canPasteIntoCurrentPath)
    }
}
