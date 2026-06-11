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
        onPressed: root.cancelRenameOnPress("list-item-press")

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                root.rightClicked()
            } else if (root.isPointOnBadge(mouse.x, mouse.y)) {
                root.controller.directoryModel.toggleSelected(root.index)
            } else {
                root.clicked(mouse)
            }
        }

        onDoubleClicked: (mouse) => {
            if (root.isPointOnBadge(mouse.x, mouse.y)) {
                return
            }
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
        x: 8 + root.visualOffsetX
        y: 3
        z: 30
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
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            path: root.path
            iconName: root.iconName
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
            font.pixelSize: 13
            font.weight: isSelected ? Font.Medium : Font.Normal
        }

        Label {
            text: root.isDirectory ? "Folder" : root.sizeText
            color: Theme.textSecondary
            opacity: 0.92
            font.pixelSize: 12
            Layout.preferredWidth: 80
            horizontalAlignment: Text.AlignRight
            visible: parent.width > 400
        }

        Label {
            text: modifiedText
            color: Theme.textSecondary
            opacity: 0.92
            font.pixelSize: 12
            Layout.preferredWidth: 140
            horizontalAlignment: Text.AlignRight
            visible: parent.width > 600
        }
    }
}
