import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../../style"

Rectangle {
    id: root

    property string iconSource: ""
    property string fallbackIconSource: ""
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

    implicitHeight: root.subtitle.indexOf("\n") >= 0 ? 66 : 54
    color: "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 12
        spacing: 12

        Item {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24

            Image {
                id: primaryIcon
                anchors.fill: parent
                source: root.iconSource
                sourceSize: Qt.size(24, 24)
                visible: root.iconSource.length > 0 && status !== Image.Error
            }

            Image {
                anchors.fill: parent
                source: root.fallbackIconSource
                sourceSize: Qt.size(24, 24)
                visible: root.fallbackIconSource.length > 0 && (root.iconSource.length === 0 || primaryIcon.status === Image.Error)
            }
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
                maximumLineCount: root.subtitle.indexOf("\n") >= 0 ? 2 : 1
                wrapMode: Text.NoWrap
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
                radius: Theme.radiusForSide(Math.min(width, height))
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

            contentItem: Item {
                implicitWidth: 18
                implicitHeight: 18

                RecolorSvgIcon {
                    anchors.centerIn: parent
                    width: parent.implicitWidth
                    height: parent.implicitHeight
                    sourcePath: root.closeIconSource
                    sourceSize: Qt.size(36, 36)
                    recolorColor: closeBtn.hovered ? root.closeIconTintHover : root.closeIconTint
                    opacity: closeBtn.hovered ? 1.0 : 0.72
                }
            }
        }
    }
}
