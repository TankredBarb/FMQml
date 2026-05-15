import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

ToolBar {
    id: root
    padding: 8
    property bool pathEditing: false
    property string pathEditError: ""
    readonly property bool textEditingActive: pathEditing || searchField.activeFocus
    background: Rectangle {
        color: Theme.surface
        border.color: Theme.border
    }

    readonly property var activeController: workspaceController.activePanel === 0
                                            ? workspaceController.leftPanel
                                            : workspaceController.rightPanel

    function focusPath() {
        root.pathEditError = ""
        pathEditor.text = root.activeController.currentPath
        root.pathEditing = true
        pathEditor.forceActiveFocus()
        pathEditor.selectAll()
    }

    function acceptPathEdit() {
        const path = pathEditor.text.trim()
        if (path.length > 0) {
            if (root.activeController.openPath(path)) {
                root.pathEditError = ""
                root.pathEditing = false
                workspaceController.focusActivePanel()
                return
            }

            root.pathEditError = "Path not found"
            pathEditor.forceActiveFocus()
            pathEditor.selectAll()
            return
        }

        root.pathEditError = "Enter a valid path"
        pathEditor.forceActiveFocus()
        pathEditor.selectAll()
    }

    function cancelPathEdit() {
        root.pathEditing = false
        root.pathEditError = ""
        workspaceController.focusActivePanel()
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

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.pathEditing ? 60 : 36

            PathBar {
                id: pathBar
                anchors.fill: parent
                controller: root.activeController
                visible: !root.pathEditing
            }

            Rectangle {
                anchors.fill: parent
                visible: root.pathEditing
                radius: Theme.radius
                color: themeController.isDark ? Theme.surface : Theme.bg
                border.color: pathEditor.activeFocus ? Theme.accent : Theme.border
                border.width: pathEditor.activeFocus ? 2 : 1
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                visible: root.pathEditing
                width: 3
                radius: 1.5
                color: root.pathEditError.length > 0 ? Theme.danger : Theme.accent
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                spacing: 4
                visible: root.pathEditing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TbIcon {
                        source: root.pathEditError.length > 0
                                ? "../assets/icons/info.svg"
                                : "../assets/icons/folder-plus.svg"
                        opacity: 0.75
                    }

                    TextField {
                        id: pathEditor
                        Layout.fillWidth: true
                        text: root.activeController.currentPath
                        placeholderText: "Enter path..."
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textSecondary
                        font.pixelSize: 13
                        selectByMouse: true
                        background: Rectangle {
                            color: Theme.bg
                            radius: 5
                            border.color: root.pathEditError.length > 0
                                          ? Theme.danger
                                          : (pathEditor.activeFocus ? Theme.accent : Theme.border)
                            border.width: 1
                        }
                        onTextEdited: {
                            if (root.pathEditError.length > 0) {
                                root.pathEditError = ""
                            }
                        }
                        onAccepted: root.acceptPathEdit()
                        onActiveFocusChanged: {
                            if (!activeFocus && root.pathEditing) {
                                root.cancelPathEdit()
                            }
                        }
                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_Escape) {
                                event.accepted = true
                                root.cancelPathEdit()
                            } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                                event.accepted = true
                                root.acceptPathEdit()
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.pathEditError.length > 0
                    text: root.pathEditError
                    color: Theme.danger
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }

        ToolButton {
            id: splitButton
            checkable: true
            checked: workspaceController.splitEnabled
            onClicked: workspaceController.toggleSplit()

            background: Rectangle {
                implicitWidth: 68
                implicitHeight: 32
                color: splitButton.checked
                       ? (themeController.isDark ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.95) : Theme.accent)
                       : (splitButton.pressed ? Theme.surfaceActive : (splitButton.hovered ? Theme.surfaceHover : "transparent"))
                border.color: splitButton.checked ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 1.0) : Theme.border
                border.width: splitButton.checked ? 2 : 1
                radius: 6
                Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: 5
                    color: "transparent"
                    border.color: splitButton.checked ? Qt.rgba(255, 255, 255, themeController.isDark ? 0.16 : 0.10) : "transparent"
                    border.width: splitButton.checked ? 1 : 0
                }
            }
            contentItem: RowLayout {
                spacing: 4
                anchors.centerIn: parent
                TbIcon {
                    source: "../assets/icons/columns-2.svg"
                    opacity: splitButton.checked ? 1.0 : 0.92
                }
                Text {
                    text: workspaceController.splitEnabled ? "Unsplit" : "Split"
                    color: splitButton.checked
                           ? (themeController.isDark ? "#0D0D0D" : "#FFFFFF")
                           : Theme.textPrimary
                    font.pixelSize: 12
                    font.weight: splitButton.checked ? Font.Bold : Font.Medium
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
                    root.activeController.directoryModel.clearSelection()
                    workspaceController.focusActivePanel()
                    event.accepted = true
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

        Rectangle {
            Layout.preferredHeight: 26
            Layout.preferredWidth: clipboardText.implicitWidth + 18
            visible: workspaceController.hasClipboard
            radius: 13
            color: workspaceController.clipboardCut
                    ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.12)
                    : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
            border.color: workspaceController.clipboardCut ? Theme.danger : Theme.accent
            border.width: 1

            Text {
                id: clipboardText
                anchors.centerIn: parent
                text: workspaceController.clipboardSummary
                color: workspaceController.clipboardCut ? Theme.danger : Theme.accent
                font.pixelSize: 11
                font.bold: true
            }
        }

        ProgressBar {
            Layout.preferredWidth: 128
            visible: workspaceController.operationQueue.busy
            from: 0
            to: 1
            value: workspaceController.operationQueue.progress
        }

        Label {
            Layout.maximumWidth: 220
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
