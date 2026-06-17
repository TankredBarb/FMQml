import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
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
    property string displayedValue: value
    property bool animateValueChanges: true
    property bool componentReady: false
    property int valueMaximumLineCount: 2

    Layout.fillWidth: true
    radius: Theme.radiusSm
    color: rowMouse.containsMouse ? Theme.panelSurfaceSoft : Theme.panelSurface
    border.color: Theme.panelBorder
    border.width: 1
    implicitHeight: rowLayout.implicitHeight + 12

    Component.onCompleted: {
        displayedValue = value
        componentReady = true
    }

    onValueChanged: {
        if (!componentReady || !animateValueChanges || valueLabel.text.length === 0) {
            displayedValue = value
            return
        }
        valueChangeAnimation.restart()
    }

    SequentialAnimation {
        id: valueChangeAnimation

        NumberAnimation {
            target: valueLabel
            property: "opacity"
            to: 0.46
            duration: 70
            easing.type: Easing.OutCubic
        }

        ScriptAction {
            script: root.displayedValue = root.value
        }

        ParallelAnimation {
            NumberAnimation {
                target: valueLabel
                property: "opacity"
                to: 1.0
                duration: 130
                easing.type: Easing.OutCubic
            }

            NumberAnimation {
                target: valueLabel
                property: "scale"
                from: 0.985
                to: 1.0
                duration: 130
                easing.type: Easing.OutCubic
            }
        }
    }

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

        ThemedMenuSeparator {}

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

        RecolorSvgIcon {
            visible: root.iconSource.length > 0
            sourcePath: root.iconSource
            recolorEnabled: root.colorizeIcon
            recolorColor: root.accentColor
            sourceSize: Qt.size(28, 28)
            smooth: true
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
        }

        Label {
            visible: root.label.length > 0
            text: root.label
            Layout.preferredWidth: 100
            Layout.alignment: Qt.AlignTop
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeCaption
            font.weight: Font.Medium
            elide: Text.ElideRight
        }

        RowLayout {
            visible: root.showValue
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 6

            Label {
                id: valueLabel
                text: root.displayedValue.length > 0 ? root.displayedValue : "-"
                Layout.fillWidth: !root.showBusy
                Layout.alignment: Qt.AlignVCenter
                color: root.isLink ? Theme.accent : root.valueColor
                font.pixelSize: Theme.fontSizeLabel
                font.weight: root.emphasizeValue ? Font.DemiBold : Font.Normal
                elide: Text.ElideMiddle
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                maximumLineCount: root.valueMaximumLineCount
                transformOrigin: Item.Left
            }

            Item {
                id: busySpinner
                visible: root.showBusy
                rotation: 0

                Layout.preferredWidth: 13
                Layout.preferredHeight: 13
                Layout.alignment: Qt.AlignVCenter

                onVisibleChanged: {
                    if (visible) {
                        rotation = 0
                    }
                    spinnerCanvas.requestPaint()
                }

                RotationAnimator on rotation {
                    from: 0
                    to: 360
                    duration: 900
                    loops: Animation.Infinite
                    running: busySpinner.visible
                }

                Canvas {
                    id: spinnerCanvas
                    anchors.fill: parent
                    antialiasing: true
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.setTransform(1, 0, 0, 1, 0, 0)
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = root.accentColor
                        ctx.lineWidth = 1.8
                        ctx.lineCap = "round"
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, width / 2 - ctx.lineWidth, -Math.PI / 2, Math.PI * 1.15)
                        ctx.stroke()
                    }
                }
            }

            Item {
                visible: root.showBusy
                Layout.fillWidth: true
            }
        }
    }
}
