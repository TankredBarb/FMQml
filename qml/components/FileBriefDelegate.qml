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
    required property string mimeType
    required property bool   isDirectory
    required property bool   isSelected
    required property bool   isHidden
    required property bool   isArchiveFile
    required property bool   isIsoImageFile
    required property string primaryBadgeKind
    required property bool isPinned
    required property bool   isImage
    required property bool   hasThumbnail
    required property int    thumbnailRevision
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
    property real dragStartX: 0
    property real dragStartY: 0
    property bool dragCandidate: false
    property bool dragStarted: false
    property bool badgePressed: false
    property bool suppressClickAfterDrag: false
    property string thumbnailFailedPath: ""
    property int thumbnailRetryAttempt: 0
    property int thumbnailRetryRevision: 0
    z: root.isRenaming ? 100 : 0

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: root.panel ? root.panel.briefRowHeight : Math.max(Theme.controlHeight - 10, Theme.fontSizeLabel + 16)

    readonly property real  scaleFactor: implicitHeight > 0 ? height / implicitHeight : 1.0
    readonly property int   iconSize: Math.max(Theme.scaledSize(16),
                                                Math.min(Theme.scaledSize(40),
                                                         Math.round(Math.max(Theme.scaledSize(16), height - Theme.scaledSize(8)))))
    readonly property int   nameFontSize: Theme.fontSizeBody
    readonly property int   metaFontSize: Theme.fontSizeCaption
    readonly property bool  canShowThumbnail: !isDirectory && hasThumbnail
    readonly property bool  thumbnailEligible: root.canShowThumbnail
                                           && !root.thumbnailLoadingPaused
                                           && (root.panel ? root.panel.effectiveUseNativeIcons : (typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true))
                                           && (root.panel ? root.panel.effectiveShowThumbnails
                                                          : ((typeof appSettings !== "undefined" && appSettings ? appSettings.showThumbnails : true)
                                                             && !(typeof appSettings !== "undefined" && appSettings ? appSettings.ultraLightMode : false)))
                                           && root.thumbnailFailedPath !== root.path
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
        thumbnailFailedPath = ""
        thumbnailRetryAttempt = 0
        thumbnailRetryRevision = 0
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

    onThumbnailRevisionChanged: {
        thumbnailFailedPath = ""
        thumbnailRetryAttempt = 0
        thumbnailRetryRevision = 0
        if (!thumbnailLoadEnabled) {
            queueThumbnailLoad()
        }
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
        thumbnailFailedPath = ""
        thumbnailRetryAttempt = 0
        thumbnailRetryRevision = 0
        // A path can still be visible in the other panel. Cancellation needs
        // per-delegate ownership before pooled delegates may cancel shared jobs.
        if (root.controller.hoveredPath === root.path) {
            if (root.panel && root.panel.clearHoveredItem) {
                root.panel.clearHoveredItem(root.path)
            } else {
                root.controller.hoveredPath = ""
            }
        }
    }

    GridView.onReused: {
        isRenaming = false
        visualOffsetX = 0
        thumbnailFailedPath = ""
        thumbnailRetryAttempt = 0
        thumbnailRetryRevision = 0
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

    function scheduleThumbnailRetry() {
        if (!root.thumbnailRequestActive || root.thumbnailRetryAttempt >= 3) {
            return
        }
        thumbnailRetryTimer.interval = 350 * Math.pow(2, root.thumbnailRetryAttempt)
        thumbnailRetryTimer.restart()
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
        onTriggered: {
            root.thumbnailLoadEnabled = root.thumbnailEligible && !root.thumbnailSchedulingPaused
            if (root.thumbnailLoadEnabled && typeof thumbnailController !== "undefined" && thumbnailController) {
                thumbnailController.requestThumbnail(root.path, root.iconSize * 2, root.iconSize * 2, 100, "visible")
            }
        }
    }

    Timer {
        id: thumbnailRetryTimer
        repeat: false
        onTriggered: {
            if (!root.thumbnailRequestActive) {
                return
            }
            root.thumbnailRetryAttempt += 1
            root.thumbnailRetryRevision += 1
        }
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
        cursorShape: root.panel && root.panel.internalDragEnabled
                     ? root.panel.itemHoverCursorShape(root, point.position.x, point.position.y)
                     : Qt.ArrowCursor
        onHoveredChanged: {
            if (root.scrolling) return
            if (hovered) {
                if (root.panel && root.panel.setHoveredItem) {
                    root.panel.setHoveredItem(root, root.path, point.position)
                } else {
                    root.controller.hoveredPath = root.path
                }
                if (root.panel && root.panel.internalDragEnabled) {
                    root.panel.updateHoverDragCursor(root, point.position.x, point.position.y)
                }
            } else {
                if (root.panel && root.panel.clearHoveredItem) {
                    root.panel.clearHoveredItem(root.path)
                } else if (root.controller.hoveredPath === root.path) {
                    root.controller.hoveredPath = ""
                }
                if (root.panel) {
                    root.panel.clearHoverDragCursor(root)
                }
            }
        }
        onPointChanged: {
            if (hovered && root.panel && root.panel.setHoveredItem) {
                root.panel.setHoveredItem(root, root.path, point.position)
            }
            if (hovered && root.panel && root.panel.internalDragEnabled) {
                root.panel.updateHoverDragCursor(root, point.position.x, point.position.y)
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
                        if (root.panel && root.panel.setHoveredItem) {
                            root.panel.setHoveredItem(root, root.path, hover.point.position)
                        } else {
                            root.controller.hoveredPath = root.path
                        }
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
                            if (root.panel && root.panel.setHoveredItem) {
                                root.panel.setHoveredItem(root, root.path, hover.point.position)
                            } else {
                                root.controller.hoveredPath = root.path
                            }
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
        preventStealing: root.dragCandidate || root.badgePressed
        cursorShape: root.panel
                     && root.panel.internalDragEnabled
                     && typeof root.panel.itemDragAffordanceCursor === "function"
                     ? root.panel.itemHoverCursorShape(root, mouseX, mouseY)
                     : Qt.ArrowCursor
        scrollGestureEnabled: false
        onWheel: (wheel) => { wheel.accepted = false }
        onPressed: (mouse) => {
            root.cancelRenameOnPress("brief-item-press")
            root.badgePressed = mouse.button === Qt.LeftButton
                                && root.isPointOnBadge(mouse.x, mouse.y)
            root.dragCandidate = root.panel
                                 && root.panel.internalDragEnabled
                                 && mouse.button === Qt.LeftButton
                                 && !root.isRenaming
                                 && !root.badgePressed
                                 && (root.isSelected || root.isPointOnDragSurface(mouse.x, mouse.y))
            root.dragStarted = false
            root.dragStartX = mouse.x
            root.dragStartY = mouse.y
        }

        onPositionChanged: (mouse) => {
            if (root.badgePressed) {
                return
            }
            if (!root.dragCandidate || !root.panel) {
                return
            }
            if (root.dragStarted) {
                root.panel.updateSelectionDragPosition(mouse, root)
            } else {
                root.dragStarted = root.panel.updateSelectionDragCandidate(
                            root.index, root.path, root.dragStartX, root.dragStartY,
                            mouse.x, mouse.y, mouse)
                if (root.dragStarted) {
                    root.panel.updateSelectionDragPosition(mouse, root)
                }
            }
        }

        onReleased: (mouse) => {
            if (root.dragStarted && root.panel) {
                root.panel.finishSelectionDrag(mouse, root)
                root.suppressClickAfterDrag = true
                suppressClickReset.restart()
            }
            root.dragCandidate = false
            root.dragStarted = false
            root.badgePressed = false
        }

        onCanceled: {
            if (root.dragStarted && root.panel && root.panel.internalDragEnabled && root.panel.dragCoordinator) {
                root.panel.dragCoordinator.cancelDrag("Drag canceled.")
            }
            root.dragCandidate = false
            root.dragStarted = false
            root.badgePressed = false
        }

        onClicked: (mouse) => {
            if (root.suppressClickAfterDrag) {
                root.suppressClickAfterDrag = false
                return
            }
            if (mouse.button === Qt.RightButton) root.rightClicked()
            else if (root.isPointOnBadge(mouse.x, mouse.y)) root.controller.directoryModel.toggleSelected(root.index)
            else root.clicked(mouse)
        }
        onDoubleClicked: (mouse) => {
            root.doubleClicked()
        }
    }

    Timer {
        id: suppressClickReset
        interval: 0
        repeat: false
        onTriggered: root.suppressClickAfterDrag = false
    }
    function isPointOnBadge(x, y) {
        if (!selectionToggleBadge || !selectionToggleBadge.visible) return false
        const mapped = selectionToggleBadge.mapFromItem(root, x, y)
        return mapped.x >= 0 && mapped.y >= 0 && mapped.x < selectionToggleBadge.width && mapped.y < selectionToggleBadge.height
    }

    function isWithinItem(item, x, y, padding) {
        if (!item || !item.visible) {
            return false
        }
        const mapped = item.mapFromItem(root, x, y)
        const pad = padding || 0
        return mapped.x >= -pad && mapped.y >= -pad
                && mapped.x < item.width + pad && mapped.y < item.height + pad
    }

    function isPointOnDragSurface(x, y) {
        if (!iconFrame || !iconFrame.visible) {
            return false
        }
        const mapped = iconFrame.mapFromItem(root, x, y)
        const handleSize = Math.min(iconFrame.width, iconFrame.height, Theme.scaledSize(24))
        const left = (iconFrame.width - handleSize) / 2
        const top = (iconFrame.height - handleSize) / 2
        return mapped.x >= left && mapped.y >= top
                && mapped.x < left + handleSize && mapped.y < top + handleSize
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
        windowObject: root.panel ? root.panel.Window.window : null
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
            id: iconFrame
            Layout.preferredWidth:  root.iconSize
            Layout.preferredHeight: root.iconSize
            Layout.alignment: Qt.AlignVCenter

            FileIconCell {
                anchors.fill: parent
                path: root.path
                name: root.name
                iconName: root.iconName
                mimeType: root.mimeType
                isDirectory: root.isDirectory
                primaryBadgeKind: root.primaryBadgeKind
                isPinned: root.isPinned
                hasThumbnail: root.hasThumbnail
                suffix: root.suffix
                useNativeIcons: root.panel ? root.panel.effectiveUseNativeIcons : (typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true)
                thumbnailSource: root.thumbnailRequestActive
                                 ? (root.panel && root.panel.thumbnailSourceFor
                                    ? root.panel.thumbnailSourceFor(root.path, root.thumbnailRevision + root.thumbnailRetryRevision * 1000000)
                                    : "image://thumbnail/" + encodeURIComponent(root.path + "::thumbrev=" + (root.thumbnailRevision + root.thumbnailRetryRevision * 1000000)))
                                 : ""
                showThumbnail: root.thumbnailRequestActive
                iconSize: root.iconSize
                onThumbnailError: {
                    root.thumbnailFailedPath = root.path
                    root.thumbnailLoadEnabled = false
                }
                onThumbnailSoftMiss: root.scheduleThumbnailRetry()
            }
        }

        // File name
        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            clip: true

            Label {
                text: {
                    if (root.isDirectory || !root.suffix) return root.name
                    const extLen = root.suffix.length
                    if (extLen > 0 && root.name.endsWith("." + root.suffix)) {
                        return root.name.substring(0, root.name.length - extLen - 1)
                    }
                    return root.name
                }
                color: root.isDirectory ? TextColors.folderNameText : TextColors.fileNameText
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: root.nameFontSize
                font.weight: isSelected ? Font.Medium : Font.Normal
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
                Layout.maximumWidth: root.isDirectory ? Infinity : Math.ceil(implicitWidth)
            }

            Label {
                visible: !root.isDirectory && !!root.suffix && root.name.endsWith("." + root.suffix)
                text: "." + root.suffix
                color: TextColors.fileExtensionText
                font.family: Theme.fontFamily
                font.pixelSize: root.nameFontSize
                font.weight: isSelected ? Font.Medium : Font.Normal
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                Layout.fillWidth: false
            }

            Item {
                Layout.fillWidth: true
            }
        }

        // Size badge (files only)
        Label {
            text: root.sizeText
            color: TextColors.fileSecondaryText
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
