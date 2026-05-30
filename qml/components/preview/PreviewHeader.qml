import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"

Rectangle {
    id: root

    property string iconSource: ""
    property string title: ""
    property string subtitle: ""
    property string closeIconSource: ""
    property color closeIconTint: Theme.textSecondary
    property color closeIconTintHover: Theme.textPrimary
    property bool liveResizeActive: false
    readonly property bool ultraLightMode: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.ultraLightMode
                                           : false
    readonly property bool effectsReduced: root.liveResizeActive || root.ultraLightMode

    signal closeRequested()

    implicitHeight: 54
    color: "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 12
        spacing: 12

        Image {
            source: root.iconSource
            sourceSize: Qt.size(24, 24)
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: -2

            Label {
                text: root.title
                font.bold: true
                font.pixelSize: 15
                color: Theme.textPrimary
                Layout.fillWidth: true
                elide: Text.ElideMiddle
            }

            Label {
                text: root.subtitle
                font.pixelSize: 10
                color: Theme.textSecondary
                opacity: 0.7
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        Button {
            id: closeBtn
            onClicked: root.closeRequested()
            hoverEnabled: true

            background: Rectangle {
                implicitWidth: 32
                implicitHeight: 32
                radius: Theme.radiusLg
                color: closeBtn.hovered ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.10 : 0.06) : "transparent"

                Behavior on color {
                    enabled: !root.effectsReduced
                    ColorAnimation { duration: 150 }
                }

                scale: closeBtn.hovered ? 1.08 : 1.0
                Behavior on scale {
                    enabled: !root.effectsReduced
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }
            }

            contentItem: Image {
                source: root.closeIconSource
                sourceSize: Qt.size(18, 18)
                opacity: closeBtn.hovered ? 1.0 : 0.72
                smooth: true
                layer.enabled: !root.effectsReduced
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: closeBtn.hovered ? root.closeIconTintHover : root.closeIconTint
                }
            }
        }
    }
}
