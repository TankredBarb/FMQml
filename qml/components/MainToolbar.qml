import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

ToolBar {
    id: root
    padding: 8
    background: Rectangle {
        color: Theme.surface
        border.color: Theme.border
    }

    readonly property var activeController: workspaceController.activePanel === 0
                                            ? workspaceController.leftPanel
                                            : workspaceController.rightPanel

    function focusPath() {
        pathBar.focusPath()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 12
        spacing: 6

        ToolButton {
            text: workspaceController.viewMode === 0 ? "List" : "Grid"
            onClicked: workspaceController.viewMode = (workspaceController.viewMode === 0 ? 1 : 0)

            background: Rectangle {
                implicitWidth: 60
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                color: Theme.textPrimary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
            }
        }


        ToolButton {
            text: "<"
            enabled: root.activeController.canGoBack
            onClicked: root.activeController.goBack()
            background: Rectangle {
                implicitWidth: 32
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: Text { text: parent.text; color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; opacity: parent.enabled ? 1.0 : 0.5 }
        }

        ToolButton {
            text: ">"
            enabled: root.activeController.canGoForward
            onClicked: root.activeController.goForward()
            background: Rectangle {
                implicitWidth: 32
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: Text { text: parent.text; color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; opacity: parent.enabled ? 1.0 : 0.5 }
        }

        ToolButton {
            text: "^"
            onClicked: root.activeController.goUp()
            background: Rectangle {
                implicitWidth: 32
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: Text { text: parent.text; color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        }

        PathBar {
            id: pathBar
            Layout.fillWidth: true
            controller: root.activeController
        }

        ToolButton {
            text: workspaceController.splitEnabled ? "Unsplit" : "Split"
            checkable: true
            checked: workspaceController.splitEnabled
            onClicked: workspaceController.toggleSplit()
            
            background: Rectangle {
                implicitWidth: 70
                implicitHeight: 32
                color: parent.checked ? Theme.accent : (parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent"))
                border.color: parent.checked ? Theme.accent : Theme.border
                radius: 6
                Behavior on color { ColorAnimation { duration: Theme.motionFast } }
            }
            contentItem: Text {
                text: parent.text
                color: parent.checked ? Theme.accentText : Theme.textPrimary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
            }
        }

        ToolButton {
            text: themeController.isDark ? "Light" : "Dark"
            onClicked: themeController.mode = themeController.isDark ? 0 : 1 // 0=Light, 1=Dark
            
            background: Rectangle {
                implicitWidth: 60
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: Text {
                text: parent.text
                color: Theme.textPrimary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
            }
        }

        ToolButton {
            text: "Copy"
            enabled: workspaceController.splitEnabled
                     && root.activeController.directoryModel.selectedCount > 0
                     && !workspaceController.operationQueue.busy
            onClicked: workspaceController.copyActiveSelectionToOpposite()
            
            background: Rectangle {
                implicitWidth: 60
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
                opacity: parent.enabled ? 1.0 : 0.4
            }
            contentItem: Text {
                text: parent.text
                color: Theme.textPrimary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
                opacity: parent.enabled ? 1.0 : 0.5
            }
        }

        ToolButton {
            text: "Move"
            enabled: workspaceController.splitEnabled
                     && root.activeController.directoryModel.selectedCount > 0
                     && !workspaceController.operationQueue.busy
            onClicked: workspaceController.moveActiveSelectionToOpposite()
            background: Rectangle { color: parent.down ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : Theme.surface); border.color: Theme.border; radius: Theme.radius }
            contentItem: Text { text: parent.text; color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; opacity: parent.enabled ? 1.0 : 0.5 }
        }

        ToolButton {
            text: "Refresh"
            onClicked: root.activeController.refresh()
            background: Rectangle {
                implicitWidth: 60
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: Text { text: parent.text; color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12 }
        }

        ToolButton {
            text: "+ Folder"
            onClicked: root.activeController.createFolder("New Folder")
            background: Rectangle {
                implicitWidth: 70
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: Text { text: parent.text; color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12 }
        }

        TextField {
            id: searchField
            Layout.preferredWidth: 150
            placeholderText: "Search..."
            text: root.activeController.directoryModel.filterText
            onTextChanged: root.activeController.directoryModel.filterText = text
            color: Theme.textPrimary
            placeholderTextColor: Theme.textSecondary
            font.pixelSize: 13
            background: Rectangle {
                color: Theme.bg
                border.color: searchField.activeFocus ? Theme.accent : Theme.border
                radius: Theme.radius - 2
            }
        }

        ProgressBar {
            Layout.preferredWidth: 140
            visible: workspaceController.operationQueue.busy
            from: 0
            to: 1
            value: workspaceController.operationQueue.progress
        }

        Label {
            Layout.maximumWidth: 160
            visible: workspaceController.operationQueue.busy || workspaceController.operationQueue.error.length > 0
            text: workspaceController.operationQueue.error.length > 0
                  ? workspaceController.operationQueue.error
                  : workspaceController.operationQueue.currentLabel
            elide: Text.ElideMiddle
            color: workspaceController.operationQueue.error.length > 0 ? Theme.danger : Theme.textSecondary
            font.pixelSize: 12
        }
    }
}
