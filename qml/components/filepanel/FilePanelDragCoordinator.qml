import QtQuick

Item {
    id: root

    visible: false
    width: 0
    height: 0

    required property var workspaceController
    property bool renamingActive: false

    readonly property bool active: paths.length > 0
    property int sourcePanelSide: -1
    property int destinationPanelSide: -1
    property var sourceController: null
    property var destinationController: null
    property var paths: []
    property var dragItems: []
    readonly property int itemCount: paths.length
    property real pointerX: 0
    property real pointerY: 0
    property int pointerRevision: 0
    property string destinationPath: ""
    property bool canCopy: false
    property bool canMove: false
    property string reason: ""
    property string copyReason: ""
    property string moveReason: ""
    property string cancelReason: ""

    function panelController(panelSide) {
        if (!root.workspaceController) {
            return null
        }
        if (panelSide === 0) {
            return root.workspaceController.leftPanel
        }
        if (panelSide === 1) {
            return root.workspaceController.rightPanel
        }
        return null
    }

    function oppositePanelSide(panelSide) {
        return panelSide === 0 ? 1 : (panelSide === 1 ? 0 : -1)
    }

    function selectedPathsFor(panelSide) {
        const controller = root.panelController(panelSide)
        if (!controller || !controller.selectedPaths) {
            return []
        }
        const selected = controller.selectedPaths()
        return selected ? Array.from(selected) : []
    }

    function suffixForPath(path) {
        const value = String(path || "")
        const slash = Math.max(value.lastIndexOf("/"), value.lastIndexOf("\\"))
        const name = slash >= 0 ? value.slice(slash + 1) : value
        const dot = name.lastIndexOf(".")
        return dot > 0 && dot < name.length - 1 ? name.slice(dot + 1) : ""
    }

    function dragItemsFor(panelSide, paths) {
        const controller = root.panelController(panelSide)
        const model = controller ? controller.directoryModel : null
        const items = []
        for (let i = 0; i < paths.length; ++i) {
            const path = String(paths[i] || "")
            const row = model && model.indexOfPath ? model.indexOfPath(path) : -1
            items.push({
                path: path,
                isDirectory: row >= 0 && model && model.isDirectoryAt ? model.isDirectoryAt(row) : false,
                suffix: root.suffixForPath(path)
            })
        }
        return items
    }

    function canStartDrag(panelSide, path) {
        if (!root.workspaceController || !root.workspaceController.splitEnabled) {
            return false
        }
        if (root.workspaceController.operationQueue && root.workspaceController.operationQueue.busy) {
            return false
        }
        const controller = root.panelController(panelSide)
        if (!controller || controller.isVirtualRoot || root.renamingActive) {
            return false
        }
        const selected = root.selectedPathsFor(panelSide)
        if (selected.length === 0) {
            return false
        }
        return !path || selected.indexOf(path) >= 0
    }

    function startDrag(panelSide, path) {
        if (!root.canStartDrag(panelSide, path)) {
            return false
        }

        const destinationSide = root.oppositePanelSide(panelSide)
        const selected = root.selectedPathsFor(panelSide)
        const capabilities = root.workspaceController.oppositePanelDropCapabilities(
                    panelSide, selected, destinationSide)

        root.sourcePanelSide = panelSide
        root.destinationPanelSide = destinationSide
        root.sourceController = root.panelController(panelSide)
        root.destinationController = root.panelController(destinationSide)
        root.paths = selected
        root.dragItems = root.dragItemsFor(panelSide, selected)
        root.destinationPath = String(capabilities.destinationPath || "")
        root.canCopy = Boolean(capabilities.canCopy)
        root.canMove = Boolean(capabilities.canMove)
        root.reason = String(capabilities.reason || "")
        root.copyReason = String(capabilities.copyReason || "")
        root.moveReason = String(capabilities.moveReason || "")
        root.cancelReason = ""
        return true
    }

    function updateDragPosition(sourceItem, x, y) {
        if (!root.active || !sourceItem || !root.parent) {
            return
        }
        const point = root.parent.mapFromItem(sourceItem, x, y)
        root.pointerX = point.x
        root.pointerY = point.y
        root.pointerRevision += 1
    }

    function isOppositePanel(panelSide) {
        return root.active && panelSide === root.destinationPanelSide
    }

    function canDropOn(panelSide) {
        return root.isOppositePanel(panelSide) && (root.canCopy || root.canMove)
    }

    function deniedReasonFor(panelSide) {
        if (!root.active) {
            return ""
        }
        if (!root.isOppositePanel(panelSide)) {
            return "Drop target must be the opposite panel."
        }
        return root.reason || root.copyReason || root.moveReason || ""
    }

    function cancelDrag(reason) {
        root.cancelReason = reason || ""
        root.sourcePanelSide = -1
        root.destinationPanelSide = -1
        root.sourceController = null
        root.destinationController = null
        root.paths = []
        root.dragItems = []
        root.destinationPath = ""
        root.canCopy = false
        root.canMove = false
        root.reason = ""
        root.copyReason = ""
        root.moveReason = ""
    }

    onRenamingActiveChanged: {
        if (root.active && root.renamingActive) {
            root.cancelDrag("Rename started.")
        }
    }

    Connections {
        target: root.workspaceController
        function onSplitEnabledChanged() {
            if (root.active && !root.workspaceController.splitEnabled) {
                root.cancelDrag("Split view closed.")
            }
        }
    }

    Connections {
        target: root.workspaceController ? root.workspaceController.operationQueue : null
        function onBusyChanged() {
            if (root.active && root.workspaceController.operationQueue.busy) {
                root.cancelDrag("Another file operation started.")
            }
        }
    }

    Connections {
        target: root.workspaceController ? root.workspaceController.leftPanel : null
        function onCurrentPathChanged() {
            if (root.active) {
                root.cancelDrag("Panel path changed.")
            }
        }
    }

    Connections {
        target: root.workspaceController ? root.workspaceController.rightPanel : null
        function onCurrentPathChanged() {
            if (root.active) {
                root.cancelDrag("Panel path changed.")
            }
        }
    }
}
