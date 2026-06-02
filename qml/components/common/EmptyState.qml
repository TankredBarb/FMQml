import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

ColumnLayout {
    id: root

    property string iconSource: ""
    property int iconSize: 42
    property string title: ""
    property string subtitle: ""
    property string hint: ""
    property real iconOpacity: 0.42
    property bool colorizeIcon: false
    property color iconColor: Theme.textSecondary
    property real contentOpacity: 1.0
    property int maxTextWidth: 260

    spacing: 12
    opacity: root.contentOpacity

    RecolorSvgIcon {
        visible: root.iconSource.length > 0
        sourcePath: root.iconSource
        recolorEnabled: root.colorizeIcon
        recolorColor: root.iconColor
        sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
        Layout.preferredWidth: root.iconSize
        Layout.preferredHeight: root.iconSize
        Layout.alignment: Qt.AlignHCenter
        opacity: root.iconOpacity
    }

    Label {
        visible: root.title.length > 0
        text: root.title
        Layout.alignment: Qt.AlignHCenter
        color: Theme.textPrimary
        font.pixelSize: 15
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
    }

    Label {
        visible: root.subtitle.length > 0
        text: root.subtitle
        Layout.alignment: Qt.AlignHCenter
        color: Theme.textSecondary
        font.pixelSize: 11
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        Layout.preferredWidth: root.maxTextWidth
    }

    InlineBadge {
        visible: root.hint.length > 0
        Layout.alignment: Qt.AlignHCenter
        text: root.hint
        textColor: Theme.textSecondary
        fillColor: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.10 : 0.08)
        strokeColor: Theme.border
        horizontalPadding: 18
        badgeHeight: 28
    }
}
