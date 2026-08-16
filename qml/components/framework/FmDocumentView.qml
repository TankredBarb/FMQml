import QtQuick
import QtQuick.Controls
import FM
import "../../style"

Item {
    id: root

    property alias text: renderer.text
    property alias fontFamily: renderer.fontFamily
    property alias fontPixelSize: renderer.fontPixelSize
    property alias styleRanges: renderer.styleRanges
    property alias wrap: renderer.wrap
    property alias showLineNumbers: renderer.showLineNumbers
    property alias firstLine: renderer.firstLine
    property alias textPadding: renderer.textPadding
    property alias lineNumberWidth: renderer.lineNumberWidth
    readonly property alias selectedText: renderer.selectedText
    readonly property alias hasSelection: renderer.hasSelection
    readonly property alias documentWidth: renderer.documentWidth
    readonly property alias documentHeight: renderer.documentHeight
    property color documentColor: themeController.isDark
                                  ? Theme.opaque(Theme.mixColors(Theme.bg, Theme.panelSurfaceStrong, 0.72))
                                  : "#ffffff"
    property color textColor: themeController.isDark ? Theme.textPrimary : "#202124"
    property color lineNumberColor: themeController.isDark
                                    ? Theme.withAlpha(Theme.textSecondary, 0.72) : "#667085"
    property color selectionColor: themeController.isDark
                                   ? Theme.withAlpha(Theme.accent, 0.42) : "#b8d8ff"
    property color selectedTextColor: themeController.isDark ? Theme.textPrimary : "#111827"
    property color gutterColor: themeController.isDark
                                ? Theme.opaque(Theme.mixColors(Theme.bg, Theme.panelSurfaceStrong, 0.88))
                                : "#f4f5f7"
    property color dividerColor: themeController.isDark
                                 ? Theme.withAlpha(Theme.panelBorder, 0.42) : "#d9dee7"
    property color tokenColor1: textColor
    property color tokenColor2: textColor
    property color tokenColor3: textColor
    property color tokenColor4: textColor

    function copySelection() { renderer.copySelection() }
    function clearSelection() { renderer.clearSelection() }

    Rectangle {
        anchors.fill: parent
        color: root.documentColor
    }

    WheelHandler {
        target: null
        blocking: true
        onWheel: (event) => {
            const horizontal = (event.modifiers & Qt.ShiftModifier) !== 0
                               || Math.abs(event.angleDelta.x) > Math.abs(event.angleDelta.y)
                               || Math.abs(event.pixelDelta.x) > Math.abs(event.pixelDelta.y)
            const pixelDelta = horizontal && event.pixelDelta.x !== 0
                             ? event.pixelDelta.x : event.pixelDelta.y
            const angleDelta = horizontal && event.angleDelta.x !== 0
                             ? event.angleDelta.x : event.angleDelta.y
            const rawDelta = pixelDelta !== 0 ? pixelDelta : angleDelta
            if (rawDelta === 0)
                return

            const directedDelta = (event.inverted ? rawDelta : -rawDelta) * 0.4
            if (horizontal) {
                const maximumX = Math.max(0, viewport.contentWidth - viewport.width)
                viewport.contentX = Math.max(0, Math.min(maximumX,
                                                        viewport.contentX + directedDelta))
            } else {
                const maximumY = Math.max(0, viewport.contentHeight - viewport.height)
                viewport.contentY = Math.max(0, Math.min(maximumY,
                                                        viewport.contentY + directedDelta))
            }
            event.accepted = true
        }
    }

    Flickable {
        id: viewport
        anchors.fill: parent
        clip: true
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        bottomMargin: horizontalScrollBar.scrollNeeded
                      ? horizontalScrollBar.implicitHeight : 0
        contentWidth: Math.max(width, renderer.documentWidth)
        contentHeight: Math.max(height, renderer.documentHeight)

        FmDocumentViewVisual {
            id: renderer
            x: viewport.contentX
            y: viewport.contentY
            width: viewport.width
            height: viewport.height - (horizontalScrollBar.scrollNeeded
                                       ? horizontalScrollBar.implicitHeight : 0)
            contentX: viewport.contentX
            contentY: viewport.contentY
            textColor: root.textColor
            lineNumberColor: root.lineNumberColor
            selectionColor: root.selectionColor
            selectedTextColor: root.selectedTextColor
            gutterColor: root.gutterColor
            dividerColor: root.dividerColor
            tokenColor1: root.tokenColor1
            tokenColor2: root.tokenColor2
            tokenColor3: root.tokenColor3
            tokenColor4: root.tokenColor4
        }

        ScrollBar.vertical: FmScrollBar {
            id: verticalScrollBar
            policy: ScrollBar.AsNeeded
            wheelTarget: viewport
            trailingInset: horizontalScrollBar.scrollNeeded
                            ? horizontalScrollBar.implicitHeight : 0
        }
        ScrollBar.horizontal: FmScrollBar {
            id: horizontalScrollBar
            policy: renderer.wrap ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
            wheelTarget: viewport
            trailingInset: verticalScrollBar.scrollNeeded
                            ? verticalScrollBar.implicitWidth : 0
        }
    }
}
