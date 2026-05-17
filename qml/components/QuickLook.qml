import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.85, 900)
    height: Math.min(parent.height * 0.85, 650)
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 250; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.95; duration: 150; easing.type: Easing.InCubic }
    }

    background: Item {
        Rectangle {
            id: bgRect
            anchors.fill: parent
            color: Theme.glassSurfaceStrong
            radius: 16
            border.color: Theme.glassBorder
            border.width: 1
        }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.glassShadow
            shadowBlur: 0.8
            shadowVerticalOffset: 12
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 12
                spacing: 12
                
                Image {
                    source: "image://icon/" + quickLookController.path
                    sourceSize: Qt.size(24, 24)
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: -2
                    Label {
                        text: quickLookController.path.split(/[/\\]/).pop()
                        font.bold: true
                        font.pixelSize: 15
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                    }
                    Label {
                        text: quickLookController.type.toUpperCase() + " Preview"
                        font.pixelSize: 10
                        color: Theme.textSecondary
                        opacity: 0.7
                    }
                }

                ToolButton {
                    id: closeBtn
                    onClicked: root.close()
                    padding: 8
                    contentItem: Image {
                        source: "../assets/icons/eye-off.svg"
                        sourceSize: Qt.size(18, 18)
                        opacity: closeBtn.hovered ? 1.0 : 0.6
                    }
                    background: Rectangle {
                        implicitWidth: 36
                        implicitHeight: 36
                        color: parent.hovered ? Theme.surfaceHover : "transparent"
                        radius: 18
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
            opacity: themeController.isDark ? 0.34 : 0.26
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Item {
                anchors.fill: parent
                visible: quickLookController.type === "text" || quickLookController.type === "info"

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Line Numbers Sidebar with transparency
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 45
                        color: Theme.glassSurfaceSoft
                        visible: quickLookController.type === "text"

                        Column {
                            anchors.fill: parent
                            anchors.topMargin: 24
                            spacing: 0
                            Repeater {
                                model: Math.min(quickLookController.lines, 100)
                                Label {
                                    width: parent.width
                                    text: index + 1
                                    font.family: "Cascadia Code, Consolas, Monospace"
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                    opacity: 0.5
                                    horizontalAlignment: Text.AlignHCenter
                                    height: 18.2
                                }
                            }
                        }

                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: Theme.border
                            opacity: 0.2
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        background: null
                        clip: true

                        TextArea {
                            id: textPreview
                            text: quickLookController.content
                            readOnly: true
                            color: Theme.textPrimary
                            font.family: "Cascadia Code, Consolas, Monospace"
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                            padding: 24
                            topPadding: 24
                            background: null
                            selectByMouse: true
                            selectionColor: Theme.accent
                            selectedTextColor: Theme.accentText
                        }
                    }
                }
            }

            Item {
                anchors.fill: parent
                visible: quickLookController.type === "image"
                
                Image {
                    id: previewImage
                    anchors.fill: parent
                    anchors.margins: 20
                    source: (quickLookController.type === "image" && root.opened) ? ("image://thumbnail/" + quickLookController.path) : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: true
                    sourceSize.width: 1600
                    sourceSize.height: 1600
                    smooth: true
                    opacity: status === Image.Ready ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 300 } }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: previewImage.status === Image.Loading
                }
            }
        }
    }

    onOpened: forceActiveFocus()
    
    Connections {
        target: quickLookController
        function onVisibleChanged() {
            if (quickLookController.visible && !root.opened) root.open()
            else if (!quickLookController.visible && root.opened) root.close()
        }
    }
    
    onClosed: quickLookController.visible = false
}
