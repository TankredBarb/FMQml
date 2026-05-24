import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string iconSource: "qrc:/qt/qml/FM/qml/assets/icons/folder.svg"
    property string title: "Unsupported File"
    property string subtitle: "This item cannot be previewed here."
    property int iconSize: 40
    property int cardSize: 80
    property color accentColor: Theme.accent

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
            color: Theme.textPrimary
        }

        Label {
            visible: root.subtitle.length > 0
            text: root.subtitle
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: 11
            color: Theme.textSecondary
            opacity: 0.75
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
