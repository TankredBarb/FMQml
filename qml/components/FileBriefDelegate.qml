import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "filepanel"

Item {
    id: root

    required property var    controller
    required property int    index
    required property string name
    required property string path
    required property bool   isDirectory
    required property bool   isSelected
    required property bool   isHidden
    required property bool   isImage
    required property bool   hasThumbnail
    required property string sizeText
    required property string suffix

    property bool currentItem:    false
    property bool panelActive:    true
    property bool scrolling:      false
    property bool isRenaming:     false
    property real visualOffsetX:  0
    z: root.isRenaming ? 100 : 0

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: 28

    // ── Dynamic Scaling ────────────────────────────────────────────────────────
    readonly property int   baseHeight: 28
    readonly property real  scaleFactor: height / baseHeight
    readonly property int   iconSize: Math.max(16, Math.min(48, Math.round(16 * scaleFactor)))
    readonly property int   fontSize: Math.max(11, Math.min(16, Math.round(12 * (1.0 + (scaleFactor - 1.0) * 0.5))))
    readonly property bool  canShowThumbnail: !isDirectory && hasThumbnail

    // ── Opacity for hidden files ───────────────────────────────────────────────
    opacity: isHidden ? 0.55 : 1.0

    // ── Type colour for the dot indicator ─────────────────────────────────────
    readonly property color dotColor: {
        if (isDirectory) return "#3b82f6"
        const s = suffix.toLowerCase()
        if (["jpg","jpeg","png","gif","bmp","webp","avif","heic","tiff","svg"].indexOf(s) >= 0) return "#10b981"
        if (["mp4","mov","avi","mkv","webm","wmv","flv","m4v"].indexOf(s) >= 0) return "#8b5cf6"
        if (["mp3","flac","wav","aac","ogg","m4a","opus"].indexOf(s) >= 0) return "#f59e0b"
        if (["zip","rar","7z","tar","gz","bz2","xz","cab"].indexOf(s) >= 0) return "#f97316"
        if (s === "pdf") return "#ef4444"
        if (["md","txt","doc","docx","rtf","odt"].indexOf(s) >= 0) return "#64748b"
        return Theme.panelBorder
    }

    // ── Reset on reuse ─────────────────────────────────────────────────────────
    onPathChanged: {
        isRenaming = false
        visualOffsetX = 0
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    Component.onCompleted: {
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    GridView.onPooled: {
        isRenaming = false
        visualOffsetX = 0
        if (root.controller.hoveredPath === root.path) {
            root.controller.hoveredPath = ""
        }
    }

    GridView.onReused: {
        isRenaming = false
        visualOffsetX = 0
        opacity = Qt.binding(() => isHidden ? 0.55 : 1.0)
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    function startRename() {
        root.isRenaming = true
    }

    // ── Background ─────────────────────────────────────────────────────────────
    FileItemStateLayer {
        selected: isSelected
        panelActive: root.panelActive
        currentItem: root.currentItem
        hovered: hover.hovered
        scrolling: root.scrolling
        visualOffsetX: root.visualOffsetX
        leftMargin: 3
        rightMargin: 3
        topMargin: 1
        bottomMargin: 0
        selectionBarLeftMargin: 2
        selectionBarTopMargin: 3
        selectionBarBottomMargin: 3
    }

    // ── Hover / mouse ──────────────────────────────────────────────────────────
    HoverHandler {
        id: hover
        enabled: true
        onHoveredChanged: {
            if (root.scrolling) return
            if (hovered) {
                root.controller.hoveredPath = root.path
            } else if (root.controller.hoveredPath === root.path) {
                root.controller.hoveredPath = ""
            }
        }
    }

    onScrollingChanged: {
        if (!scrolling) {
            Qt.callLater(() => {
                if (hover) {
                    hover.enabled = false
                    hover.enabled = true
                    if (hover.hovered) {
                        root.controller.hoveredPath = root.path
                    }
                }
            })
        }
    }

    Connections {
        target: root.controller ? root.controller.directoryModel : null
        ignoreUnknownSignals: true
        function onLoadingChanged() {
            if (root.controller && root.controller.directoryModel && !root.controller.directoryModel.loading) {
                Qt.callLater(() => {
                    if (hover) {
                        hover.enabled = false
                        hover.enabled = true
                        if (hover.hovered) {
                            root.controller.hoveredPath = root.path
                        }
                    }
                })
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: false

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) root.rightClicked()
            else root.clicked(mouse)
        }
        onDoubleClicked: root.doubleClicked()
    }

    // ── Rename overlay ─────────────────────────────────────────────────────────
    FileNameEditor {
        anchors.fill: parent
        anchors.leftMargin: 34
        anchors.rightMargin: 6
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        active: root.isRenaming
        name: root.name
        isDirectory: root.isDirectory
        index: root.index
        controller: root.controller
        fontPixelSize: 12
        onCancelRequested: root.isRenaming = false
        onCommitSucceeded: root.isRenaming = false
    }

    // ── Content row ────────────────────────────────────────────────────────────
    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.leftMargin:  8
        anchors.rightMargin: 6
        spacing: 5
        visible: !root.isRenaming
        transform: Translate { x: root.visualOffsetX }

        // Type dot
        Rectangle {
            width:  Math.max(4, Math.round(5 * (1.0 + (scaleFactor - 1.0) * 0.3)))
            height: width
            radius: width / 2
            color: root.dotColor
            Layout.alignment: Qt.AlignVCenter
            opacity: 0.85
        }

        // Icon or thumbnail
        Item {
            Layout.preferredWidth:  root.iconSize
            Layout.preferredHeight: root.iconSize
            Layout.alignment: Qt.AlignVCenter

            FileIconCell {
                anchors.fill: parent
                iconSource: "image://icon/" + encodeURIComponent(root.path + (root.isDirectory ? "?directory=true" : ""))
                thumbnailSource: root.canShowThumbnail ? "image://thumbnail/" + encodeURIComponent(root.path) : ""
                showThumbnail: root.canShowThumbnail
                iconSize: root.iconSize
            }
        }

        // File name
        Label {
            Layout.fillWidth: true
            text: root.name
            color: Theme.textPrimary
            font.pixelSize: root.fontSize
            font.weight: (isSelected || root.currentItem) ? Font.Medium : Font.Normal
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        // Size badge (files only)
        Label {
            text: root.sizeText
            color: Theme.textSecondary
            font.pixelSize: Math.max(9, root.fontSize - 2)
            opacity: 0.65
            Layout.preferredWidth: Math.max(52, 52 * scaleFactor * 0.7)
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            visible: !root.isDirectory && root.sizeText !== ""
            elide: Text.ElideRight
        }
    }
}
