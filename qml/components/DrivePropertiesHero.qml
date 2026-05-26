import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Rectangle {
    id: root

    required property string rootPathText
    required property string nameText
    required property string typeText
    required property color accentColor
    required property bool ready
    required property bool critical
    required property real percent
    required property string usedText
    required property string freeText
    required property string totalText

    Layout.fillWidth: true
    Layout.preferredHeight: 190
    radius: 24
    clip: true
    color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.11 : 0.07)
    border.color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.30 : 0.18)
    border.width: 1

    Rectangle {
        width: parent.width * 0.72
        height: width
        radius: width / 2
        x: parent.width * 0.52
        y: -height * 0.36
        color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.16 : 0.12)
    }

    Rectangle {
        width: parent.width * 0.46
        height: width
        radius: width / 2
        x: -width * 0.20
        y: parent.height * 0.46
        color: Theme.withAlpha(Theme.secondaryAccent, themeController.isDark ? 0.10 : 0.08)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Rectangle {
                Layout.preferredWidth: 58
                Layout.preferredHeight: 58
                radius: 18
                color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.22 : 0.14)
                border.color: Theme.withAlpha(root.accentColor, 0.35)
                border.width: 1

                Image {
                    anchors.centerIn: parent
                    width: 30
                    height: 30
                    source: "qrc:/qt/qml/FM/qml/assets/icons/hard-drive.svg"
                    sourceSize: Qt.size(30, 30)
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        colorization: 1.0
                        colorizationColor: root.accentColor
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Label {
                    text: root.rootPathText
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.0
                    color: Theme.withAlpha(root.accentColor, 0.96)
                    elide: Text.ElideRight
                }

                Label {
                    text: root.nameText
                    Layout.fillWidth: true
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    text: root.typeText
                    Layout.fillWidth: true
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.preferredWidth: 86
                Layout.preferredHeight: 30
                radius: 15
                color: root.ready
                    ? Theme.withAlpha(Theme.success, 0.14)
                    : Theme.withAlpha(Theme.warning, 0.16)
                border.color: root.ready
                    ? Theme.withAlpha(Theme.success, 0.34)
                    : Theme.withAlpha(Theme.warning, 0.36)

                Label {
                    anchors.centerIn: parent
                    text: root.ready ? "READY" : "OFFLINE"
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.0
                    color: root.ready ? Theme.success : Theme.warning
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: Math.round(root.percent * 100) + "% used"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: root.critical ? Theme.danger : Theme.textPrimary
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: root.freeText + " free of " + root.totalText
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 16
                radius: 8
                color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.10 : 0.08)
                border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
                border.width: 1
                clip: true

                Rectangle {
                    height: parent.height
                    width: Math.max(parent.height, parent.width * root.percent)
                    radius: 8
                    color: root.critical ? Theme.danger : root.accentColor

                    Behavior on width {
                        NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
                    }
                }
            }
        }
    }
}
