import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "filepanel"

Item {
    id: root

    required property var    controller
    property var panel
    required property int    index
    required property string name
    required property string path
    required property string iconName
    required property bool   isDirectory
    required property bool   isSelected
    required property bool   isHidden
    required property bool   isArchiveFile
    required property bool   isIsoImageFile
    required property bool   isImage
    required property bool   hasThumbnail
    required property string sizeText
    required property string suffix

    property bool currentItem:    false
    property bool panelActive:    true
    property bool scrolling:      false
    property bool resizeOptimized: false
    property bool thumbnailSchedulingPaused: false
    property bool thumbnailLoadingPaused: false
    property bool isRenaming:     false
    property real visualOffsetX:  0
    z: root.isRenaming ? 100 : 0

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: root.panel ? root.panel.briefRowHeight : Math.max(Theme.controlHeight - 10, Theme.fontSizeLabel + 16)

    readonly property real  scaleFactor: implicitHeight > 0 ? height / implicitHeight : 1.0
    readonly property int   iconSize: Math.max(Theme.scaledSize(16),
                                                Math.min(Theme.scaledSize(24),
                                                         Math.round(Math.max(Theme.scaledSize(16), height - Theme.scaledSize(10)))))
    readonly property int   nameFontSize: Theme.fontSizeBody
    readonly property int   metaFontSize: Theme.fontSizeCaption
    readonly property bool  canShowThumbnail: !isDirectory && hasThumbnail
    readonly property bool  thumbnailEligible: root.canShowThumbnail
                                           && !root.thumbnailLoadingPaused
                                           && (root.panel ? root.panel.effectiveUseNativeIcons : (typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true))
                                           && (root.panel ? root.panel.effectiveShowThumbnails
                                                          : ((typeof appSettings !== "undefined" && appSettings ? appSettings.showThumbnails : true)
                                                             && !(typeof appSettings !== "undefined" && appSettings ? appSettings.ultraLightMode : false)))
    property bool thumbnailLoadEnabled: false
    readonly property bool thumbnailRequestActive: root.thumbnailLoadEnabled && root.thumbnailEligible

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
        queueThumbnailLoad(true)
        if (root.resizeOptimized) {
            return
        }
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    Component.onCompleted: {
        queueThumbnailLoad(true)
        if (root.resizeOptimized) {
            return
        }
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
        thumbnailLoadEnabled = false
        if (root.controller.hoveredPath === root.path) {
            root.controller.hoveredPath = ""
        }
    }

    GridView.onReused: {
        isRenaming = false
        visualOffsetX = 0
        queueThumbnailLoad(true)
        opacity = Qt.binding(() => isHidden ? 0.55 : 1.0)
        if (root.resizeOptimized) {
            return
        }
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

    function cancelRename() {
        root.isRenaming = false
    }

    function focusRenameEditor(selectText) {
        return briefRenameEditor.forceEditorFocus(selectText)
    }

    function renameEditorHasFocus() {
        return briefRenameEditor.editorHasFocus()
    }

    function cancelRenameOnPress(reason) {
        if (root.panel && root.panel.cancelInlineRenameForNavigation) {
            root.panel.cancelInlineRenameForNavigation(reason)
        }
    }

    function queueThumbnailLoad(clearExisting) {
        if (clearExisting === true || !root.thumbnailEligible) {
            root.thumbnailLoadEnabled = false
        }
        if (root.thumbnailEligible && !root.thumbnailSchedulingPaused && !root.thumbnailLoadEnabled) {
            thumbnailDelayTimer.restart()
        } else {
            thumbnailDelayTimer.stop()
        }
    }

    onResizeOptimizedChanged: {
        queueThumbnailLoad()
    }
    onThumbnailEligibleChanged: {
        queueThumbnailLoad()
    }
    onThumbnailLoadingPausedChanged: {
        queueThumbnailLoad()
    }
    onThumbnailSchedulingPausedChanged: {
        queueThumbnailLoad()
    }

    Timer {
        id: thumbnailDelayTimer
        interval: 90 + (Math.max(0, root.index) % 12) * 24
        repeat: false
        onTriggered: root.thumbnailLoadEnabled = root.thumbnailEligible && !root.thumbnailSchedulingPaused
    }

    // ── Background ─────────────────────────────────────────────────────────────
    FileItemStateLayer {
        selected: isSelected
        panelActive: root.panelActive
        currentItem: root.currentItem
        hovered: hover.hovered
        scrolling: root.scrolling
        resizeOptimized: root.resizeOptimized
        animationsSuppressed: Boolean(root.panel && root.panel.keyboardNavigationActive)
        visualOffsetX: root.visualOffsetX
        leftMargin: 6
        rightMargin: 6
        topMargin: 2
        bottomMargin: 2
    }

    // ── Hover / mouse ──────────────────────────────────────────────────────────
    HoverHandler {
        id: hover
        enabled: !root.resizeOptimized
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
        if (!scrolling && !root.resizeOptimized) {
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
        target: typeof appSettings !== "undefined" ? appSettings : null
        ignoreUnknownSignals: true
        function onUseNativeIconsChanged() {
            root.queueThumbnailLoad()
        }
        function onShowThumbnailsChanged() {
            root.queueThumbnailLoad()
        }
    }

    Connections {
        target: root.controller ? root.controller.directoryModel : null
        ignoreUnknownSignals: true
        function onLoadingChanged() {
            if (root.controller && root.controller.directoryModel && !root.controller.directoryModel.loading && !root.resizeOptimized) {
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
        scrollGestureEnabled: false
        onWheel: (wheel) => { wheel.accepted = false }
        onPressed: root.cancelRenameOnPress("brief-item-press")

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) root.rightClicked()
            else if (root.isPointOnBadge(mouse.x, mouse.y)) root.controller.directoryModel.toggleSelected(root.index)
            else root.clicked(mouse)
        }
        onDoubleClicked: (mouse) => {
            root.doubleClicked()
        }
    }
    function isPointOnBadge(x, y) {
        if (!selectionToggleBadge || !selectionToggleBadge.visible) return false
        const mapped = selectionToggleBadge.mapFromItem(root, x, y)
        return mapped.x >= 0 && mapped.y >= 0 && mapped.x < selectionToggleBadge.width && mapped.y < selectionToggleBadge.height
    }

    SelectionToggleBadge {
        id: selectionToggleBadge
        x: Theme.scaledSize(7) + root.visualOffsetX
        y: Math.round((root.height - height) / 2)
        z: 30
        badgeSize: Math.max(Theme.scaledSize(14), Math.min(Theme.scaledSize(20), Math.round(root.height * 0.56)))
        markSize: Math.max(5, Math.round(badgeSize * 0.38))
        markStroke: 1
        available: root.panel ? root.panel.showSelectionBadges : true
        controller: root.controller
        panel: root.panel
        index: root.index
        selected: root.isSelected
        hovered: hover.hovered
        currentItem: root.currentItem
        scrolling: root.scrolling || root.isRenaming
    }

    // ── Rename overlay ─────────────────────────────────────────────────────────
    FileNameEditor {
        id: briefRenameEditor
        anchors.fill: parent
        anchors.leftMargin: root.panel && root.panel.showSelectionBadges ? Theme.scaledSize(28) : Theme.scaledSize(14)
        anchors.rightMargin: Theme.scaledSize(6)
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        active: root.isRenaming
        name: root.name
        isDirectory: root.isDirectory
        index: root.index
        controller: root.controller
        fontPixelSize: Theme.fontSizeBody
        onCancelRequested: {
            root.isRenaming = false
            if (root.panel) {
                root.panel.cancelInlineRename()
            }
        }
        onCommitSucceeded: root.isRenaming = false
        onFocusLost: {
            if (root.panel) {
                root.panel.recoverInlineRenameFocus("brief-editor-focus-lost")
            }
        }
    }

    // ── Content row ────────────────────────────────────────────────────────────
    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.leftMargin: root.panel && root.panel.showSelectionBadges ? Theme.scaledSize(28) : Theme.scaledSize(14)
        anchors.rightMargin: Theme.scaledSize(8)
        spacing: Theme.scaledSize(5)
        visible: !root.isRenaming
        transform: Translate { x: root.visualOffsetX }

        // Type dot
        Rectangle {
            width:  Math.max(Theme.scaledSize(4), Math.round(Theme.scaledSize(5) * (1.0 + (scaleFactor - 1.0) * 0.3)))
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
                path: root.path
                iconName: root.iconName
                isDirectory: root.isDirectory
                suffix: root.suffix
                useNativeIcons: root.panel ? root.panel.effectiveUseNativeIcons : (typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true)
                thumbnailSource: root.thumbnailRequestActive
                                 ? "image://thumbnail/" + encodeURIComponent(root.path)
                                 : ""
                showThumbnail: root.thumbnailRequestActive
                iconSize: root.iconSize
            }
        }

        // File name
        Label {
            Layout.fillWidth: true
            text: root.name
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: root.nameFontSize
            font.weight: isSelected ? Font.Medium : Font.Normal
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        // Size badge (files only)
        Label {
            text: root.sizeText
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: root.metaFontSize
            opacity: 0.65
            Layout.preferredWidth: Math.max(Theme.scaledSize(52), Math.round(Theme.scaledSize(52) * scaleFactor * 0.7))
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            visible: !root.isDirectory && root.sizeText !== ""
            elide: Text.ElideRight
        }
    }
}
