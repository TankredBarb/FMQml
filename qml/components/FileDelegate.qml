import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
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
    required property bool isHidden
    required property string sizeText
    required property string modifiedText
    property bool currentItem: false
    property bool panelActive: true
    property bool scrolling: false

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
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    Component.onCompleted: {
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    ListView.onPooled: {
        isRenaming = false
        visualOffsetX = 0
        if (root.controller.hoveredPath === root.path) {
            root.controller.hoveredPath = ""
        }
    }

    ListView.onReused: {
        isRenaming = false
        visualOffsetX = 0
        opacity = Qt.binding(() => isHidden ? 0.55 : 1.0)
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    function startRename() {
        root.isRenaming = true
    }

    opacity: isHidden ? 0.55 : 1.0

    Rectangle {
        id: bgRect
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        radius: Theme.radiusSm

        color: isSelected
               ? (root.panelActive ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
               : (root.currentItem
                  ? Theme.itemCurrentFill
                  : ((hover.hovered && !root.scrolling) ? Theme.itemHoverFill : "transparent"))
        border.color: isSelected
                      ? (root.panelActive ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                      : (root.currentItem ? Theme.itemCurrentBorder : "transparent")
        border.width: isSelected || root.currentItem ? 1 : 0
        transform: Translate { x: root.visualOffsetX }

        // Subtle vertical indicator bar for selected rows
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 4
            anchors.bottomMargin: 4
            anchors.leftMargin: 4
            width: isSelected ? 3 : 0
            radius: 1.5
            color: Theme.accent
            
            Behavior on width { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutQuad } }
        }

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }

    HoverHandler {
        id: hover
        enabled: true
        onHoveredChanged: {
            if (root.scrolling) return
            if (hovered) {
                root.controller.hoveredPath = root.path
            } else if (root.controller.hoveredPath === root.path) {
                root.controller.hoveredPath = ""
            }
        }
    }

    onScrollingChanged: {
        if (!scrolling) {
            Qt.callLater(() => {
                if (hover) {
                    hover.enabled = false
                    hover.enabled = true
                    if (hover.hovered) {
                        root.controller.hoveredPath = root.path
                    }
                }
            })
        }
    }

    Connections {
        target: root.controller ? root.controller.directoryModel : null
        ignoreUnknownSignals: true
        function onLoadingChanged() {
            if (root.controller && root.controller.directoryModel && !root.controller.directoryModel.loading) {
                Qt.callLater(() => {
                    if (hover) {
                        hover.enabled = false
                        hover.enabled = true
                        if (hover.hovered) {
                            root.controller.hoveredPath = root.path
                        }
                    }
                })
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

    Loader {
        id: renameLoader
        anchors.fill: parent
        anchors.leftMargin: 52
        anchors.rightMargin: 8
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        active: root.isRenaming
        visible: root.isRenaming
        sourceComponent: TextField {
            id: renameInput
            text: root.name
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 13
            color: Theme.textPrimary
            selectByMouse: true
            leftPadding: 8
            rightPadding: 8
            
            opacity: 0
            scale: 0.96
            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }

            background: Rectangle {
                id: bgRect
                color: Theme.panelSurfaceStrong
                radius: Theme.radiusSm
                border.color: renameInput.activeFocus ? Theme.focusRing : Theme.panelBorder
                border.width: renameInput.activeFocus ? 1.5 : 1
                
                Behavior on border.color { ColorAnimation { duration: 120 } }
                Behavior on border.width { NumberAnimation { duration: 120 } }

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: renameInput.activeFocus 
                        ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.35 : 0.2)
                        : Theme.glassShadow
                    shadowBlur: renameInput.activeFocus ? 12 : 8
                    shadowVerticalOffset: renameInput.activeFocus ? 1 : 2
                    
                    Behavior on shadowColor { ColorAnimation { duration: 120 } }
                    Behavior on shadowBlur { NumberAnimation { duration: 120 } }
                }
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
                            if (renameLoader.item) {
                                renameLoader.item.forceActiveFocus()
                                renameLoader.item.selectAll()
                            }
                        }
                    })
                }
            }
            
            Keys.onEscapePressed: (event) => {
                root.isRenaming = false
                event.accepted = true
            }

            onActiveFocusChanged: if (!activeFocus) root.isRenaming = false
            
            Component.onCompleted: {
                opacity = 1.0
                scale = 1.0
                forceActiveFocus()
                let lastDot = name.lastIndexOf(".")
                if (!isDirectory && lastDot > 0) {
                    select(0, lastDot)
                } else {
                    selectAll()
                }
            }
        }
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
                    source: "image://icon/" + encodeURIComponent(root.path + (root.isDirectory ? "?directory=true" : ""))
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
            text: root.isDirectory ? "Folder" : root.sizeText
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
