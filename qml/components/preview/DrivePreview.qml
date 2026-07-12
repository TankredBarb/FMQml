import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"

Item {
    id: root

    property string title: "Drive"
    property string mountPoint: ""
    property string fileSystem: ""
    property string driveType: ""
    property string deviceDescription: ""
    property string freeText: ""
    property string totalText: ""
    property real usage: 0
    property bool critical: false
    property bool compact: false

    readonly property color accent: critical ? Theme.danger : Theme.actionIconColor("storage")

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.compact ? 8 : 18
        radius: Theme.panelRadius
        color: Theme.withAlpha(root.accent, themeController.isDark ? 0.12 : 0.08)
        border.color: Theme.withAlpha(root.accent, themeController.isDark ? 0.32 : 0.22)
        border.width: 1
        clip: true

        Rectangle {
            width: parent.width * 0.72
            height: width
            radius: width / 2
            x: parent.width * 0.55
            y: -height * 0.45
            color: Theme.withAlpha(root.accent, themeController.isDark ? 0.15 : 0.10)
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.compact ? 12 : 22
            spacing: root.compact ? 9 : 14

            RowLayout {
                Layout.fillWidth: true
                spacing: root.compact ? 10 : 14

                IconTile {
                    Layout.preferredWidth: root.compact ? 46 : 64
                    Layout.preferredHeight: width
                    tileSize: width
                    iconSize: root.compact ? 24 : 32
                    cornerRadius: Theme.radiusLg
                    source: "qrc:/qt/qml/FM/qml/assets/icons/hard-drive.svg"
                    iconColor: root.accent
                    tileColor: Theme.withAlpha(root.accent, themeController.isDark ? 0.20 : 0.14)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: root.title
                        font.pixelSize: root.compact ? Theme.fontSizeBody : Theme.scaledSize(23)
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.deviceDescription.length > 0 ? root.deviceDescription : root.driveType
                        visible: text.length > 0
                        font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeCaption
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.mountPoint
                        visible: text.length > 0
                        font.pixelSize: Theme.fontSizeMicro
                        color: Theme.withAlpha(root.accent, 0.94)
                        elide: Text.ElideMiddle
                    }
                }

                InlineBadge {
                    visible: root.fileSystem.length > 0
                    text: root.fileSystem.toUpperCase()
                    fillColor: Theme.withAlpha(root.accent, themeController.isDark ? 0.18 : 0.12)
                    strokeColor: "transparent"
                    textColor: root.accent
                    horizontalPadding: 8
                    badgeHeight: 19
                    fontSize: 9
                    fontWeight: Font.Bold
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 7

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: Math.round(root.usage * 100) + "% used"
                        font.pixelSize: root.compact ? Theme.fontSizeCaption : Theme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: root.critical ? Theme.danger : Theme.textPrimary
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: root.freeText + (root.totalText.length > 0 ? " free of " + root.totalText : "")
                        font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeCaption
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                }

                LinearProgress {
                    Layout.fillWidth: true
                    value: Math.max(0, Math.min(1, root.usage))
                    trackColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.52 : 0.58)
                    fillColor: root.accent
                    preserveMinimumFill: true
                }
            }
        }
    }
}
