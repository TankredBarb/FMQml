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
    visible: true
    title: "FM"
    color: Theme.bg

    Shortcut {
        sequence: "F3"
        onActivated: workspaceController.toggleSplit()
    }

    Shortcut {
        sequence: "F2"
        onActivated: {
            // This is a simplified approach, will need to be refined if index handling is complex.
            // Assuming active panel has focus or selection.
            // For now, implementing as a trigger for active panel's current item.
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

    Connections {
        target: workspaceController.operationQueue
        function onConflictDetected(source, destination) {
            conflictDialog.sourcePath = source
            conflictDialog.destinationPath = destination
            conflictDialog.open()
        }
    }
}

