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
            radius: 16
            color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.15)

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
            font.pixelSize: 13
            color: Theme.textPrimary
        }

        Label {
            text: root.subtitle
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: 11
            color: Theme.textSecondary
            opacity: 0.7
        }
    }
}
