import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "../style"

Pane {
    id: root

    required property var controller
    property bool active: false
    readonly property int viewMode: workspaceController.viewMode

    signal activated()

    padding: 0
    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius
        border.color: root.active ? Theme.accent : Theme.border
        border.width: root.active ? 2 : 1
        
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }

    Menu {
        id: contextMenu
        MenuItem {
            text: "Rename"
            onTriggered: {
                if (root.viewMode === 0) {
                    if (listView.currentIndex >= 0) listView.currentItem.startRename()
                } else {
                    // Logic for GridView startRename handled via focus
                }
            }
        }
        MenuItem {
            text: "Delete"
            onTriggered: workspaceController.deleteActiveSelection()
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
                onClicked: root.activated()
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
                          ? root.controller.directoryModel.selectedCount + " selected"
                          : root.controller.directoryModel.count + " items"
                    color: Theme.textPrimary
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
                        if (currentIndex >= 0) root.controller.openRow(currentIndex)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Backspace) {
                        root.controller.goUp()
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
                    onDoubleClicked: root.controller.openRow(index)
                    onRightClicked: {
                        root.activated()
                        if (!isSelected) {
                            root.controller.directoryModel.selectOnly(index)
                        }
                        listView.currentIndex = index
                        contextMenu.popup()
                    }
                }

                ScrollBar.vertical: ScrollBar {}
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
                        if (currentIndex >= 0) root.controller.openRow(currentIndex)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Backspace) {
                        root.controller.goUp()
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

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: 6
                        color: isSelected ? Theme.surfaceActive : (hoverGrid.hovered ? Theme.surfaceHover : "transparent")
                        border.color: isSelected ? Theme.accent : "transparent"
                        border.width: isSelected ? 1 : 0
                    }

                    HoverHandler { id: hoverGrid }

                    ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    source: {
                        const images = ["jpg", "jpeg", "png", "gif", "bmp", "webp", "ico"]
                        if (!isDirectory && images.includes(suffix.toLowerCase())) {
                            return "image://thumbnail/" + path
                        }
                        return "image://icon/" + path
                    }
                    sourceSize: Qt.size(48, 48)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: name
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        font.pixelSize: 11
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
                                if (!isSelected) root.controller.directoryModel.selectOnly(index)
                                contextMenu.popup()
                            } else {
                                if (mouse.modifiers & Qt.ControlModifier) root.controller.directoryModel.toggleSelected(index)
                                else root.controller.directoryModel.selectOnly(index)
                            }
                        }
                        onDoubleClicked: root.controller.openRow(index)
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }
        }
    }
}
