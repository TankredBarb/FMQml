import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string iconSource: "qrc:/qt/qml/FM/qml/assets/icons/folder.svg"
    property string fallbackIconSource: ""
    property string title: "Folder"
    property string sizeText: ""
    property string modifiedText: ""
    property string locationText: ""
    property bool compact: false
    property color accentColor: Theme.secondaryAccent
    property color iconAccentColor: Theme.accent

    readonly property string metaText: {
        if (modifiedText.length > 0) return modifiedText
        if (sizeText.length > 0) return sizeText
        return "Folder"
    }

    clip: true

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.compact ? 8 : 18
        radius: Theme.panelRadius
        color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.11 : 0.08)
        border.color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.30 : 0.22)
        border.width: 1
        clip: true

        RowLayout {
            anchors.fill: parent
            anchors.margins: root.compact ? 10 : 20
            spacing: root.compact ? 10 : 18

            Rectangle {
                Layout.preferredWidth: root.compact ? 58 : 112
                Layout.preferredHeight: width
                radius: Theme.radiusLg
                color: Theme.withAlpha(root.iconAccentColor, themeController.isDark ? 0.16 : 0.12)
                border.color: Theme.withAlpha(root.iconAccentColor, themeController.isDark ? 0.42 : 0.30)
                border.width: 1

                Item {
                    anchors.centerIn: parent
                    width: root.compact ? 36 : 70
                    height: width

                    Image {
                        id: primaryIcon
                        anchors.fill: parent
                        source: root.iconSource
                        sourceSize: Qt.size(parent.width, parent.height)
                        opacity: 0.94
                        smooth: true
                        visible: root.iconSource.length > 0 && status !== Image.Error
                    }

                    Image {
                        anchors.fill: parent
                        source: root.fallbackIconSource
                        sourceSize: Qt.size(parent.width, parent.height)
                        opacity: 0.94
                        smooth: true
                        visible: root.fallbackIconSource.length > 0 && (root.iconSource.length === 0 || primaryIcon.status === Image.Error)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: root.compact ? 4 : 7

                Label {
                    Layout.fillWidth: true
                    text: root.title.length > 0 ? root.title : "Folder"
                    font.pixelSize: root.compact ? 14 : 23
                    font.bold: true
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: "Folder"
                    font.pixelSize: root.compact ? 11 : 13
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: root.metaText
                    font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeCaption
                    color: Theme.textSecondary
                    opacity: 0.84
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.locationText.length > 0 && !root.compact
                    text: root.locationText
                    font.pixelSize: Theme.fontSizeMicro
                    color: Theme.textSecondary
                    opacity: 0.74
                    elide: Text.ElideMiddle
                }

                Rectangle {
                    Layout.preferredWidth: statusLabel.implicitWidth + 18
                    Layout.preferredHeight: root.compact ? 22 : 24
                    radius: Theme.radiusSm
                    color: Theme.withAlpha(root.iconAccentColor, themeController.isDark ? 0.13 : 0.10)
                    border.color: Theme.withAlpha(root.iconAccentColor, themeController.isDark ? 0.32 : 0.24)
                    border.width: 1

                    Label {
                        id: statusLabel
                        anchors.centerIn: parent
                        text: "Folder information"
                        font.pixelSize: root.compact ? Theme.scaledSize(9) : Theme.fontSizeMicro
                        font.bold: true
                        color: root.iconAccentColor
                    }
                }
            }
        }
    }
}
