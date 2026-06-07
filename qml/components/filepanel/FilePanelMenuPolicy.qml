import QtQuick

QtObject {
    id: root

    property var controller: null
    property var workspaceController: null
    property var favoritesController: null
    property var contextRowProvider: null
    property int contextRowValue: -1
    property string contextPathValue: ""
    property bool isCurrentPathReadOnlyContainer: false

    property FilePanelActionPolicy actionPolicy: FilePanelActionPolicy {
        controller: root.controller
        workspaceController: root.workspaceController
    }

    readonly property int favoritesPinnedCount: root.favoritesController ? root.favoritesController.pinnedCount : -1
    readonly property string contextArchiveFolderName: archiveFolderName(root.contextPathValue)
    readonly property string revealInOsLabel: Qt.platform.os === "windows" ? "Show in Explorer"
            : Qt.platform.os === "osx" ? "Reveal in Finder"
            : "Open Containing Folder"

    function contextRow() {
        return root.contextRowValue >= 0 ? root.contextRowValue
                                         : (root.contextRowProvider ? root.contextRowProvider() : -1)
    }

    function directoryModel() {
        return root.controller ? root.controller.directoryModel : null
    }

    function operationAvailable() {
        return Boolean(root.workspaceController
                       && root.workspaceController.operationQueue
                       && !root.workspaceController.operationQueue.busy)
    }

    function selectedCount() {
        const model = root.directoryModel()
        return model ? model.selectedCount : 0
    }

    function archiveFolderName(path) {
        if (!path || path.length === 0) {
            return ""
        }
        const normalized = String(path).replace(/\\/g, "/")
        const fileName = normalized.split("/").filter(part => part.length > 0).pop() || ""
        if (fileName.length === 0) {
            return ""
        }
        const dot = fileName.lastIndexOf(".")
        return dot > 0 ? fileName.substring(0, dot) : fileName
    }

    function localPathFromUrl(url) {
        let value = url ? url.toString() : ""
        if (value.startsWith("file:///")) {
            value = decodeURIComponent(value.substring(8))
            if (Qt.platform.os === "windows" && value.length >= 3 && value[1] === ":") {
                return value
            }
            return "/" + value
        }
        if (value.startsWith("file://")) {
            return decodeURIComponent(value.substring(7))
        }
        return decodeURIComponent(value)
    }

    function currentFolderUrl() {
        return root.controller && root.controller.currentPath.length > 0
               ? "file:///" + root.controller.currentPath.replace(/\\/g, "/")
               : "file:///"
    }

    function favoriteMenuPaths() {
        if (!root.controller) {
            return []
        }

        const selected = root.controller.selectedPaths ? root.controller.selectedPaths() : []
        if (selected && selected.length > 0
                && root.contextPathValue.length > 0
                && selected.indexOf(root.contextPathValue) >= 0) {
            return selected
        }
        return root.contextPathValue.length > 0 ? [root.contextPathValue] : []
    }

    function pathsCanBeFavorited(paths) {
        return actionPolicy.pathsCanBeFavorited(paths)
    }

    function favoriteMenuAvailable() {
        return Boolean(root.favoritesController
               && root.controller
               && !root.controller.isVirtualRoot
               && root.pathsCanBeFavorited(root.favoriteMenuPaths()))
    }

    function favoriteMenuAllPinned() {
        if (!root.favoritesController) {
            return false
        }

        const revision = root.favoritesPinnedCount
        const paths = favoriteMenuPaths()
        if (!paths || paths.length === 0) {
            return false
        }

        for (let i = 0; i < paths.length; ++i) {
            if (!root.favoritesController.isPinned(paths[i])) {
                return false
            }
        }
        if (revision < 0) {
            return false
        }
        return true
    }

    function currentFolderPinned() {
        const revision = root.favoritesPinnedCount
        if (revision < 0 || !root.favoritesController || !root.controller) {
            return false
        }
        return root.favoritesController.isPinned(root.controller.currentPath)
    }

    function canFavoriteCurrentFolder() {
        return Boolean(root.favoritesController
               && root.controller
               && root.controller.currentPath.length > 0
               && !root.controller.isVirtualRoot
               && actionPolicy.pathCanBeFavorited(root.controller.currentPath))
    }

    function canOpenContextItem() {
        return root.contextRow() >= 0
    }

    function canCutToClipboard() {
        return actionPolicy.canCutToClipboard()
    }

    function canCopyToClipboard() {
        return actionPolicy.canCopyToClipboard()
    }

    function canDuplicateSelection() {
        return actionPolicy.canDuplicateSelection()
    }

    function canCompressSelection() {
        return actionPolicy.canCompressSelection()
    }

    function canPasteFromClipboard() {
        return actionPolicy.canPasteFromClipboard()
    }

    function canCreateInCurrentPath() {
        return actionPolicy.canCreateManualItem()
    }

    function currentPathIsProvider() {
        return actionPolicy.currentPathIsProvider()
    }

    function canRenameSelection() {
        return root.contextRow() >= 0 && actionPolicy.canRenameSelection()
    }

    function canDeleteSelection() {
        return actionPolicy.canDeleteSelection()
    }

    function canAnalyzeContextFolder() {
        const row = root.contextRow()
        return row >= 0
            && root.contextPathValue.length > 0
            && root.controller
            && root.controller.directoryModel
            && actionPolicy.canAnalyzePath(root.contextPathValue, root.controller.directoryModel.isDirectoryAt(row))
    }

    function canAnalyzeCurrentFolder() {
        return Boolean(root.controller
                       && !root.controller.isVirtualRoot
                       && actionPolicy.canAnalyzePath(root.controller.currentPath, true))
    }

    function canCompareChecksums() {
        const model = root.directoryModel()
        if (!root.controller || !model) {
            return false
        }
        if (model.selectedCount !== 2) {
            return false
        }
        const paths = root.controller.selectedPaths()
        if (paths.length !== 2) {
            return false
        }
        const idx1 = model.indexOfPath(paths[0])
        const idx2 = model.indexOfPath(paths[1])
        return idx1 >= 0 && idx2 >= 0
            && !model.isDirectoryAt(idx1)
            && !model.isDirectoryAt(idx2)
            && actionPolicy.canUseLocalShellAction(paths[0])
            && actionPolicy.canUseLocalShellAction(paths[1])
    }

    function canOfferCompareChecksums() {
        const model = root.directoryModel()
        if (!root.controller || !model || model.selectedCount === 0) {
            return false
        }
        const paths = root.controller.selectedPaths()
        if (!paths || paths.length === 0) {
            return false
        }
        for (let i = 0; i < paths.length; ++i) {
            if (!actionPolicy.canUseLocalShellAction(paths[i])) {
                return false
            }
        }
        return true
    }

    function canOpenTerminal() {
        return actionPolicy.canOpenTerminal()
    }

    function canRevealContextItem() {
        return root.contextRow() >= 0
                && actionPolicy.canRevealPath(root.contextPathValue)
    }

    function canShowContextProperties() {
        return root.contextRow() >= 0
                && actionPolicy.canShowPropertiesPath(root.contextPathValue)
    }

    function canShowCurrentFolderProperties() {
        return Boolean(root.controller
                       && !root.controller.isVirtualRoot
                       && actionPolicy.canShowPropertiesPath(root.controller.currentPath))
    }

    function canExtractContextArchive() {
        return root.contextRow() >= 0
                && !actionPolicy.isProviderPath(root.controller ? root.controller.currentPath : "")
                && actionPolicy.canShowPropertiesPath(root.contextPathValue)
    }
}
