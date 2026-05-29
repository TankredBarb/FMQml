import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import ".."

Rectangle {
    id: root

    property string label: ""
    property string value: ""
    property string iconSource: ""
    property bool isLink: false
    property bool emphasizeValue: false
    property bool showValue: value.length > 0
    property color valueColor: Theme.textPrimary
    property color accentColor: Theme.accent
    property bool showBusy: false
    property bool colorizeIcon: true

    Layout.fillWidth: true
    radius: Theme.radiusSm
    color: rowMouse.containsMouse ? Theme.panelSurfaceSoft : Theme.panelSurface
    border.color: Theme.panelBorder
    border.width: 1
    implicitHeight: rowLayout.implicitHeight + 12

    ThemedContextMenu {
        id: rowContextMenu
        implicitWidth: 180

        ThemedMenuItem {
            text: "Copy Value"
            enabled: root.value.length > 0
            onClicked: {
                if (typeof workspaceController !== "undefined" && workspaceController) {
                    workspaceController.copyTextToClipboard(root.value)
                    rowTooltip.show("Value copied")
                }
            }
        }

        ThemedMenuItem {
            text: "Copy Label"
            enabled: root.label.length > 0
            onClicked: {
                if (typeof workspaceController !== "undefined" && workspaceController) {
                    workspaceController.copyTextToClipboard(root.label)
                    rowTooltip.show("Label copied")
                }
            }
        }

        ThemedMenuItem {
            text: "Copy Row"
            enabled: root.label.length > 0 && root.value.length > 0
            onClicked: {
                if (typeof workspaceController !== "undefined" && workspaceController) {
                    workspaceController.copyTextToClipboard(root.label + ": " + root.value)
                    rowTooltip.show("Row copied")
                }
            }
        }
    }

    ToolTip {
        id: rowTooltip
        text: ""
        timeout: 1500
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onDoubleClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton && root.value.length > 0 && typeof workspaceController !== "undefined" && workspaceController) {
                workspaceController.copyTextToClipboard(root.value)
                rowTooltip.show("Copied: " + root.value)
            }
        }
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                rowContextMenu.popup()
            }
        }
    }

    RowLayout {
        id: rowLayout
        anchors.fill: parent
        anchors.margins: 6
        spacing: 8

        Image {
            visible: root.iconSource.length > 0
            source: root.iconSource
            sourceSize: Qt.size(14, 14)
            smooth: true
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            layer.enabled: root.colorizeIcon
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: root.accentColor
            }
        }

        Label {
            visible: root.label.length > 0
            text: root.label
            Layout.preferredWidth: 100
            Layout.alignment: Qt.AlignTop
            color: Theme.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            elide: Text.ElideRight
        }

        RowLayout {
            visible: root.showValue
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 6

            Label {
                text: root.value.length > 0 ? root.value : "-"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                color: root.isLink ? Theme.accent : root.valueColor
                font.pixelSize: 12
                font.weight: root.emphasizeValue ? Font.DemiBold : Font.Normal
                elide: Text.ElideMiddle
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                maximumLineCount: 2
            }

            BusyIndicator {
                id: busyIndicator
                visible: root.showBusy
                running: root.showBusy
                Layout.preferredWidth: 12
                Layout.preferredHeight: 12
                Layout.alignment: Qt.AlignVCenter

                contentItem: Canvas {
                    anchors.fill: parent
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = Theme.accent
                        ctx.lineWidth = 1.5
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, width / 2 - ctx.lineWidth, 0, Math.PI * 1.5)
                        ctx.stroke()
                    }

                    RotationAnimator on rotation {
                        from: 0
                        to: 360
                        duration: 1000
                        loops: Animation.Infinite
                        running: busyIndicator.running
                    }
                }
            }
        }
    }
}
