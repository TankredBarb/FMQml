import QtQuick
import QtQuick.Controls
import "../../style"

Button {
    id: root

    property color primaryColor: Theme.accent
    property color primaryHoverColor: Theme.accent
    property color primaryPressedColor: Theme.accent
    property color textColor: Theme.accentText
    property color secondaryTextColor: Theme.textSecondary
    property bool enforceTextContrast: true
    readonly property color effectiveTextColor: enforceTextContrast
                                               ? Theme.readableOn(root.primaryColor, root.textColor)
                                               : root.textColor

    contentItem: Label {
        text: root.text
        font.pixelSize: 12
        font.weight: root.highlighted ? Font.Medium : Font.Normal
        color: root.enabled ? (root.highlighted ? root.effectiveTextColor : root.secondaryTextColor) : Theme.textSecondary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 34
        radius: Theme.radiusSm
        color: !root.enabled ? Theme.panelBorder
             : root.highlighted
               ? (root.pressed ? root.primaryPressedColor : (root.hovered ? root.primaryHoverColor : root.primaryColor))
               : (root.pressed ? Theme.surfaceActive : (root.hovered ? Theme.panelSurfaceSoft : "transparent"))

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: root.highlighted && root.enabled
            color: Theme.withAlpha(root.effectiveTextColor,
                                   root.pressed ? 0.18
                                                : (root.hovered ? 0.10 : 0.0))
        }
    }
}
