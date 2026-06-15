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

    function explicitScheme(path) {
        const value = String(path || "").trim()
        const index = value.indexOf("://")
        if (index <= 0) {
            return ""
        }
        const scheme = value.substring(0, index).toLowerCase()
        if (scheme.length === 0 || !/[a-z]/.test(scheme.charAt(0))) {
            return ""
        }
        for (let i = 0; i < scheme.length; ++i) {
            const ch = scheme.charAt(i)
            if (!/[a-z0-9+.-]/.test(ch)) {
                return ""
            }
        }
        return scheme
    }

    function hasExplicitNonLocalScheme(path) {
        const scheme = explicitScheme(path)
        return scheme.length > 0 && scheme !== "file"
    }

    function isArchivePath(path) {
        return String(path || "").toLowerCase().startsWith("archive://")
    }

    function isProviderPath(path) {
        const scheme = explicitScheme(path)
        return scheme.length > 0
               && scheme !== "file"
               && scheme !== "archive"
               && scheme !== "devices"
               && scheme !== "favorites"
    }

    function currentPathIsProvider() {
        return root.controller && isProviderPath(root.controller.currentPath)
    }

    function oppositePathIsProvider() {
        const destination = oppositePanel()
        return destination && isProviderPath(destination.currentPath)
    }

    function canUseLocalShellAction(path) {
        const value = String(path || "")
        return value.length > 0
               && !hasExplicitNonLocalScheme(value)
               && !(root.workspaceController && root.workspaceController.isInsideManagedIsoMount(value))
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
        const suffix = String(path || "").split(".").pop().toLowerCase()
        return suffix === "png"
                || suffix === "jpg"
                || suffix === "jpeg"
                || suffix === "bmp"
    }

    function canAnalyzePath(path, isDirectory) {
        return Boolean(isDirectory
                       && canUseLocalShellAction(path)
                       && typeof diskUsageController !== "undefined"
                       && diskUsageController)
    }

    function canShowPropertiesPath(path) {
        const value = String(path || "")
        const lower = value.toLowerCase()
        return value.length > 0
               && !hasExplicitNonLocalScheme(value)
               && lower !== "devices://"
               && lower !== "favorites://"
    }

    function pathsCanShowProperties(paths) {
        if (!paths || paths.length === 0) {
            return false
        }
        for (let i = 0; i < paths.length; ++i) {
            if (!canShowPropertiesPath(paths[i])) {
                return false
            }
        }
        return true
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
        const value = String(path || "")
        return value.length > 0
               && !hasExplicitNonLocalScheme(value)
               && !isArchivePath(value)
               && value.toLowerCase() !== "devices://"
               && value.toLowerCase() !== "favorites://"
    }

    function pathsCanBeFavorited(paths) {
        if (!paths || paths.length === 0) {
            return false
        }
        for (let i = 0; i < paths.length; ++i) {
            if (!pathCanBeFavorited(paths[i])) {
                return false
            }
        }
        return true
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
