import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string title: "PDF Document"
    property string subtitle: "No system preview available"
    property string iconSource: "qrc:/qt/qml/FM/qml/assets/icons/document.svg"
    property int iconSize: 40
    property int cardSize: 80
    property color accentColor: Theme.danger

    clip: true

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: root.cardSize
            height: root.cardSize
            radius: Theme.radiusForSide(Math.min(width, height))
            color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.18 : 0.12)
            border.color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.34 : 0.24)
            border.width: 1

            Image {
                anchors.centerIn: parent
                source: root.iconSource
                sourceSize: Qt.size(root.iconSize, root.iconSize)
                opacity: 0.8
            }
        }

        Label {
            text: root.title
            Layout.alignment: Qt.AlignHCenter
            font.bold: true
            font.pixelSize: Theme.fontSizeBody
            color: Theme.textPrimary
        }

        Label {
            text: root.subtitle
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: Theme.fontSizeCaption
            color: Theme.textSecondary
            opacity: 0.7
        }
    }
}
