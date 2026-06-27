import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "filepanel"

Item {
    id: root

    required property var controller
    property var panel
    
    // Model roles
    required property int index
    required property string name
    required property string path
    required property string iconName
    required property string mimeType
    required property bool isDirectory
    required property bool isSelected
    required property bool isHidden
    required property bool isArchiveFile
    required property bool isIsoImageFile
    required property string suffix
    required property string sizeText
    required property string modifiedText
    property bool currentItem: false
    property bool panelActive: true
    property bool scrolling: false
    property bool resizeOptimized: false

    // Signals
    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: Theme.rowHeight

    property bool isRenaming: false
    property real visualOffsetX: 0
    property real dragStartX: 0
    property real dragStartY: 0
    property bool dragCandidate: false
    property bool dragStarted: false
    property bool badgePressed: false
    property bool suppressClickAfterDrag: false
    z: root.isRenaming ? 100 : 0

    onPathChanged: {
        isRenaming = false
        visualOffsetX = 0
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

    ListView.onPooled: {
        isRenaming = false
        visualOffsetX = 0
        if (root.controller.hoveredPath === root.path) {
            root.controller.hoveredPath = ""
        }
    }

    ListView.onReused: {
        isRenaming = false
        visualOffsetX = 0
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
        return renameEditor.forceEditorFocus(selectText)
    }

    function renameEditorHasFocus() {
        return renameEditor.editorHasFocus()
    }

    function cancelRenameOnPress(reason) {
        if (root.panel && root.panel.cancelInlineRenameForNavigation) {
            root.panel.cancelInlineRenameForNavigation(reason)
        }
    }

    opacity: isHidden ? 0.55 : 1.0

    FileItemStateLayer {
        selected: isSelected
        panelActive: root.panelActive
        currentItem: root.currentItem
        hovered: hover.hovered
        scrolling: root.scrolling
        resizeOptimized: root.resizeOptimized
        animationsSuppressed: Boolean(root.panel && root.panel.keyboardNavigationActive)
        visualOffsetX: root.visualOffsetX
        leftMargin: 4
        rightMargin: 4
        topMargin: 1
        bottomMargin: 1
        selectionBarLeftMargin: 4
        selectionBarTopMargin: 4
        selectionBarBottomMargin: 4
    }

    HoverHandler {
        id: hover
        enabled: !root.resizeOptimized
        cursorShape: root.panel && root.panel.internalDragEnabled
                     ? root.panel.itemHoverCursorShape(root, point.position.x, point.position.y)
                     : Qt.ArrowCursor
        onHoveredChanged: {
            if (root.scrolling) return
            if (hovered) {
                root.controller.hoveredPath = root.path
                if (root.panel && root.panel.internalDragEnabled) {
                    root.panel.updateHoverDragCursor(root, point.position.x, point.position.y)
                }
            } else {
                if (root.controller.hoveredPath === root.path) {
                    root.controller.hoveredPath = ""
                }
                if (root.panel) {
                    root.panel.clearHoverDragCursor(root)
                }
            }
        }
        onPointChanged: {
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
        preventStealing: root.dragCandidate || root.badgePressed
        cursorShape: root.panel
                     && root.panel.internalDragEnabled
                     && typeof root.panel.itemDragAffordanceCursor === "function"
                     ? root.panel.itemHoverCursorShape(root, mouseX, mouseY)
                     : Qt.ArrowCursor
        scrollGestureEnabled: false
        onWheel: (wheel) => { wheel.accepted = false }
        onPressed: (mouse) => {
            root.cancelRenameOnPress("list-item-press")
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
            if (mouse.button === Qt.RightButton) {
                root.rightClicked()
            } else if (root.isPointOnBadge(mouse.x, mouse.y)) {
                root.controller.directoryModel.toggleSelected(root.index)
            } else {
                root.clicked(mouse)
            }
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
        return root.isWithinItem(fileIcon, x, y, 2)
    }

    SelectionToggleBadge {
        id: selectionToggleBadge
        x: 8 + root.visualOffsetX
        y: 3
        z: 30
        badgeSize: 16
        markSize: 6
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

    FileNameEditor {
        id: renameEditor
        anchors.fill: parent
        anchors.leftMargin: 52
        anchors.rightMargin: 8
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        active: root.isRenaming
        name: root.name
        isDirectory: root.isDirectory
        index: root.index
        controller: root.controller
        windowObject: root.panel ? root.panel.Window.window : null
        fontPixelSize: 13
        onCancelRequested: {
            root.isRenaming = false
            if (root.panel) {
                root.panel.cancelInlineRename()
            }
        }
        onCommitSucceeded: root.isRenaming = false
        onFocusLost: {
            if (root.panel) {
                root.panel.recoverInlineRenameFocus("list-editor-focus-lost")
            }
        }
    }

        RowLayout {
            id: fileContent
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12
            visible: !isRenaming
            transform: Translate { x: root.visualOffsetX }

        FileIconCell {
            id: fileIcon
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            path: root.path
            name: root.name
            iconName: root.iconName
            mimeType: root.mimeType
            isDirectory: root.isDirectory
            suffix: root.suffix
            useNativeIcons: root.panel ? root.panel.effectiveUseNativeIcons : (typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true)
            iconSize: 16
        }

        Label {
            Layout.fillWidth: true
            text: name
            color: Theme.textPrimary
            elide: Text.ElideRight
            font.pixelSize: Theme.fontSizeBody
            font.weight: isSelected ? Font.Medium : Font.Normal
        }

        Label {
            text: root.isDirectory ? "Folder" : root.sizeText
            color: Theme.textSecondary
            opacity: 0.92
            font.pixelSize: Theme.fontSizeLabel
            Layout.preferredWidth: 80
            horizontalAlignment: Text.AlignRight
            visible: parent.width > 400
        }

        Label {
            text: modifiedText
            color: Theme.textSecondary
            opacity: 0.92
            font.pixelSize: Theme.fontSizeLabel
            Layout.preferredWidth: 140
            horizontalAlignment: Text.AlignRight
            visible: parent.width > 600
        }
    }
}
