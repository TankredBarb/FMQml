import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Item {
    id: root

    required property var controller
    
    // Model roles
    required property int index
    required property string name
    required property string path
    required property bool isDirectory
    required property bool isSelected
    required property string sizeText
    required property string modifiedText

    // Signals
    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: Theme.rowHeight

    property bool isRenaming: false

    // Tactile Feedback
    scale: mouseArea.pressed ? 0.98 : (hover.hovered ? 1.01 : 1.0)
    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }

    function startRename() {
        root.isRenaming = true
        renameField.forceActiveFocus()
        
        let lastDot = name.lastIndexOf(".")
        if (!isDirectory && lastDot > 0) {
            renameField.select(0, lastDot)
        } else {
            renameField.selectAll()
        }
    }

    Rectangle {
        id: bgRect
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        radius: 6
        
        color: isSelected || hover.hovered ? Theme.surfaceHover : "transparent"
        border.color: isSelected ? Theme.accent : "transparent"
        border.width: isSelected ? 1 : 0

        Behavior on color { 
            enabled: !hover.hovered
            ColorAnimation { duration: 100 } 
        }
    }

    HoverHandler {
        id: hover
        onHoveredChanged: {
            if (hovered) {
                root.controller.hoveredPath = root.path
            } else if (root.controller.hoveredPath === root.path) {
                root.controller.hoveredPath = ""
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: false 

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                root.rightClicked()
            } else {
                root.clicked(mouse)
            }
        }

        onDoubleClicked: root.doubleClicked()
    }

    TextField {
        id: renameField
        anchors.fill: parent
        anchors.leftMargin: 52
        anchors.rightMargin: 8
        visible: root.isRenaming
        text: root.name
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 13
        color: Theme.textPrimary
        selectByMouse: true
        background: Rectangle { 
            color: Theme.surface
            radius: 4
            border.color: Theme.accent
        }

        onAccepted: {
            if (root.index >= 0) {
                const idx = root.index
                const txt = text
                const ctrl = controller
                Qt.callLater(function() {
                    if (!ctrl.rename(idx, txt))
                        root.isRenaming = false
                })
            }
        }
        onActiveFocusChanged: if (!activeFocus) root.isRenaming = false
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12
        visible: !isRenaming

        Item {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            
            Image {
                anchors.centerIn: parent
                source: "image://icon/" + root.path
                sourceSize: Qt.size(24, 24)
                asynchronous: true
                cache: true
            }
        }

        Label {
            Layout.fillWidth: true
            text: name
            color: Theme.textPrimary
            elide: Text.ElideRight
            font.pixelSize: 13
            font.weight: isSelected ? Font.Medium : Font.Normal
        }

        Label {
            text: sizeText
            color: Theme.textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 80
            horizontalAlignment: Text.AlignRight
            visible: parent.width > 400
        }

        Label {
            text: modifiedText
            color: Theme.textSecondary
            font.pixelSize: 12
            Layout.preferredWidth: 140
            horizontalAlignment: Text.AlignRight
            visible: parent.width > 600
        }
    }
}
