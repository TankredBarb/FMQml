import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../../style"

Rectangle {
    id: root

    property string iconSource: ""
    property string title: ""
    property string subtitle: ""
    property string closeText: "x"
    property bool showCloseButton: true
    property bool nativeIconPresentation: false
    property color iconTint: Theme.accent
    property color accentColor: root.iconTint
    property color closeTint: Theme.textSecondary
    property color closeTintHover: Theme.textPrimary
    signal closeRequested()

    implicitHeight: 60
    color: "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 12

        Rectangle {
            Layout.preferredWidth: root.nativeIconPresentation ? 32 : 36
            Layout.preferredHeight: root.nativeIconPresentation ? 32 : 36
            radius: root.nativeIconPresentation ? 8 : Theme.radiusSm
            color: root.nativeIconPresentation
                   ? "transparent"
                   : Theme.withAlpha(root.accentColor, themeController.isDark ? 0.14 : 0.10)
            border.color: root.nativeIconPresentation
                          ? "transparent"
                          : Theme.withAlpha(root.accentColor, themeController.isDark ? 0.34 : 0.24)
            border.width: 1
            visible: root.iconSource.length > 0

            RecolorSvgIcon {
                anchors.centerIn: parent
                width: 24
                height: 24
                sourcePath: root.iconSource
                recolorEnabled: !root.nativeIconPresentation
                recolorColor: root.iconTint
                sourceSize: Qt.size(48, 48)
                smooth: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                text: root.title
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                Layout.fillWidth: true
                elide: Text.ElideMiddle
            }

            Label {
                visible: root.subtitle.length > 0
                text: root.subtitle
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.82 : 0.72)
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        Button {
            id: closeBtn
            visible: root.showCloseButton
            flat: true
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            onClicked: root.closeRequested()

            contentItem: Label {
                text: root.closeText
                font.pixelSize: Theme.fontSizeSubtitle
                color: closeBtn.hovered ? root.closeTintHover : root.closeTint
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: Theme.radiusMd
                color: closeBtn.pressed ? Theme.surfaceActive : (closeBtn.hovered ? Theme.panelSurfaceSoft : "transparent")
            }
        }
    }
}
