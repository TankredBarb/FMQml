import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "../../style"

Control {
    id: root

    property string title: ""
    property string subtitle: ""
    property bool checked: false
    property bool toggleEnabled: true
    property color accentColor: Theme.accent
    property color titleColor: Theme.textPrimary
    property color subtitleColor: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.74 : 0.82)
    signal toggled(bool checked)

    Layout.fillWidth: true
    implicitHeight: Math.max(48, contentLayout.implicitHeight + 16)
    enabled: root.toggleEnabled
    hoverEnabled: true
    padding: 0

    background: FmToggleRowVisual {
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        checked: root.checked
        hovered: root.hovered
        pressed: rowMouse.pressed
        radius: Theme.radiusSm
        surfaceColor: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.30 : 0.52)
        hoverColor: Theme.withAlpha(Theme.surfaceHover, themeController.isDark ? 0.42 : 0.58)
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.62 : 0.72)
        accentColor: root.accentColor
    }

    contentItem: RowLayout {
        id: contentLayout
        spacing: 10

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            spacing: 2

            Label {
                text: root.title
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
                color: root.titleColor
                elide: Text.ElideRight
            }

            Label {
                text: root.subtitle
                visible: text.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: root.subtitleColor
            }
        }

        FmSwitch {
            checked: root.checked
            enabled: root.enabled
            accentColor: root.accentColor
            Layout.preferredWidth: 46
            Layout.preferredHeight: 26
            Layout.rightMargin: 10
        }
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.toggled(!root.checked)
    }
}
