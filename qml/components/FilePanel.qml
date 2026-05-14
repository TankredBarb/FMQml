import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "../style"

Pane {
    id: root

    required property var controller
    property bool active: false
    readonly property int viewMode: root.controller.viewMode

    signal activated()

    property string statusMessage: ""
    Timer {
        id: statusTimer
        interval: 5000
        onTriggered: root.statusMessage = ""
    }

    Connections {
        target: workspaceController.operationQueue
        function onStatusMessageChanged() {
            root.statusMessage = workspaceController.operationQueue.statusMessage
            statusTimer.restart()
        }
        function onBusyChanged() {
            if (!workspaceController.operationQueue.busy) {
                statusTimer.restart()
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.controller.directoryModel.selectedCount > 0
        onActivated: root.controller.directoryModel.clearSelection()
    }

    padding: 0
    background: Rectangle {
        color: themeController.isDark
                ? Theme.surface
                : Theme.bg

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                          themeController.isDark ? 0.04 : 0.06)
        }

        radius: Theme.radius
        border.color: root.active ? Theme.accent : Theme.border
        border.width: root.active ? 2 : 1

        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }

    function contextRow() {
        return root.viewMode === 0 ? listView.currentIndex : gridView.currentIndex
    }

    function startRename() {
        let idx = contextRow()
        if (idx < 0) return
        
        if (root.viewMode === 0) {
            if (listView.currentItem) listView.currentItem.startRename()
        } else {
            if (gridView.currentItem) gridView.currentItem.startRename()
        }
    }

    Connections {
        target: workspaceController
        function onRenameRequested() {
            if (root.active) root.startRename()
        }
    }

    readonly property string revealInOsLabel: Qt.platform.os === "windows" ? "Show in Explorer"
            : Qt.platform.os === "osx" ? "Reveal in Finder"
            : "Open Containing Folder"

    ThemedContextMenu {
        id: contextMenu
        ThemedMenuItem {
            text: "Open"
            icon.source: "../assets/icons/folder-plus.svg"
            enabled: contextRow() >= 0
            onTriggered: root.controller.openItem(contextRow())
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Cut to Clipboard"
            icon.source: "../assets/icons/move.svg"
            enabled: root.controller.directoryModel.selectedCount > 0
                     && !workspaceController.operationQueue.busy
            onTriggered: workspaceController.cutToClipboard()
        }
        ThemedMenuItem {
            text: "Copy to Clipboard"
            icon.source: "../assets/icons/copy.svg"
            enabled: root.controller.directoryModel.selectedCount > 0
                     && !workspaceController.operationQueue.busy
            onTriggered: workspaceController.copyToClipboard()
        }
        ThemedMenuItem {
            text: "Paste from Clipboard"
            icon.source: "../assets/icons/paste.svg"
            enabled: workspaceController.hasClipboard && !workspaceController.operationQueue.busy
            onTriggered: workspaceController.pasteFromClipboard()
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Rename"
            icon.source: "../assets/icons/rename.svg"
            enabled: contextRow() >= 0
            onTriggered: root.startRename()
        }
        ThemedMenuItem {
            text: "Delete"
            icon.source: "../assets/icons/delete.svg"
            destructive: true
            enabled: root.controller.directoryModel.selectedCount > 0
                     && !workspaceController.operationQueue.busy
            onTriggered: workspaceController.deleteActiveSelection()
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Refresh"
            icon.source: "../assets/icons/refresh.svg"
            onTriggered: root.controller.refresh()
        }
        ThemedMenuItem {
            text: revealInOsLabel
            icon.source: "../assets/icons/reveal.svg"
            enabled: contextRow() >= 0
            onTriggered: root.controller.revealInFileManager(contextRow())
        }
        ThemedMenuItem {
            text: "Properties"
            icon.source: "../assets/icons/info.svg"
            enabled: contextRow() >= 0
            onTriggered: root.controller.showProperties(contextRow())
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Open in PowerShell"
            icon.source: "../assets/icons/terminal.svg"
            visible: Qt.platform.os === "windows"
            enabled: root.controller.currentPath.length > 0
            onTriggered: root.controller.openInTerminal()
        }
    }

    ThemedContextMenu {
        id: emptyContextMenu
        ThemedMenuItem {
            text: "Open in PowerShell"
            icon.source: "../assets/icons/terminal.svg"
            visible: Qt.platform.os === "windows"
            enabled: root.controller.currentPath.length > 0
            onTriggered: root.controller.openInTerminal()
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "New Folder"
            icon.source: "../assets/icons/folder-plus.svg"
            onTriggered: root.controller.createFolder("New Folder")
        }
        ThemedMenuItem {
            text: "New Text File"
            icon.source: "../assets/icons/document.svg"
            onTriggered: root.controller.createFile("New Text File.txt")
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Paste from Clipboard"
            icon.source: "../assets/icons/paste.svg"
            enabled: workspaceController.hasClipboard && !workspaceController.operationQueue.busy
            onTriggered: workspaceController.pasteFromClipboard()
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Select All"
            icon.source: "../assets/icons/select-all.svg"
            onTriggered: root.controller.directoryModel.selectAll()
        }
        ThemedMenuItem {
            text: root.controller.directoryModel.showHidden ? "Hide Hidden Files" : "Show Hidden Files"
            icon.source: root.controller.directoryModel.showHidden ? "../assets/icons/eye-off.svg" : "../assets/icons/eye.svg"
            onTriggered: root.controller.directoryModel.showHidden = !root.controller.directoryModel.showHidden
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Refresh"
            icon.source: "../assets/icons/refresh.svg"
            onTriggered: root.controller.refresh()
        }
        ThemedMenuItem {
            text: "Properties"
            icon.source: "../assets/icons/info.svg"
            onTriggered: propertiesController.load(root.controller.currentPath)
        }
    }

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasText) {
                const paths = [drop.text]
                workspaceController.operationQueue.copyTo(paths, root.controller.currentPath)
            }
        }
        
        Rectangle {
            anchors.fill: parent
            color: Theme.accent
            opacity: parent.containsDrag ? 0.1 : 0
            visible: parent.containsDrag
            border.color: Theme.accent
            border.width: 2
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 42
            color: "transparent"

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        root.activated()
                        emptyContextMenu.popup()
                    } else {
                        root.activated()
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 12
                spacing: 8

                PathBar {
                    id: panelPathBar
                    Layout.fillWidth: true
                    controller: root.controller
                    onActiveFocusChanged: if (activeFocus) root.activated()
                }

                Label {
                    text: root.controller.directoryModel.selectedCount > 0
                          ? root.controller.directoryModel.selectedCount + " selected  ·  Esc to clear"
                          : root.controller.directoryModel.count + " items"
                    color: root.controller.directoryModel.selectedCount > 0
                           ? Theme.danger : Theme.textPrimary
                    font.pixelSize: 12
                    font.bold: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: listView
                anchors.fill: parent
                visible: root.viewMode === 0
                enabled: visible
                clip: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                pixelAligned: false
                flickableDirection: Flickable.VerticalFlick
                model: root.controller.directoryModel
                currentIndex: -1
                focus: root.active
                cacheBuffer: height * 4
                
                highlight: null
                highlightFollowsCurrentItem: false

                // Layout Transitions
                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: 250 }
                    NumberAnimation { property: "x"; from: -30; duration: 250; easing.type: Easing.OutQuad }
                }
                remove: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; to: 0; duration: 200 }
                        NumberAnimation { property: "scale"; to: 0.8; duration: 200 }
                    }
                }
                displaced: Transition {
                    NumberAnimation { properties: "x,y"; duration: 200; easing.type: Easing.OutQuad }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                        if (currentIndex >= 0 && listView.currentItem && !listView.currentItem.isRenaming)
                            root.controller.openItem(currentIndex)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Backspace) {
                        root.controller.goUp()
                        event.accepted = true
                    } else if (event.key === Qt.Key_F2) {
                        root.startRename()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        root.controller.directoryModel.clearSelection()
                        event.accepted = true
                    }
                }

                delegate: FileDelegate {
                    id: fileDelegate
                    width: listView.width
                    controller: root.controller
                    
                    onClicked: (mouse) => {
                        root.activated()
                        listView.currentIndex = index
                        if (mouse.modifiers & Qt.ControlModifier) {
                            root.controller.directoryModel.toggleSelected(index)
                        } else {
                            root.controller.directoryModel.selectOnly(index)
                        }
                    }
                    onDoubleClicked: root.controller.openItem(index)
                    onRightClicked: {
                        root.activated()
                        listView.currentIndex = index
                        contextMenu.popup()
                    }
                }

                ScrollBar.vertical: ScrollBar {}

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        root.activated()
                        emptyContextMenu.popup()
                    }
                }
            }

            GridView {
                id: gridView
                anchors.fill: parent
                anchors.margins: 10
                visible: root.viewMode === 1
                enabled: visible
                clip: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                pixelAligned: false
                flickableDirection: Flickable.VerticalFlick
                cellWidth: 100
                cellHeight: 120
                model: root.controller.directoryModel
                currentIndex: -1
                focus: root.active
                cacheBuffer: Math.max(0, height * 4)
                
                highlight: null
                highlightFollowsCurrentItem: false

                // Layout Transitions
                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: 300 }
                    NumberAnimation { property: "scale"; from: 0.6; to: 1.0; duration: 300; easing.type: Easing.OutBack }
                }
                remove: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; to: 0; duration: 200 }
                        NumberAnimation { property: "scale"; to: 0.6; duration: 200 }
                    }
                }
                displaced: Transition {
                    NumberAnimation { properties: "x,y"; duration: 300; easing.type: Easing.OutBack }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                        if (currentIndex >= 0 && gridView.currentItem && !gridView.currentItem.isRenaming)
                            root.controller.openItem(currentIndex)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Backspace) {
                        root.controller.goUp()
                        event.accepted = true
                    } else if (event.key === Qt.Key_F2) {
                        root.startRename()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        root.controller.directoryModel.clearSelection()
                        event.accepted = true
                    }
                }

                delegate: Item {
                    id: gridDelegate
                    width: gridView.cellWidth
                    height: gridView.cellHeight

                    required property int index
                    required property string name
                    required property string path
                    required property string suffix
                    required property bool isDirectory
                    required property bool isSelected
                    required property bool isImage

                    property bool isRenaming: false

                    function startRename() {
                        isRenaming = true
                        gridRenameField.forceActiveFocus()
                        let lastDot = name.lastIndexOf(".")
                        if (!isDirectory && lastDot > 0) {
                            gridRenameField.select(0, lastDot)
                        } else {
                            gridRenameField.selectAll()
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: 6
                        color: isSelected || hoverGrid.hovered ? Theme.surfaceHover : "transparent"
                        border.color: isSelected ? Theme.accent : "transparent"
                        border.width: isSelected ? 1 : 0
                    }

                    HoverHandler { 
                        id: hoverGrid 
                        onHoveredChanged: {
                            if (hovered) {
                                root.controller.hoveredPath = path
                            } else if (root.controller.hoveredPath === path) {
                                root.controller.hoveredPath = ""
                            }
                        }
                    }

                    TextField {
                        id: gridRenameField
                        anchors.top: parent.top
                        anchors.topMargin: 74
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        height: 24
                        visible: isRenaming
                        text: name
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 12
                        color: Theme.textPrimary
                        selectByMouse: true
                        background: Rectangle {
                            color: Theme.surface
                            radius: 4
                            border.color: Theme.accent
                        }
                        onAccepted: {
                            if (index >= 0) {
                                const idx = index
                                const txt = text
                                const ctrl = root.controller
                                Qt.callLater(function() {
                                    if (!ctrl.rename(idx, txt))
                                        isRenaming = false
                                })
                            }
                        }
                        onActiveFocusChanged: if (!activeFocus) isRenaming = false
                    }

                    ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    source: isImage ? "image://thumbnail/" + path : "image://icon/" + path
                    sourceSize: Qt.size(48, 48)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: true
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !isRenaming
                        text: name
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    font.pixelSize: 12
                        color: Theme.textPrimary
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                    }
                    }
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        
                        onClicked: (mouse) => {
                            if (!root.visible) return
                            root.activated()
                            gridView.currentIndex = index
                            if (mouse.button === Qt.RightButton) {
                                contextMenu.popup()
                            } else {
                                if (mouse.modifiers & Qt.ControlModifier) root.controller.directoryModel.toggleSelected(index)
                                else root.controller.directoryModel.selectOnly(index)
                            }
                        }
                        onDoubleClicked: root.controller.openItem(index)
                    }
                }

                ScrollBar.vertical: ScrollBar {}

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        root.activated()
                        emptyContextMenu.popup()
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 34
                visible: root.statusMessage.length > 0 && root.active
                color: Theme.surface

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: Theme.border
                }

                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.statusMessage
                    color: Theme.accent
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    width: parent.width - 24
                }
            }
        }
    }
}

