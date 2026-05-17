import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 460
    height: 580
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200 }
        NumberAnimation { property: "scale"; from: 0.9; to: 1.0; duration: 200; easing.type: Easing.OutBack }
    }

    background: Rectangle {
        color: Theme.glassSurfaceStrong
        radius: 16
        border.color: Theme.glassBorder
        border.width: 1
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.7
            shadowVerticalOffset: 12
            shadowColor: Theme.glassShadow
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 126
            radius: 16
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.12 : 0.07)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                Image {
                    source: "image://icon/" + propertiesController.path
                    sourceSize: Qt.size(48, 48)
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: propertiesController.name
                            font.bold: true
                            font.pixelSize: 20
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                        }

                        Rectangle {
                            radius: 10
                            height: 24
                            implicitWidth: typeChip.implicitWidth + 18
                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.18 : 0.12)
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                            border.width: 1

                            Label {
                                id: typeChip
                                anchors.centerIn: parent
                                text: propertiesController.typeText
                                color: Theme.accent
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    Label {
                        text: propertiesController.path
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            radius: 10
                            height: 24
                            implicitWidth: sizeChip.implicitWidth + (propertiesController.isCalculating ? 34 : 18)
                            color: Theme.glassSurface
                            border.color: Theme.glassBorder
                            border.width: 1

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 5

                                Image {
                                    visible: propertiesController.isCalculating
                                    source: "../assets/icons/refresh.svg"
                                    sourceSize: Qt.size(12, 12)
                                    width: 12
                                    height: 12
                                    opacity: 0.88
                                    antialiasing: true

                                    RotationAnimator on rotation {
                                        running: propertiesController.isCalculating
                                        from: 0
                                        to: 360
                                        duration: 900
                                        loops: Animation.Infinite
                                    }
                                }

                                Label {
                                    id: sizeChip
                                    text: propertiesController.sizeText
                                    color: Theme.textPrimary
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                        }

                        Rectangle {
                            radius: 10
                            height: 24
                            implicitWidth: infoChip.implicitWidth + 18
                            color: Theme.glassSurface
                            border.color: Theme.glassBorder
                            border.width: 1

                            Label {
                                id: infoChip
                                anchors.centerIn: parent
                                text: "Metadata"
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
            opacity: 0.28
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            background: null

            Pane {
                width: parent.width
                padding: 24
                background: null

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 16

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 98
                        radius: 14
                        color: Theme.glassSurface
                        border.color: Theme.glassBorder
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 8

                            Label {
                                text: "DETAILS"
                                font.bold: true
                                font.pixelSize: 11
                                color: Theme.textSecondary
                            }

                            PropertyRow { label: "Type"; value: propertiesController.typeText }
                            PropertyRow { label: "Location"; value: propertiesController.path; elideMode: true }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border; opacity: 0.25 }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 146
                        radius: 14
                        color: Theme.glassSurface
                        border.color: Theme.glassBorder
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 8

                            Label {
                                text: "DATES"
                                font.bold: true
                                font.pixelSize: 11
                                color: Theme.textSecondary
                            }

                            PropertyRow { label: "Created"; value: propertiesController.created }
                            PropertyRow { label: "Modified"; value: propertiesController.modified }
                            PropertyRow { label: "Accessed"; value: propertiesController.accessed }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 74
            color: Qt.rgba(0, 0, 0, themeController.isDark ? 0.10 : 0.03)
            radius: 16

            Button {
                anchors.centerIn: parent
                text: "Close"
                onClicked: {
                    propertiesController.visible = false
                    root.close()
                }
                
                background: Rectangle {
                    implicitWidth: 112
                    implicitHeight: 36
                    radius: 10
                    color: parent.pressed
                           ? Qt.darker(Theme.accent, 1.06)
                           : (parent.hovered ? Qt.lighter(Theme.accent, 1.05) : Theme.accent)
                    border.color: Theme.accent
                    border.width: 1
                }
                contentItem: Label {
                    text: parent.text
                    font.bold: true
                    font.pixelSize: 13
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    component PropertyRow : RowLayout {
        property string label: ""
        property string value: ""
        property bool elideMode: false
        spacing: 10

        Label {
            text: label + ":"
            color: Theme.textSecondary
            Layout.preferredWidth: 84
        }

        Label {
            text: value
            color: Theme.textPrimary
            Layout.fillWidth: true
            elide: elideMode ? Text.ElideMiddle : Text.ElideNone
            wrapMode: elideMode ? Text.NoWrap : Text.Wrap
            font.pixelSize: 12
        }
    }

    Connections {
        target: propertiesController
        function onVisibleChanged() {
            if (propertiesController.visible) root.open()
            else root.close()
        }
    }

    onClosed: propertiesController.visible = false
    onVisibleChanged: {
        if (!visible && propertiesController.visible) {
            propertiesController.visible = false
        }
    }
}
