import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Item {
    id: root

    property int schemeIndex: 0
    property string title: ""
    property string subtitle: ""
    property color bgColor: Theme.bg
    property color surfaceColor: Theme.surface
    property color accentColor: Theme.accent
    property color glowColor: Theme.activeGlow
    property bool selected: false
    readonly property color cardFill: root.selected
                                      ? Theme.withAlpha(root.accentColor, themeController.isDark ? 0.17 : 0.10)
                                      : (cardMouse.containsMouse
                                         ? Theme.withAlpha(root.surfaceColor, themeController.isDark ? 0.42 : 0.18)
                                         : Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.88 : 0.98))
    readonly property color cardBorder: root.selected
                                        ? Theme.withAlpha(root.accentColor, themeController.isDark ? 0.82 : 0.64)
                                        : (cardMouse.containsMouse
                                           ? Theme.withAlpha(root.accentColor, themeController.isDark ? 0.52 : 0.38)
                                           : Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.18 : 0.16))
    readonly property color previewStroke: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.24 : 0.18)

    signal activated()

    implicitWidth: 184
    implicitHeight: 86
    Layout.fillWidth: true
    Layout.preferredHeight: 86

    layer.enabled: root.selected || cardMouse.containsMouse
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Theme.withAlpha(Theme.glassShadow, themeController.isDark ? 0.62 : 0.36)
        shadowBlur: root.selected ? 14 : (cardMouse.containsMouse ? 10 : 7)
        shadowVerticalOffset: root.selected ? 4 : 2
        shadowHorizontalOffset: 0
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: Theme.radiusMd
        color: root.cardFill
        border.width: 1
        border.color: root.cardBorder
        antialiasing: true

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }

    Rectangle {
        anchors.left: shell.left
        anchors.top: shell.top
        anchors.bottom: shell.bottom
        width: 4
        radius: Theme.radiusXs
        color: root.accentColor
        opacity: root.selected ? 0.98 : (cardMouse.containsMouse ? 0.56 : 0.22)

        Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 9
        spacing: 9

        Item {
            Layout.preferredWidth: 60
            Layout.preferredHeight: 60
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusMd
                color: root.bgColor
                border.color: root.selected || cardMouse.containsMouse
                              ? Theme.withAlpha(root.accentColor, themeController.isDark ? 0.68 : 0.46)
                              : root.previewStroke
                border.width: root.selected ? 2 : 1
                antialiasing: true
            }

            Rectangle {
                x: 6
                y: 6
                width: parent.width - 12
                height: parent.height - 18
                radius: Theme.radiusSm
                color: root.surfaceColor
                border.color: root.previewStroke
                border.width: 1
                antialiasing: true
            }

            Rectangle {
                x: 12
                y: 12
                width: parent.width - 24
                height: 2
                radius: 1
                color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.66 : 0.52)
            }

            Rectangle {
                x: 12
                y: 19
                width: parent.width - 24
                height: 2
                radius: 1
                color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.22 : 0.16)
            }

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 7
                anchors.rightMargin: 7
                anchors.bottomMargin: 7
                height: 10
                spacing: 3

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: root.accentColor
                    border.color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.18 : 0.14)
                    border.width: 1
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: root.glowColor
                    border.color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.18 : 0.14)
                    border.width: 1
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.58 : 0.68)
                    border.color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.18 : 0.14)
                    border.width: 1
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: root.title
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Rectangle {
                    visible: root.selected
                    Layout.preferredWidth: activeLabel.implicitWidth + 12
                    Layout.preferredHeight: 18
                    radius: Theme.radiusSm
                    color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.18 : 0.11)
                    border.color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.42 : 0.30)
                    border.width: 1

                    Label {
                        id: activeLabel
                        anchors.centerIn: parent
                        text: "Active"
                        color: root.accentColor
                        font.pixelSize: Theme.scaledSize(9)
                        font.weight: Font.DemiBold
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.subtitle
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeMicro
                lineHeight: 0.92
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
            }
        }
    }

    Rectangle {
        anchors.left: shell.left
        anchors.right: shell.right
        anchors.bottom: shell.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 5
        height: 2
        radius: 1
        color: root.accentColor
        opacity: root.selected ? 0.9 : 0.0
    }

    MouseArea {
        id: cardMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activated()
    }
}
