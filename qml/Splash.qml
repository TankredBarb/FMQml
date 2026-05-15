import QtQuick
import QtQuick.Window

Window {
    id: root

    width: 800
    height: 480
    visible: false
    flags: Qt.SplashScreen | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "#0D0D0D"
    title: "FM"

    property real sweep: 0.0
    property int statusIndex: 0
    readonly property var statuses: [
        "Loading workspace",
        "Preparing panels",
        "Building file model"
    ]

    Rectangle {
        anchors.fill: parent
        color: root.color
    }

    Rectangle {
        anchors.centerIn: parent
        width: 560
        height: 260
        radius: 24
        color: "#14161a"
        border.color: "#2a2f36"
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 4
            radius: 2
            color: "#00bcd4"
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 23
            color: "transparent"
            border.color: Qt.rgba(1, 1, 1, 0.03)
            border.width: 1
        }

        Column {
            anchors.fill: parent
            anchors.leftMargin: 44
            anchors.rightMargin: 44
            anchors.topMargin: 42
            anchors.bottomMargin: 34
            spacing: 22

            Row {
                spacing: 16

                Rectangle {
                    width: 68
                    height: 68
                    radius: 18
                    color: "#0f1114"
                    border.color: "#26303a"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "FM"
                        color: "white"
                        font.family: "Segoe UI"
                        font.pixelSize: 28
                        font.bold: true
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: "FM"
                        color: "white"
                        font.family: "Segoe UI"
                        font.pixelSize: 34
                        font.bold: true
                    }

                    Text {
                        text: "File manager"
                        color: "#8e949d"
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                    }
                }
            }

            Item {
                width: parent.width
                height: 28

                Rectangle {
                    width: parent.width
                    height: 2
                    anchors.bottom: parent.bottom
                    radius: 1
                    color: "#22282f"
                }

                Rectangle {
                    width: 92
                    height: 2
                    radius: 1
                    anchors.bottom: parent.bottom
                    color: "#00bcd4"
                    x: -92 + ((parent.width + 92) * root.sweep)
                }
            }

            Column {
                width: parent.width
                spacing: 10

                Text {
                    width: parent.width
                    text: root.statuses[root.statusIndex]
                    color: "#d5d8dd"
                    font.family: "Segoe UI"
                    font.pixelSize: 16
                    font.weight: Font.Medium
                }

                Text {
                    width: parent.width
                    text: "Initializing application shell"
                    color: "#7f8690"
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                }
            }
        }
    }

    NumberAnimation on sweep {
        from: 0.0
        to: 1.0
        duration: 1100
        loops: Animation.Infinite
        running: true
    }

    Timer {
        interval: 520
        repeat: true
        running: true
        onTriggered: root.statusIndex = (root.statusIndex + 1) % root.statuses.length
    }
}
