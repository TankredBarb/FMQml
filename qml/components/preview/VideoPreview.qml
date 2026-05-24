import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string sourcePath: ""
    property int sourceSizeWidth: 2048
    property int sourceSizeHeight: 2048
    property string loadingText: "Loading video preview..."
    property bool showBusyIndicator: true

    readonly property int imageStatus: previewImage.imageStatus

    clip: true

    ImagePreview {
        id: previewImage
        anchors.fill: parent
        sourcePath: root.sourcePath
        fillMode: Image.PreserveAspectFit
        sourceSizeWidth: root.sourceSizeWidth
        sourceSizeHeight: root.sourceSizeHeight
        showOverlayIcon: true
        overlayIconSource: "qrc:/qt/qml/FM/qml/assets/icons/video.svg"
        overlayIconSize: 64
        showBusyIndicator: false
    }

    Rectangle {
        anchors.fill: parent
        visible: root.showBusyIndicator && previewImage.imageStatus === Image.Loading
        color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, themeController.isDark ? 0.58 : 0.64)

        Column {
            anchors.centerIn: parent
            spacing: 10
            width: Math.min(parent.width - 24, 240)

            BusyIndicator {
                running: true
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Label {
                text: root.loadingText
                color: Theme.textSecondary
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }
        }
    }
}
