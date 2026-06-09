import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string iconSource: "qrc:/qt/qml/FM/qml/assets/icons/document.svg"
    property string fallbackIconSource: ""
    property string title: "File"
    property string typeText: "Unsupported File"
    property string sizeText: ""
    property string modifiedText: ""
    property string locationText: ""
    property string extension: ""
    property string statusText: "Preview unavailable"
    property bool compact: false
    property color accentColor: Theme.accent

    readonly property string formatText: extension.length > 0 ? extension.toUpperCase() : "FILE"
    readonly property string metaText: {
        if (sizeText.length > 0 && modifiedText.length > 0) return sizeText + "  |  " + modifiedText
        if (sizeText.length > 0) return sizeText
        if (modifiedText.length > 0) return modifiedText
        return "Preview unavailable"
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
                color: Theme.withAlpha(Theme.bg, themeController.isDark ? 0.30 : 0.46)
                border.color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.38 : 0.28)
                border.width: 1

                Item {
                    anchors.centerIn: parent
                    width: root.compact ? 34 : 66
                    height: width

                    Image {
                        id: primaryIcon
                        anchors.fill: parent
                        source: root.iconSource
                        sourceSize: Qt.size(parent.width, parent.height)
                        opacity: 0.92
                        smooth: true
                        visible: root.iconSource.length > 0 && status !== Image.Error
                    }

                    Image {
                        anchors.fill: parent
                        source: root.fallbackIconSource
                        sourceSize: Qt.size(parent.width, parent.height)
                        opacity: 0.92
                        smooth: true
                        visible: root.fallbackIconSource.length > 0 && (root.iconSource.length === 0 || primaryIcon.status === Image.Error)
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: root.compact ? 5 : 7
                    width: formatLabel.implicitWidth + 12
                    height: root.compact ? 18 : 20
                    radius: Theme.radiusSm
                    color: Theme.withAlpha(Theme.bg, themeController.isDark ? 0.72 : 0.82)

                    Label {
                        id: formatLabel
                        anchors.centerIn: parent
                        text: root.formatText
                        font.pixelSize: root.compact ? 8 : 9
                        font.bold: true
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: root.compact ? 4 : 7

                Label {
                    Layout.fillWidth: true
                    text: root.title.length > 0 ? root.title : "File"
                    font.pixelSize: root.compact ? 14 : 23
                    font.bold: true
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: root.typeText.length > 0 ? root.typeText : "Unsupported File"
                    font.pixelSize: root.compact ? 11 : 13
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: root.metaText
                    font.pixelSize: root.compact ? 10 : 11
                    color: Theme.textSecondary
                    opacity: 0.84
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.locationText.length > 0 && !root.compact
                    text: root.locationText
                    font.pixelSize: 10
                    color: Theme.textSecondary
                    opacity: 0.74
                    elide: Text.ElideMiddle
                }

                Rectangle {
                    Layout.preferredWidth: Math.min(parent.width, statusLabel.implicitWidth + 18)
                    Layout.maximumWidth: parent.width
                    Layout.preferredHeight: root.compact ? 22 : 24
                    radius: Theme.radiusSm
                    color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.13 : 0.10)
                    border.color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.32 : 0.24)
                    border.width: 1

                    Label {
                        id: statusLabel
                        anchors.fill: parent
                        anchors.leftMargin: 9
                        anchors.rightMargin: 9
                        text: root.statusText.length > 0 ? root.statusText : "Preview unavailable"
                        font.pixelSize: root.compact ? 9 : 10
                        font.bold: true
                        color: root.accentColor
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
