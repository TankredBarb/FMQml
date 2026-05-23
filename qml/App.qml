import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "components"
import "style"

ApplicationWindow {
    id: root

    width: 1120
    height: 720
    minimumWidth: 760
    minimumHeight: 480
    visible: false
    title: "FM"
    color: Theme.bg

    function openDeleteConfirm(paths, label) {
        const list = paths ? Array.from(paths) : []
        if (list.length === 0) {
            return
        }
        deleteConfirmDialog.openFor(list, label || "")
    }

    function activePanelController() {
        return workspaceController.activePanel === 0
            ? workspaceController.leftPanel
            : workspaceController.rightPanel
    }

    function activePanelScrolling() {
        const controller = activePanelController()
        return controller ? controller.scrolling : false
    }

    property bool previewOnHover: false
    property bool previewPaneVisible: false

    function previewTargetFor(controller) {
        if (!controller) {
            return ""
        }

        if (root.previewOnHover && controller.hoveredPath && controller.hoveredPath.length > 0) {
            return controller.hoveredPath
        }

        const selected = controller.selectedPaths()
        if (selected.length > 0) {
            return selected[0]
        }

        return controller.currentPath || ""
    }

    property string pendingPreviewPath: ""
    property int hoverPreviewDelayMs: 250

    Timer {
        id: previewSyncTimer
        interval: 250
        repeat: false
        onTriggered: {
            if (!root.previewPaneVisible) {
                return
            }
            if (activePanelScrolling()) {
                previewSyncTimer.restart()
                return
            }
            quickLookController.preview(root.pendingPreviewPath)
        }
    }

    Timer {
        id: hoverPreviewTimer
        interval: root.hoverPreviewDelayMs
        repeat: false
        onTriggered: {
            if (!root.previewPaneVisible) {
                return
            }
            if (activePanelScrolling()) {
                hoverPreviewTimer.restart()
                return
            }
            quickLookController.preview(root.pendingPreviewPath)
        }
    }

    function syncPreviewFromActivePanel(immediate) {
        if (!root.previewPaneVisible) {
            return
        }
        const controller = activePanelController()
        const targetPath = previewTargetFor(controller)
        if (immediate !== true && activePanelScrolling()) {
            pendingPreviewPath = targetPath
            return
        }
        if (immediate === true) {
            previewSyncTimer.stop()
            hoverPreviewTimer.stop()
            pendingPreviewPath = targetPath
            quickLookController.preview(targetPath)
            return
        }

        pendingPreviewPath = targetPath
        previewSyncTimer.restart()
    }

    function scheduleHoverPreview(path) {
        if (!root.previewPaneVisible) {
            return
        }

        if (activePanelScrolling()) {
            pendingPreviewPath = path
            return
        }

        if (hoverPreviewDelayMs <= 0) {
            hoverPreviewTimer.stop()
            return
        }

        pendingPreviewPath = path
        hoverPreviewTimer.restart()
    }

    Shortcut {
        sequence: "F1"
        onActivated: helpDialog.open()
    }

    Shortcut {
        sequence: "F3"
        onActivated: workspaceController.toggleSplit()
    }

    Shortcut {
        sequence: "F2"
        enabled: {
            let activeCtrl = workspaceController.activePanel === 0 
                             ? workspaceController.leftPanel 
                             : workspaceController.rightPanel
            let isArchive = activeCtrl.currentPath ? activeCtrl.currentPath.toLowerCase().startsWith("archive://") : false
            return !isArchive
        }
        onActivated: {
            workspaceController.triggerRename()
        }
    }

    Shortcut {
        sequence: "Space"
        enabled: !mainToolbar.textEditingActive || propertiesDialog.opened
        onActivated: {
            if (propertiesDialog.opened) {
                propertiesDialog.close()
                return
            }
            
            // If we're editing text, don't trigger preview (redundant with 'enabled' but good for clarity)
            if (mainToolbar.textEditingActive) return

            const controller = activePanelController()
            const targetPath = previewTargetFor(controller)
            if (targetPath.length > 0) {
                const row = controller.directoryModel.indexOfPath(targetPath)
                const isDir = row >= 0 ? controller.directoryModel.isDirectoryAt(row) : false
                
                if (isDir) {
                    propertiesController.load(targetPath)
                } else {
                    const previewableTypes = ["text", "image", "pdf", "svg", "font", "audio", "video", "executable"]
                    if (root.previewPaneVisible) {
                        if (previewableTypes.includes(quickLookController.type)) {
                            quickLookPopup.previewPath = targetPath
                            quickLookPopup.open()
                        } else {
                            propertiesController.load(targetPath)
                        }
                    } else {
                        quickLookController.preview(targetPath)
                        if (previewableTypes.includes(quickLookController.type)) {
                            quickLookPopup.previewPath = targetPath
                            quickLookPopup.open()
                        } else {
                            quickLookController.preview("")
                            propertiesController.load(targetPath)
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Delete"
        enabled: {
            if (mainToolbar.textEditingActive
                || propertiesDialog.opened
                || conflictDialog.opened
                || deleteConfirmDialog.opened
                || workspaceController.operationQueue.busy) {
                return false
            }
            let activeCtrl = workspaceController.activePanel === 0 
                             ? workspaceController.leftPanel 
                             : workspaceController.rightPanel
            let isArchive = activeCtrl.currentPath ? activeCtrl.currentPath.toLowerCase().startsWith("archive://") : false
            if (isArchive) {
                return false
            }
            return activeCtrl.directoryModel.selectedCount > 0
        }
        onActivated: {
            const active = workspaceController.activePanel === 0
                           ? workspaceController.leftPanel
                           : workspaceController.rightPanel
            workspaceController.requestDelete(active.selectedPaths(), active.currentPath)
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: !mainToolbar.textEditingActive
                 && !propertiesDialog.opened
                 && !conflictDialog.opened
                 && !fileWorkspace.isRenaming
                 && ((workspaceController.activePanel === 0
                      && workspaceController.leftPanel.directoryModel.selectedCount > 0)
                     || (workspaceController.activePanel === 1
                         && workspaceController.rightPanel.directoryModel.selectedCount > 0))
        onActivated: {
            const active = workspaceController.activePanel === 0
                           ? workspaceController.leftPanel
                           : workspaceController.rightPanel
            active.directoryModel.clearSelection()
            workspaceController.focusActivePanel()
        }
    }

    Shortcut {
        sequence: "Tab"
        onActivated: {
            if (workspaceController.splitEnabled) {
                workspaceController.activePanel = workspaceController.activePanel === 0 ? 1 : 0
            }
        }
    }

    Shortcut {
        sequence: "Alt+Left"
        onActivated: workspaceController.activePanel === 0
                     ? workspaceController.leftPanel.goBack()
                     : workspaceController.rightPanel.goBack()
    }

    Shortcut {
        sequence: "Alt+Right"
        onActivated: workspaceController.activePanel === 0
                     ? workspaceController.leftPanel.goForward()
                     : workspaceController.rightPanel.goForward()
    }

    Shortcut {
        sequence: "Alt+Up"
        onActivated: workspaceController.activePanel === 0
                     ? workspaceController.leftPanel.goUp()
                     : workspaceController.rightPanel.goUp()
    }

    Shortcut {
        sequence: "Ctrl+L"
        onActivated: mainToolbar.focusPath()
    }

    Shortcut {
        sequence: "Ctrl+C"
        onActivated: workspaceController.copyToClipboard()
    }

    Shortcut {
        sequence: "Ctrl+X"
        onActivated: workspaceController.cutToClipboard()
    }

    Shortcut {
        sequence: "Ctrl+V"
        onActivated: workspaceController.pasteFromClipboard()
    }

    Shortcut {
        sequence: "Ctrl+Z"
        onActivated: workspaceController.undo()
    }

    Shortcut {
        sequence: "Ctrl+Y"
        onActivated: workspaceController.redo()
    }

    Shortcut {
        sequence: "F5"
        onActivated: {
            const ctrl = activePanelController()
            if (workspaceController.splitEnabled && ctrl && ctrl.directoryModel.selectedCount > 0 && !workspaceController.operationQueue.busy) {
                workspaceController.copyActiveSelectionToOpposite()
            } else if (ctrl) {
                ctrl.refresh()
            }
        }
    }

    Shortcut {
        sequence: "F6"
        enabled: workspaceController.splitEnabled 
                 && activePanelController() 
                 && activePanelController().directoryModel.selectedCount > 0
                 && !workspaceController.operationQueue.busy
        onActivated: {
            workspaceController.moveActiveSelectionToOpposite()
        }
    }

    Shortcut {
        sequence: "Ctrl+R"
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.refresh()
        }
    }

    Shortcut {
        sequence: "Ctrl+H"
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) {
                const newValue = !ctrl.directoryModel.showHidden
                ctrl.directoryModel.showHidden = newValue
                workspaceController.treeModel.showHidden = newValue
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+1"
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.viewMode = 0
        }
    }

    Shortcut {
        sequence: "Ctrl+2"
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.viewMode = 1
        }
    }

    Shortcut {
        sequence: "Ctrl+3"
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.viewMode = 2
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+N"
        enabled: {
            const ctrl = activePanelController()
            return ctrl && ctrl.currentPath ? !ctrl.currentPath.toLowerCase().startsWith("archive://") : true
        }
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.createFolder("New Folder")
        }
    }

    Shortcut {
        sequence: "F7"
        enabled: {
            const ctrl = activePanelController()
            return ctrl && ctrl.currentPath ? !ctrl.currentPath.toLowerCase().startsWith("archive://") : true
        }
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.createFolder("New Folder")
        }
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: mainToolbar.focusSearch()
    }

    Shortcut {
        sequence: "Ctrl+P"
        onActivated: {
            const visible = !root.previewPaneVisible
            root.previewPaneVisible = visible
            quickLookController.visible = visible
            if (visible) {
                syncPreviewFromActivePanel(true)
            } else {
                quickLookController.preview("")
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+A"
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl && ctrl.directoryModel) {
                ctrl.directoryModel.selectAll()
            }
        }
    }

    Shortcut {
        sequence: "Alt+D"
        onActivated: mainToolbar.focusPath()
    }


    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        focus: true

        Keys.onPressed: (event) => {
            if (event.text.length > 0 && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.ShiftModifier)) {
                // Ignore Space, Enter/Return as they are handled by shortcuts or specific components
                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                    return;

                // Check if we're already in a text field or if a modal is open
                if (!conflictDialog.opened && !mainToolbar.activeFocus) {
                     mainToolbar.focusSearch()
                }
            }
        }

        MainToolbar {
            id: mainToolbar
            Layout.fillWidth: true
            previewVisible: root.previewPaneVisible
            onPreviewToggleRequested: (visible) => {
                root.previewPaneVisible = visible
                quickLookController.visible = visible
                if (visible) {
                    syncPreviewFromActivePanel(true)
                }
                else {
                    quickLookController.preview("")
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Sidebar {
                SplitView.preferredWidth: 200
                SplitView.minimumWidth: 140
                SplitView.maximumWidth: 300
            }

            FileWorkspace {
                id: fileWorkspace
                SplitView.fillWidth: true
            }

            PreviewPane {
                SplitView.preferredWidth: root.previewPaneVisible ? 340 : 0
                SplitView.minimumWidth: root.previewPaneVisible ? 280 : 0
                SplitView.fillWidth: false
                visible: root.previewPaneVisible || width > 0
                opacity: root.previewPaneVisible ? 1.0 : 0.0

                Behavior on opacity { NumberAnimation { duration: Theme.motionNormal } }
            }

            handle: Rectangle {
                implicitWidth: 1
                color: Theme.border
            }
        }
    }

    ConflictDialog {
        id: conflictDialog
    }

    HelpDialog {
        id: helpDialog
    }

    PropertiesDialog {
        id: propertiesDialog
    }

    QuickLook {
        id: quickLookPopup
    }

    DeleteConfirmDialog {
        id: deleteConfirmDialog
    }

    Connections {
        target: workspaceController.operationQueue
        function onConflictDetected(source, destination, sourceSize, sourceModified, destSize, destModified) {
            conflictDialog.sourcePath = source
            conflictDialog.destinationPath = destination
            conflictDialog.sourceSize = sourceSize
            conflictDialog.sourceModified = sourceModified
            conflictDialog.destSize = destSize
            conflictDialog.destModified = destModified
            conflictDialog.open()
        }
    }

    Connections {
        target: workspaceController
        function onDeleteRequested(paths, label) {
            openDeleteConfirm(paths, label)
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onRevealProperties(paths) {
            propertiesController.loadMultiple(paths)
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onRevealProperties(paths) {
            propertiesController.loadMultiple(paths)
        }
    }

    Connections {
        target: workspaceController
        function onActivePanelChanged() {
            if (root.previewPaneVisible) {
                syncPreviewFromActivePanel(true)
            }
            workspaceController.treeModel.showHidden = activePanelController().directoryModel.showHidden
        }
    }

    Connections {
        target: quickLookController
        function onVisibleChanged() {
            if (root.previewPaneVisible !== quickLookController.visible) {
                root.previewPaneVisible = quickLookController.visible
            }
            if (!quickLookController.visible) {
                previewSyncTimer.stop()
                hoverPreviewTimer.stop()
                quickLookController.preview("")
            }
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onHoveredPathChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 0 && root.previewOnHover) {
                if (workspaceController.leftPanel.hoveredPath.length > 0) {
                    scheduleHoverPreview(workspaceController.leftPanel.hoveredPath)
                } else {
                    hoverPreviewTimer.stop()
                }
            }
        }
        function onCurrentPathChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 0) {
                syncPreviewFromActivePanel(!workspaceController.leftPanel.scrolling)
            }
        }
        function onScrollingChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 0
                    && workspaceController.leftPanel.scrolling) {
                previewSyncTimer.stop()
                hoverPreviewTimer.stop()
            }
            if (root.previewPaneVisible && workspaceController.activePanel === 0
                    && !workspaceController.leftPanel.scrolling) {
                syncPreviewFromActivePanel(false)
            }
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onHoveredPathChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 1 && root.previewOnHover) {
                if (workspaceController.rightPanel.hoveredPath.length > 0) {
                    scheduleHoverPreview(workspaceController.rightPanel.hoveredPath)
                } else {
                    hoverPreviewTimer.stop()
                }
            }
        }
        function onCurrentPathChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 1) {
                syncPreviewFromActivePanel(!workspaceController.rightPanel.scrolling)
            }
        }
        function onScrollingChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 1
                    && workspaceController.rightPanel.scrolling) {
                previewSyncTimer.stop()
                hoverPreviewTimer.stop()
            }
            if (root.previewPaneVisible && workspaceController.activePanel === 1
                    && !workspaceController.rightPanel.scrolling) {
                syncPreviewFromActivePanel(false)
            }
        }
    }

    function showBatchRename(paths) {
        if (!paths || paths.length === 0) return;
        batchRenameDialog.sourcePaths = paths
        batchRenameDialog.controller = workspaceController.activePanel === 0 
                                       ? workspaceController.leftPanel 
                                       : workspaceController.rightPanel
        batchRenameDialog.open()
    }

    BatchRenameDialog {
        id: batchRenameDialog
    }

    function showChecksums(paths) {
        if (!paths || paths.length === 0) return;
        checksumDialog.path1 = paths[0]
        checksumDialog.path2 = paths.length > 1 ? paths[1] : ""
        checksumDialog.controller = workspaceController.activePanel === 0 
                                     ? workspaceController.leftPanel 
                                     : workspaceController.rightPanel
        checksumDialog.open()
    }

    ChecksumDialog {
        id: checksumDialog
    }

    Connections {
        target: workspaceController.leftPanel.directoryModel
        function onSelectionChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 0) {
                syncPreviewFromActivePanel(!workspaceController.leftPanel.scrolling)
            }
        }
    }

    Connections {
        target: workspaceController.rightPanel.directoryModel
        function onSelectionChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 1) {
                syncPreviewFromActivePanel(!workspaceController.rightPanel.scrolling)
            }
        }
    }
}

