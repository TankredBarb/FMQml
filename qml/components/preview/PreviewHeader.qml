import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../framework"
import "../../style"

Rectangle {
    id: root

    property string iconSource: ""
    property string fallbackIconSource: ""
    property string title: ""
    property string subtitle: ""
    property string closeIconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/close.svg"
    property color closeIconTint: Theme.withAlpha(Theme.actionIconColor("close"), 0.78)
    property color closeIconTintHover: Theme.actionIconColor("close")
    property bool liveResizeActive: false

    signal closeRequested()

    implicitHeight: root.subtitle.indexOf("\n") >= 0 ? 66 : 54
    color: "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 12
        spacing: 12

        Item {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24

            Image {
                id: primaryIcon
                anchors.fill: parent
                source: root.iconSource
                sourceSize: Qt.size(24, 24)
                visible: root.iconSource.length > 0 && status !== Image.Error
            }

            Image {
                anchors.fill: parent
                source: root.fallbackIconSource
                sourceSize: Qt.size(24, 24)
                visible: root.fallbackIconSource.length > 0 && (root.iconSource.length === 0 || primaryIcon.status === Image.Error)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: -2

            Label {
                text: root.title
                font.bold: true
                font.pixelSize: Theme.scaledSize(15)
                color: Theme.textPrimary
                Layout.fillWidth: true
                elide: Text.ElideMiddle
            }

            Label {
                text: root.subtitle
                font.pixelSize: Theme.fontSizeMicro
                color: Theme.textSecondary
                opacity: 0.7
                Layout.fillWidth: true
                maximumLineCount: root.subtitle.indexOf("\n") >= 0 ? 2 : 1
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }
        }

        FmIconButton {
            id: closeBtn
            onClicked: root.closeRequested()
            iconSource: root.closeIconSource
            iconSize: 18
            svgRecolorColor: closeBtn.hovered ? root.closeIconTintHover : root.closeIconTint
        }
    }
}
