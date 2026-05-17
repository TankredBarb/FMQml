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
    property bool currentItem: false
    property bool panelActive: true

    // Signals
    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: Theme.rowHeight

    property bool isRenaming: false
    property real visualOffsetX: 0

    onPathChanged: {
        isRenaming = false
        visualOffsetX = 0
    }

    ListView.onPooled: {
        isRenaming = false
        visualOffsetX = 0
    }

    ListView.onReused: {
        isRenaming = false
        visualOffsetX = 0
    }

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
        
        color: isSelected
               ? (root.panelActive ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
               : (root.currentItem
                  ? Theme.itemCurrentFill
                  : (hover.hovered ? Qt.rgba(Theme.itemHoverFill.r, Theme.itemHoverFill.g, Theme.itemHoverFill.b,
                                               Theme.itemHoverFill.a + (themeController.isDark ? 0.02 : 0.015))
                                   : "transparent"))
        border.color: isSelected
                      ? (root.panelActive ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                      : (root.currentItem ? Theme.itemCurrentBorder : "transparent")
        border.width: isSelected || root.currentItem ? 1 : 0
        transform: Translate { x: root.visualOffsetX }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: root.currentItem || isSelected ? 3 : 0
            radius: 1.5
            color: isSelected ? Theme.accent : Theme.itemCurrentBorder
            visible: width > 0
        }

        Behavior on color { ColorAnimation { duration: 90 } }
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
                    if (ctrl.rename(idx, txt)) {
                        root.isRenaming = false
                    } else {
                        renameField.forceActiveFocus()
                        renameField.selectAll()
                    }
                })
            }
        }
        onActiveFocusChanged: if (!activeFocus) root.isRenaming = false
    }

        RowLayout {
            id: fileContent
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12
            visible: !isRenaming
            transform: Translate { x: root.visualOffsetX }

        Item {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            
            Image {
                anchors.centerIn: parent
                source: "image://icon/" + root.path
                sourceSize: Qt.size(20, 20)
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
            font.weight: isSelected || root.currentItem ? Font.Medium : Font.Normal
        }

        Label {
            text: sizeText
            color: Theme.textSecondary
            opacity: 0.92
            font.pixelSize: 12
            Layout.preferredWidth: 80
            horizontalAlignment: Text.AlignRight
            visible: parent.width > 400
        }

        Label {
            text: modifiedText
            color: Theme.textSecondary
            opacity: 0.92
            font.pixelSize: 12
            Layout.preferredWidth: 140
            horizontalAlignment: Text.AlignRight
            visible: parent.width > 600
        }
    }
}
