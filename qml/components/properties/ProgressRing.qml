import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

Item {
        id: progressRing

        property real value: 0
        property bool running: false
        property color accentColor: Theme.accent
        property real displayedValue: 0

        implicitWidth: 18
        implicitHeight: 18

        Behavior on displayedValue {
            NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
        }

        Component.onCompleted: displayedValue = Math.max(0, Math.min(1, value))
        onValueChanged: displayedValue = Math.max(0, Math.min(1, value))
        onDisplayedValueChanged: {
            if (!running) {
                ringCanvas.requestPaint()
            }
        }
        onRunningChanged: ringCanvas.requestPaint()
        onAccentColorChanged: ringCanvas.requestPaint()
        onWidthChanged: ringCanvas.requestPaint()
        onHeightChanged: ringCanvas.requestPaint()

        RotationAnimator on rotation {
            from: 0
            to: 360
            duration: 1100
            loops: Animation.Infinite
            running: progressRing.running
        }

        Canvas {
            id: ringCanvas
            anchors.fill: parent
            antialiasing: true

            onPaint: {
                const ctx = getContext("2d")
                ctx.setTransform(1, 0, 0, 1, 0, 0)
                ctx.clearRect(0, 0, width, height)

                const size = Math.min(width, height)
                const center = size / 2
                const lineWidth = 2.4
                const radius = (size - lineWidth) / 2
                const start = -Math.PI / 2
                const progress = progressRing.running
                                 ? 0.34
                                 : Math.max(0, Math.min(1, progressRing.displayedValue))

                ctx.lineCap = "round"
                ctx.lineWidth = lineWidth
                ctx.strokeStyle = Theme.withAlpha(Theme.panelBorder, 0.76)
                ctx.beginPath()
                ctx.arc(center, center, radius, 0, Math.PI * 2)
                ctx.stroke()

                ctx.strokeStyle = progressRing.accentColor
                ctx.beginPath()
                ctx.arc(center, center, radius, start, start + Math.PI * 2 * progress)
                ctx.stroke()
            }
        }
    }

