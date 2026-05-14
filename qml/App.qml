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

    Shortcut {
        sequence: "F3"
        onActivated: workspaceController.toggleSplit()
    }

    Shortcut {
        sequence: "F2"
        onActivated: {
            let activeCtrl = workspaceController.activePanel === 0 
                             ? workspaceController.leftPanel 
                             : workspaceController.rightPanel
            // We need a way to find the actual panel component.
            // For now, let's assume we can trigger it through a signal or workspace controller.
            // A better way is to find the focused panel.
            workspaceController.triggerRename()
        }
    }

    Shortcut {
        sequence: "Space"
        onActivated: {
            if (quickLook.opened) {
                quickLook.close()
                return
            }
            if (propertiesDialog.opened) {
                propertiesDialog.close()
                return
            }
            let activeCtrl = workspaceController.activePanel === 0 
                             ? workspaceController.leftPanel 
                             : workspaceController.rightPanel
            
            // Prioritize hovered path for quick look/properties
            let targetPath = activeCtrl.hoveredPath
            let targetIndex = -1
            
            if (targetPath) {
                // Find index of hovered path to check if it's a directory
                targetIndex = activeCtrl.directoryModel.indexOfPath(targetPath)
            } else {
                let selected = activeCtrl.selectedPaths()
                if (selected.length > 0) {
                    targetPath = selected[0]
                    targetIndex = activeCtrl.directoryModel.indexOfPath(targetPath)
                }
            }

            if (targetIndex >= 0) {
                if (activeCtrl.directoryModel.isDirectoryAt(targetIndex)) {
                    propertiesController.load(targetPath)
                } else {
                    quickLookController.preview(targetPath)
                }
            }
        }
    }

    Shortcut {
        sequence: "Delete"
        onActivated: workspaceController.deleteActiveSelection()
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
        onActivated: workspaceController.activePanel === 0
                     ? workspaceController.leftPanel.refresh()
                     : workspaceController.rightPanel.refresh()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        focus: true

        Keys.onPressed: (event) => {
            if (event.text.length > 0 && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.ShiftModifier)) {
                // Check if we're already in a text field or if a modal is open
                if (!quickLook.opened && !conflictDialog.opened && !mainToolbar.activeFocus) {
                     mainToolbar.focusSearch()
                }
            }
        }

        MainToolbar {
            id: mainToolbar
            Layout.fillWidth: true
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
                SplitView.fillWidth: true
            }

            handle: Rectangle {
                implicitWidth: 1
                color: Theme.border
            }
        }
    }

    OperationsDrawer {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 320
    }

    ConflictDialog {
        id: conflictDialog
    }

    QuickLook {
        id: quickLook
    }

    PropertiesDialog {
        id: propertiesDialog
    }

    Connections {
        target: workspaceController.leftPanel
        function onRevealProperties(path) {
            propertiesController.load(path)
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onRevealProperties(path) {
            propertiesController.load(path)
        }
    }

    Connections {
        target: workspaceController.operationQueue
        function onConflictDetected(source, destination) {
            conflictDialog.sourcePath = source
            conflictDialog.destinationPath = destination
            conflictDialog.open()
        }
    }
}

