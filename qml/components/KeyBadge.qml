import QtQuick
import QtQuick.Controls
import "../style"

Label {
    id: root

    property color fillColor: Theme.withAlpha(Theme.secondaryAccent, themeController.isDark ? 0.16 : 0.12)
    property color borderColor: Theme.withAlpha(Theme.secondaryAccent, themeController.isDark ? 0.28 : 0.20)
    property color textColor: Theme.secondaryAccent
    property int badgeRadius: Theme.radiusSm
    property int horizontalPadding: 8
    property int verticalPadding: 2

    padding: 0
    leftPadding: horizontalPadding
    rightPadding: horizontalPadding
    topPadding: verticalPadding
    bottomPadding: verticalPadding
    color: textColor
    font.pixelSize: Theme.fontSizeMicro
    font.weight: Font.DemiBold
    font.letterSpacing: 0.4
    verticalAlignment: Text.AlignVCenter
    horizontalAlignment: Text.AlignHCenter

    background: Rectangle {
        radius: root.badgeRadius
        color: root.fillColor
        border.color: root.borderColor
        border.width: 1
    }
}
