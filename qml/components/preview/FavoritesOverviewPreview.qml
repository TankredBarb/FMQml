import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"

Item {
    id: root

    property bool compact: false
    property var favorites: typeof favoritesController !== "undefined" ? favoritesController : null

    readonly property color accent: Theme.actionIconColor("favorite")
    readonly property int pinnedCount: favorites ? favorites.pinnedCount : 0
    readonly property int frequentCount: favorites ? favorites.frequentCount : 0
    readonly property int tagCount: favorites ? favorites.tagCount : 0

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.compact ? 8 : 18
        radius: Theme.panelRadius
        color: Theme.withAlpha(root.accent, themeController.isDark ? 0.12 : 0.08)
        border.color: Theme.withAlpha(root.accent, themeController.isDark ? 0.32 : 0.22)
        border.width: 1
        clip: true

        Rectangle {
            width: parent.width * 0.75
            height: width
            radius: width / 2
            x: parent.width * 0.55
            y: -height * 0.46
            color: Theme.withAlpha(root.accent, themeController.isDark ? 0.15 : 0.10)
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.compact ? 12 : 22
            spacing: root.compact ? 10 : 16

            RowLayout {
                Layout.fillWidth: true
                spacing: root.compact ? 10 : 14

                IconTile {
                    Layout.preferredWidth: root.compact ? 44 : 62
                    Layout.preferredHeight: width
                    tileSize: width
                    iconSize: root.compact ? 23 : 31
                    cornerRadius: Theme.radiusLg
                    source: "qrc:/qt/qml/FM/qml/assets/icons/star.svg"
                    iconColor: root.accent
                    tileColor: Theme.withAlpha(root.accent, themeController.isDark ? 0.20 : 0.14)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        Layout.fillWidth: true
                        text: "Favorites"
                        font.pixelSize: root.compact ? Theme.fontSizeBody : Theme.scaledSize(23)
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.pinnedCount > 0 ? "Your saved places and recent work" : "Pin places to keep them close"
                        font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeCaption
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: root.compact ? 7 : 10

                Repeater {
                    model: [
                        { value: root.pinnedCount, label: "Pinned" },
                        { value: root.frequentCount, label: "Frequent" },
                        { value: root.tagCount, label: "Tags" }
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.compact ? 58 : 72
                        radius: Theme.radiusMd
                        color: Theme.withAlpha(root.accent, themeController.isDark ? 0.16 : 0.11)
                        border.color: Theme.withAlpha(root.accent, themeController.isDark ? 0.25 : 0.18)
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 1

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.value
                                font.pixelSize: root.compact ? Theme.fontSizeBody : Theme.scaledSize(20)
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                font.pixelSize: Theme.fontSizeMicro
                                color: Theme.textSecondary
                            }
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.pinnedCount === 0 && root.frequentCount === 0
                      ? "Pinned items and frequently used local locations appear here."
                      : "Open Favorites to manage pinned items, recent locations, and tags."
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeCaption
                color: Theme.textSecondary
            }
        }
    }
}
