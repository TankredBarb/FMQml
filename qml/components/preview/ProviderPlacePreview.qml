import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"

Item {
    id: root

    property string providerId: ""
    property string title: "Cloud provider"
    property string accountText: ""
    property string statusText: ""
    property string contentsText: ""
    property string accessText: ""
    property bool compact: false

    readonly property color accent: providerId === "telegram" ? Theme.actionIconColor("network")
                                            : providerId === "mega" ? Theme.danger
                                                                    : Theme.actionIconColor("storage")
    readonly property string iconSource: "qrc:/qt/qml/FM/qml/assets/filetypes-next/"
                                         + providerId + ".svg"

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
                    Layout.preferredWidth: root.compact ? 52 : 68
                    Layout.preferredHeight: width
                    tileSize: width
                    iconSize: root.compact ? 30 : 40
                    cornerRadius: Theme.radiusLg
                    source: root.iconSource
                    useOriginalColor: true
                    tileColor: Theme.withAlpha(Theme.bg, themeController.isDark ? 0.30 : 0.46)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

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
                        text: root.accountText.length > 0 ? root.accountText : root.statusText
                        visible: text.length > 0
                        font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeCaption
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                }

                InlineBadge {
                    text: "PROVIDER"
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
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: root.contentsText
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: root.compact ? Theme.fontSizeCaption : Theme.fontSizeBody
                    color: Theme.textPrimary
                }

                Label {
                    Layout.fillWidth: true
                    text: root.accessText
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeCaption
                    color: Theme.textSecondary
                }
            }
        }
    }
}
