import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

Button {
        id: tabBtn

        property bool active: false

        Layout.fillWidth: true
        implicitHeight: 30
        leftPadding: 12
        rightPadding: 12
        topPadding: 0
        bottomPadding: 0

        background: Rectangle {
            radius: 7
            color: !tabBtn.active && tabBtn.hovered
                   ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.05 : 0.035)
                   : "transparent"
            border.color: "transparent"
            border.width: 1
        }

        contentItem: Label {
            text: tabBtn.text
            color: tabBtn.active ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            font.weight: tabBtn.active ? Font.DemiBold : Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }
    }

