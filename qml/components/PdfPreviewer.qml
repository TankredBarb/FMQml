import QtQuick
import QtQuick.Pdf
import QtQuick.Layouts
import QtQuick.Controls
import "../style"

Item {
    id: root

    property string sourcePath: ""
    property string pdfSourceUrl: root.sourcePath.length > 0
                                  ? ("file:///" + root.sourcePath.replace(/\\/g, "/"))
                                  : ""
    property int currentPage: 0
    property real zoomLevel: 1.0
    property real zoomStep: 0.12
    property real minimumZoom: 1.0
    property real maximumZoom: 4.0
    property real offsetX: 0.0
    property real offsetY: 0.0
    property bool compactControls: false

    readonly property bool ready: pdfDoc.status === PdfDocument.Ready
    readonly property bool loading: pdfDoc.status === PdfDocument.Loading
    readonly property bool error: pdfDoc.status === PdfDocument.Error
    readonly property string zoomPercentText: Math.round(root.zoomLevel * 100) + "%"
    readonly property real renderPixelRatio: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1.0
    readonly property real renderQualityScale: root.compactControls ? 2.0 : 2.5
    readonly property int minimumRenderSize: root.compactControls ? 900 : 1600
    readonly property int maximumRenderSize: root.compactControls ? 2400 : 4096
    readonly property int renderWidth: Math.min(root.maximumRenderSize,
                                                Math.max(root.minimumRenderSize,
                                                         Math.ceil(Math.max(root.width, 1) * root.renderPixelRatio * root.renderQualityScale)))
    readonly property int renderHeight: Math.min(root.maximumRenderSize,
                                                 Math.max(root.minimumRenderSize,
                                                          Math.ceil(Math.max(root.height, 1) * root.renderPixelRatio * root.renderQualityScale)))

    clip: true

    PdfDocument {
        id: pdfDoc
        source: root.pdfSourceUrl
    }

    function clampPage(page) {
        if (pdfDoc.pageCount <= 0) {
            return 0
        }
        return Math.max(0, Math.min(pdfDoc.pageCount - 1, page))
    }

    function clampZoom(value) {
        return Math.max(root.minimumZoom, Math.min(root.maximumZoom, value))
    }

    function clampOffsetX(value) {
        const limit = Math.max(0, (root.width * root.zoomLevel - root.width) / 2)
        return Math.max(-limit, Math.min(limit, value))
    }

    function clampOffsetY(value) {
        const limit = Math.max(0, (root.height * root.zoomLevel - root.height) / 2)
        return Math.max(-limit, Math.min(limit, value))
    }

    function resetView() {
        root.zoomLevel = 1.0
        root.offsetX = 0.0
        root.offsetY = 0.0
    }

    function applyZoom(nextZoom) {
        root.zoomLevel = root.clampZoom(nextZoom)
        root.offsetX = root.clampOffsetX(root.offsetX)
        root.offsetY = root.clampOffsetY(root.offsetY)
    }

    function goToPage(page) {
        root.currentPage = root.clampPage(page)
        root.resetView()
    }

    onSourcePathChanged: {
        root.currentPage = 0
        root.resetView()
    }

    onWidthChanged: root.offsetX = root.clampOffsetX(root.offsetX)
    onHeightChanged: root.offsetY = root.clampOffsetY(root.offsetY)

    Rectangle {
        anchors.fill: parent
        color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.035 : 0.025)
    }

    Item {
        id: viewport
        anchors.fill: parent
        clip: true

        Item {
            id: pageLayer
            width: viewport.width
            height: viewport.height
            x: root.offsetX
            y: root.offsetY
            visible: root.ready
            scale: root.zoomLevel
            transformOrigin: Item.Center

            Rectangle {
                anchors.centerIn: parent
                width: Math.max(1, pdfImage.paintedWidth)
                height: Math.max(1, pdfImage.paintedHeight)
                color: "#ffffff"
            }

            Image {
                id: pdfImage
                anchors.fill: parent
                source: root.ready ? root.pdfSourceUrl : ""
                currentFrame: root.currentPage
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                sourceSize: Qt.size(root.renderWidth, root.renderHeight)
                smooth: true
                opacity: root.ready ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 180 } }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.ready
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        preventStealing: true
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        property real pressX: 0.0
        property real pressY: 0.0
        property real startOffsetX: 0.0
        property real startOffsetY: 0.0

        onPressed: (mouse) => {
            pressX = mouse.x
            pressY = mouse.y
            startOffsetX = root.offsetX
            startOffsetY = root.offsetY
        }

        onPositionChanged: (mouse) => {
            if (!pressed) {
                return
            }
            root.offsetX = root.clampOffsetX(startOffsetX + (mouse.x - pressX))
            root.offsetY = root.clampOffsetY(startOffsetY + (mouse.y - pressY))
        }

        onWheel: (wheel) => {
            const delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
            if (delta === 0) {
                return
            }
            root.applyZoom(root.zoomLevel + (delta > 0 ? root.zoomStep : -root.zoomStep))
            wheel.accepted = true
        }

        onDoubleClicked: root.resetView()
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: root.loading
        visible: running
    }

    PdfPreviewFallback {
        anchors.centerIn: parent
        visible: root.error
        title: "Failed to load PDF"
        subtitle: "The document may be corrupted"
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.compactControls ? 10 : 16
        width: Math.max(1, Math.min(parent.width - (root.compactControls ? 12 : 32), root.compactControls ? 336 : 500))
        height: 42
        radius: Theme.radiusLg
        color: Theme.withAlpha(themeController.isDark ? Theme.surface : Theme.bg, 0.88)
        border.color: Theme.withAlpha(Theme.border, 0.85)
        border.width: 1
        visible: root.ready

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: root.compactControls ? 6 : 10
            anchors.rightMargin: root.compactControls ? 6 : 10
            spacing: root.compactControls ? 4 : 6

            PdfToolButton {
                text: "<"
                enabled: root.currentPage > 0
                onClicked: root.goToPage(root.currentPage - 1)
                ToolTip.visible: hovered
                ToolTip.text: "Previous page"
            }

            TextField {
                id: pageInput

                Layout.preferredWidth: root.compactControls ? 34 : 42
                Layout.preferredHeight: 28
                text: (root.currentPage + 1).toString()
                font.pixelSize: Theme.fontSizeCaption
                font.bold: true
                color: Theme.textPrimary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                selectByMouse: true
                validator: IntValidator {
                    bottom: 1
                    top: Math.max(1, pdfDoc.pageCount)
                }

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: pageInput.activeFocus
                           ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.18 : 0.10)
                           : "transparent"
                    border.color: pageInput.activeFocus ? Theme.accent : Theme.withAlpha(Theme.border, 0.35)
                    border.width: 1
                }

                onAccepted: {
                    const targetPage = parseInt(text) - 1
                    if (targetPage >= 0 && targetPage < pdfDoc.pageCount) {
                        root.goToPage(targetPage)
                    }
                    pageInput.focus = false
                }

                Connections {
                    target: root
                    function onCurrentPageChanged() {
                        if (!pageInput.activeFocus) {
                            pageInput.text = (root.currentPage + 1).toString()
                        }
                    }
                }
            }

            Label {
                text: "/ " + pdfDoc.pageCount
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
                Layout.preferredWidth: root.compactControls ? 32 : 44
                elide: Text.ElideRight
            }

            PdfToolButton {
                text: ">"
                enabled: root.currentPage < pdfDoc.pageCount - 1
                onClicked: root.goToPage(root.currentPage + 1)
                ToolTip.visible: hovered
                ToolTip.text: "Next page"
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 22
                color: Theme.withAlpha(Theme.border, 0.55)
            }

            PdfToolButton {
                text: "-"
                enabled: root.zoomLevel > root.minimumZoom
                onClicked: root.applyZoom(root.zoomLevel - root.zoomStep)
                ToolTip.visible: hovered
                ToolTip.text: "Zoom out"
            }

            PdfToolButton {
                text: "+"
                enabled: root.zoomLevel < root.maximumZoom
                onClicked: root.applyZoom(root.zoomLevel + root.zoomStep)
                ToolTip.visible: hovered
                ToolTip.text: "Zoom in"
            }

            PdfToolButton {
                text: "Fit"
                enabled: root.zoomLevel !== 1.0 || root.offsetX !== 0.0 || root.offsetY !== 0.0
                implicitWidth: root.compactControls ? 32 : 36
                onClicked: root.resetView()
                ToolTip.visible: hovered
                ToolTip.text: "Fit to view"
            }

            Label {
                text: root.zoomPercentText
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeMicro
                font.bold: true
                Layout.preferredWidth: 42
                horizontalAlignment: Text.AlignHCenter
                visible: !root.compactControls
            }
        }
    }

    component PdfToolButton: Button {
        id: controlButton

        implicitWidth: 28
        implicitHeight: 28
        padding: 0
        hoverEnabled: true

        contentItem: Label {
            text: controlButton.text
            color: controlButton.enabled ? Theme.accent : Theme.textSecondary
            opacity: controlButton.enabled ? 1.0 : 0.45
            font.pixelSize: Theme.fontSizeMicro
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: controlButton.down
                   ? Theme.surfaceActive
                   : (controlButton.hovered ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.20 : 0.14) : "transparent")
            border.color: controlButton.hovered ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.48 : 0.34) : "transparent"
            border.width: controlButton.hovered ? 1 : 0
        }
    }
}
