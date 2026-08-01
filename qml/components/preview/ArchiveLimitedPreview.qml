import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/archive.svg"
    property string fallbackIconSource: ""
    property bool compact: true

    clip: true

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, root.compact ? 280 : 360)
        spacing: root.compact ? 10 : 14

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: root.compact ? 70 : 88
            height: width
            radius: Theme.radiusForSide(Math.min(width, height))
            color: Theme.withAlpha(Theme.secondaryAccent, themeController.isDark ? 0.18 : 0.12)
            border.color: Theme.withAlpha(Theme.secondaryAccent, 0.35)
            border.width: 1

            Item {
                anchors.centerIn: parent
                width: root.compact ? 36 : 46
                height: width

                Image {
                    id: primaryIcon
                    anchors.fill: parent
                    source: root.iconSource
                    sourceSize: Qt.size(parent.width, parent.height)
                    smooth: true
                    opacity: 0.9
                    visible: root.iconSource.length > 0 && status !== Image.Error
                }

                Image {
                    anchors.fill: parent
                    source: root.fallbackIconSource
                    sourceSize: Qt.size(parent.width, parent.height)
                    smooth: true
                    opacity: 0.9
                    visible: root.fallbackIconSource.length > 0 && (root.iconSource.length === 0 || primaryIcon.status === Image.Error)
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Archive Preview Limited"
            font.bold: true
            font.pixelSize: root.compact ? 14 : 17
            color: Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: "Inside archives, QuickLook and preview only handle small lightweight files."
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            font.pixelSize: root.compact ? 11 : 12
            color: Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
