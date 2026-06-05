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
        if (!paths || paths.length === 0) {
            return false
        }
        for (let i = 0; i < paths.length; ++i) {
            if (String(paths[i]).toLowerCase().startsWith("archive://")) {
                return false
            }
        }
        return true
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
               && !String(root.controller.currentPath).toLowerCase().startsWith("archive://"))
    }

    function canOpenContextItem() {
        return root.contextRow() >= 0
    }

    function canCutToClipboard() {
        return Boolean(root.selectedCount() > 0
                       && root.operationAvailable()
                       && !root.isCurrentPathReadOnlyContainer)
    }

    function canCopyToClipboard() {
        return Boolean(root.selectedCount() > 0 && root.operationAvailable())
    }

    function canDuplicateSelection() {
        return Boolean(root.selectedCount() > 0
                       && root.operationAvailable()
                       && root.controller
                       && root.controller.canDuplicateSelection)
    }

    function canCompressSelection() {
        return Boolean(root.selectedCount() > 0
                       && root.operationAvailable()
                       && root.controller
                       && root.controller.canCompressSelection)
    }

    function canPasteFromClipboard() {
        return Boolean(root.workspaceController
                       && root.workspaceController.operationQueue
                       && root.workspaceController.hasClipboard
                       && !root.workspaceController.operationQueue.busy
                       && root.controller
                       && root.controller.canPasteIntoCurrentPath)
    }

    function canCreateInCurrentPath() {
        return root.controller && root.controller.canCreateInCurrentPath
    }

    function canRenameSelection() {
        return root.contextRow() >= 0
                && root.controller
                && root.controller.canRenameSelection
    }

    function canDeleteSelection() {
        return Boolean(root.selectedCount() > 0
                       && root.operationAvailable()
                       && root.controller
                       && root.controller.canDeleteSelection)
    }

    function canAnalyzeContextFolder() {
        const row = root.contextRow()
        return row >= 0
            && root.contextPathValue.length > 0
            && root.controller
            && root.controller.directoryModel
            && root.controller.directoryModel.isDirectoryAt(row)
            && !root.contextPathValue.toLowerCase().startsWith("archive://")
            && typeof diskUsageController !== "undefined"
            && diskUsageController
    }

    function canAnalyzeCurrentFolder() {
        return Boolean(root.controller
                       && root.controller.currentPath.length > 0
                       && !root.controller.isVirtualRoot
                       && !root.controller.currentPath.toLowerCase().startsWith("archive://")
                       && typeof diskUsageController !== "undefined"
                       && diskUsageController)
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
    }

    function canOpenTerminal() {
        return root.controller && root.controller.currentPath.length > 0
    }
}
