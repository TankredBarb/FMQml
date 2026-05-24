import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"

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

    signal activated()

    implicitWidth: 160
    implicitHeight: 132

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: 16
        color: Theme.panelSurfaceStrong
        border.width: 1
        border.color: root.selected ? Theme.focusRing : Theme.panelBorder

        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Theme.withAlpha(root.accentColor, root.selected ? 0.16 : 0.09) }
            GradientStop { position: 0.42; color: Theme.withAlpha(Theme.panelSurfaceStrong, 1.0) }
            GradientStop { position: 1.0; color: Theme.withAlpha(root.bgColor, root.selected ? 0.88 : 0.72) }
        }

        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }

    Rectangle {
        anchors.fill: shell
        radius: shell.radius
        color: "transparent"
        border.width: root.selected ? 1 : 0
        border.color: Theme.withAlpha(root.accentColor, 0.85)
        opacity: root.selected ? 1.0 : 0.0
    }

    Rectangle {
        anchors.fill: shell
        radius: shell.radius
        color: "transparent"
        border.width: 0
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.withAlpha(root.glowColor, root.selected ? 0.18 : 0.08) }
            GradientStop { position: 0.5; color: "transparent" }
            GradientStop { position: 1.0; color: Theme.withAlpha(root.accentColor, root.selected ? 0.20 : 0.10) }
        }
    }

    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Theme.glassShadow
        shadowBlur: 18
        shadowVerticalOffset: 6
        shadowHorizontalOffset: 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: root.title
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    text: root.subtitle
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Rectangle {
                visible: root.selected
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                radius: 9
                color: Theme.withAlpha(root.accentColor, 0.18)
                border.color: root.accentColor
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    text: "✓"
                    color: root.accentColor
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 38

            RowLayout {
                anchors.fill: parent
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: root.bgColor
                    border.color: Theme.withAlpha(Qt.darker(root.bgColor, 1.15), 0.48)
                    border.width: 1
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: root.surfaceColor
                    border.color: Theme.withAlpha(Theme.border, 0.55)
                    border.width: 1
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: root.accentColor
                    border.color: Theme.withAlpha(root.accentColor, 0.95)
                    border.width: 1
                }
            }
        }

        InlineBadge {
            text: root.selected ? "Active scheme" : "Click to apply"
            textColor: root.selected ? root.accentColor : Theme.textSecondary
            fillColor: Theme.withAlpha(root.accentColor, root.selected ? 0.16 : 0.10)
            strokeColor: Theme.withAlpha(root.accentColor, root.selected ? 0.36 : 0.22)
            horizontalPadding: 18
            badgeHeight: 20
            fontSize: 9
            fontWeight: Font.Medium
            Layout.alignment: Qt.AlignHCenter
        }
    }

    HoverHandler {
        id: hoverHandler
    }

    TapHandler {
        onTapped: root.activated()
    }

    states: [
        State {
            name: "hovered"
            when: hoverHandler.hovered && !root.selected
            PropertyChanges { target: shell; color: Theme.panelSurfaceStrong }
        }
    ]
}
