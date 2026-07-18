import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Rectangle {
    id: row

    property string title: ""
    property string subtitle: ""
    property bool checked: false
    property bool toggleEnabled: true
    property color accentColor: Theme.accent
    readonly property color titleColor: Theme.textPrimary
    readonly property color subtitleColor: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.74 : 0.82)
    signal toggled(bool checked)

    Layout.fillWidth: true
    implicitHeight: Math.max(48, rowLayout.implicitHeight + 10)
    radius: Theme.radiusSm
    color: rowMouse.containsMouse
           ? Theme.withAlpha(Theme.surfaceHover, themeController.isDark ? 0.42 : 0.58)
           : Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.30 : 0.52)
    border.color: Theme.withAlpha(row.accentColor,
                                  row.checked
                                  ? (themeController.isDark ? 0.40 : 0.34)
                                  : (themeController.isDark ? 0.26 : 0.24))
    border.width: 1

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 7
        width: 2
        radius: 1
        opacity: row.checked ? 1.0 : 0.58
        color: Theme.withAlpha(row.accentColor, themeController.isDark ? 0.86 : 0.72)
    }

    RowLayout {
        id: rowLayout
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 10

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                text: row.title
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
                color: row.titleColor
                elide: Text.ElideRight
            }

            Label {
                text: row.subtitle
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: row.subtitleColor
            }
        }

        Switch {
            id: switchControl
            checked: row.checked
            enabled: row.toggleEnabled
            Layout.preferredWidth: 46
            Layout.preferredHeight: 26

            indicator: Rectangle {
                implicitWidth: 40
                implicitHeight: 22
                x: switchControl.leftPadding
                y: parent.height / 2 - height / 2
                radius: height / 2
                color: switchControl.checked
                       ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.34 : 0.22)
                       : Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.82 : 0.92)
                border.color: switchControl.checked
                              ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.62 : 0.44)
                              : Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.26 : 0.20)
                border.width: 1
                opacity: row.toggleEnabled ? 1.0 : 0.62

                Rectangle {
                    x: switchControl.checked ? parent.width - width - 3 : 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 15
                    height: 15
                    radius: 7.5
                    color: switchControl.checked ? row.accentColor : Theme.withAlpha(Theme.textSecondary, 0.78)
                    Behavior on x { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                }
            }

            contentItem: Item {}
        }
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        enabled: row.toggleEnabled
        cursorShape: row.toggleEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: row.toggled(!row.checked)
    }
}
