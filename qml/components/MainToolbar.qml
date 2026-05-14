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

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    component TbIcon: Image {
        sourceSize: Qt.size(14, 14)
        fillMode: Image.PreserveAspectFit
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 12
        spacing: 6

        ToolButton {
            onClicked: root.activeController.viewMode = (root.activeController.viewMode === 0 ? 1 : 0)

            background: Rectangle {
                implicitWidth: 52
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: RowLayout {
                spacing: 4
                anchors.centerIn: parent
                TbIcon {
                    source: root.activeController.viewMode === 0
                            ? "../assets/icons/list.svg"
                            : "../assets/icons/grid.svg"
                }
                Text {
                    text: root.activeController.viewMode === 0 ? "List" : "Grid"
                    color: Theme.textPrimary
                    font.pixelSize: 12
                }
            }
        }

        ToolButton {
            enabled: root.activeController.canGoBack
            onClicked: root.activeController.goBack()
            background: Rectangle {
                implicitWidth: 32
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: TbIcon {
                source: "../assets/icons/arrow-left.svg"
                opacity: parent.enabled ? 1.0 : 0.5
            }
        }

        ToolButton {
            enabled: root.activeController.canGoForward
            onClicked: root.activeController.goForward()
            background: Rectangle {
                implicitWidth: 32
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: TbIcon {
                source: "../assets/icons/arrow-right.svg"
                opacity: parent.enabled ? 1.0 : 0.5
            }
        }

        ToolButton {
            onClicked: root.activeController.goUp()
            background: Rectangle {
                implicitWidth: 32
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: TbIcon {
                source: "../assets/icons/arrow-up.svg"
            }
        }

        PathBar {
            id: pathBar
            Layout.fillWidth: true
            controller: root.activeController
        }

        ToolButton {
            checkable: true
            checked: workspaceController.splitEnabled
            onClicked: workspaceController.toggleSplit()

            background: Rectangle {
                implicitWidth: 68
                implicitHeight: 32
                color: parent.checked ? Theme.accent : (parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent"))
                border.color: parent.checked ? Theme.accent : Theme.border
                radius: 6
                Behavior on color { ColorAnimation { duration: Theme.motionFast } }
            }
            contentItem: RowLayout {
                spacing: 4
                anchors.centerIn: parent
                TbIcon {
                    source: "../assets/icons/columns-2.svg"
                }
                Text {
                    text: workspaceController.splitEnabled ? "Unsplit" : "Split"
                    color: parent.parent.checked ? Theme.accentText : Theme.textPrimary
                    font.pixelSize: 12
                }
            }
        }

        ToolButton {
            onClicked: themeController.mode = themeController.isDark ? 0 : 1

            background: Rectangle {
                implicitWidth: 56
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: RowLayout {
                spacing: 4
                anchors.centerIn: parent
                TbIcon {
                    source: themeController.isDark
                            ? "../assets/icons/sun.svg"
                            : "../assets/icons/moon.svg"
                }
                Text {
                    text: themeController.isDark ? "Light" : "Dark"
                    color: Theme.textPrimary
                    font.pixelSize: 12
                }
            }
        }

        ToolButton {
            text: "Copy"
            enabled: workspaceController.splitEnabled
                     && root.activeController.directoryModel.selectedCount > 0
                     && !workspaceController.operationQueue.busy
            onClicked: workspaceController.copyActiveSelectionToOpposite()

            background: Rectangle {
                implicitWidth: 56
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
                opacity: parent.enabled ? 1.0 : 0.4
            }
            contentItem: RowLayout {
                spacing: 4
                anchors.centerIn: parent
                TbIcon { source: "../assets/icons/copy.svg" }
                Text {
                    text: parent.parent.text
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    opacity: parent.parent.enabled ? 1.0 : 0.5
                }
            }
        }

        ToolButton {
            text: "Move"
            enabled: workspaceController.splitEnabled
                     && root.activeController.directoryModel.selectedCount > 0
                     && !workspaceController.operationQueue.busy
            onClicked: workspaceController.moveActiveSelectionToOpposite()
            background: Rectangle {
                implicitWidth: 56
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
                opacity: parent.enabled ? 1.0 : 0.4
            }
            contentItem: RowLayout {
                spacing: 4
                anchors.centerIn: parent
                TbIcon { source: "../assets/icons/move.svg" }
                Text {
                    text: parent.parent.text
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    opacity: parent.parent.enabled ? 1.0 : 0.5
                }
            }
        }

        ToolButton {
            onClicked: root.activeController.directoryModel.showHidden = !root.activeController.directoryModel.showHidden

            background: Rectangle {
                implicitWidth: 86
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: RowLayout {
                spacing: 4
                anchors.centerIn: parent
                TbIcon {
                    source: root.activeController.directoryModel.showHidden
                            ? "../assets/icons/eye-off.svg"
                            : "../assets/icons/eye.svg"
                }
                Text {
                    text: root.activeController.directoryModel.showHidden ? "Hide Hidden" : "Show Hidden"
                    color: Theme.textPrimary
                    font.pixelSize: 12
                }
            }
        }

        ToolButton {
            onClicked: root.activeController.refresh()
            background: Rectangle {
                implicitWidth: 56
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: RowLayout {
                spacing: 4
                anchors.centerIn: parent
                TbIcon { source: "../assets/icons/refresh.svg" }
                Text { text: "Refresh"; color: Theme.textPrimary; font.pixelSize: 12 }
            }
        }

        ToolButton {
            onClicked: root.activeController.createFolder("New Folder")
            background: Rectangle {
                implicitWidth: 66
                implicitHeight: 32
                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                border.color: Theme.border
                radius: 6
            }
            contentItem: RowLayout {
                spacing: 4
                anchors.centerIn: parent
                TbIcon { source: "../assets/icons/folder-plus.svg" }
                Text { text: "+ Folder"; color: Theme.textPrimary; font.pixelSize: 12 }
            }
        }

        TextField {
            id: searchField
            Layout.preferredWidth: 150
            leftPadding: 22
            placeholderText: "Search..."
            text: root.activeController.directoryModel.filterText
            onTextChanged: root.activeController.directoryModel.filterText = text
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Escape) {
                    text = ""
                    root.activeController.directoryModel.filterText = ""
                    root.parent.forceActiveFocus() // Try to return focus
                }
            }
            color: Theme.textPrimary
            placeholderTextColor: Theme.textSecondary
            font.pixelSize: 13
            background: Rectangle {
                color: Theme.bg
                border.color: searchField.activeFocus ? Theme.accent : Theme.border
                radius: Theme.radius - 2
            }
            TbIcon {
                x: 6
                y: (parent.height - height) / 2
                source: "../assets/icons/search.svg"
                opacity: 0.5
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
