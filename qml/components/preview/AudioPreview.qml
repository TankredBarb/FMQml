import QtQuick
import QtQuick.Controls
import "../../style"

Item {
    id: root

    property string path: ""

    clip: true

    ImagePreview {
        id: audioCoverArt
        anchors.fill: parent
        sourcePath: root.path
        fillMode: Image.PreserveAspectFit
        sourceSizeWidth: 2048
        sourceSizeHeight: 2048
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        visible: audioCoverArt.imageStatus !== Image.Ready

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.accent }
                GradientStop { position: 1.0; color: Qt.darker(Theme.accent, 1.6) }
            }
            opacity: 0.12
        }

        Image {
            anchors.centerIn: parent
            source: "qrc:/qt/qml/FM/qml/assets/icons/music.svg"
            sourceSize: Qt.size(64, 64)
            opacity: 0.75
        }
    }
}
