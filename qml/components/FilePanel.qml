import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"

Pane {
    id: root

    required property var controller
    property bool active: false
    readonly property int viewMode: root.controller.viewMode
    property int gridIconSize: 48
    readonly property int gridIconMinSize: 32
    readonly property int gridIconMaxSize: 96
    readonly property int gridCellWidth: Math.max(96, gridIconSize + 52)
    readonly property int gridCellHeight: Math.max(112, gridIconSize + 72)
    readonly property bool statusRailVisible: root.statusMessage.length > 0 || root.controller.directoryModel.loading
    property bool scrolling: false
    focus: root.active

    Timer {
        id: scrollStopTimer
        interval: 120
        onTriggered: {
            root.scrolling = false
            root.controller.scrolling = false
        }
    }

    function updateScrollingState() {
        const moving = root.viewMode === 0 ? listView.moving : gridView.moving
        const flicking = root.viewMode === 0 ? listView.flicking : gridView.flicking
        const isScrolling = moving || flicking

        if (isScrolling) {
            scrollStopTimer.stop()
            if (!root.scrolling) {
                root.scrolling = true
                root.controller.scrolling = true
                // Clear hover on scroll start
                root.controller.hoveredPath = ""
            }
        } else {
            if (root.scrolling && !scrollStopTimer.running) {
                scrollStopTimer.start()
            }
        }
    }

    property string statusMessage: ""
    Timer {
        id: statusTimer
        interval: 5000
        onTriggered: root.statusMessage = ""
    }

    Connections {
        target: workspaceController.operationQueue
        function onStatusMessageChanged() {
            root.statusMessage = workspaceController.operationQueue.statusMessage
            statusTimer.restart()
        }
        function onBusyChanged() {
            if (!workspaceController.operationQueue.busy) {
                statusTimer.restart()
            }
        }
    }

    Connections {
        target: root.controller
        function onStatusMessageChanged() {
            if (root.controller.statusMessage.length > 0) {
                root.statusMessage = root.controller.statusMessage
                statusTimer.restart()
            }
        }
    }

    Connections {
        target: workspaceController
        function onRenameRequested() {
            if (root.active) root.startRename()
        }
    }

    Keys.onPressed: (event) => {
        if (event.matches(StandardKey.SelectAll)) {
            root.controller.directoryModel.selectAll()
            event.accepted = true
        }
    }

    padding: 0
    background: Item {
        id: backgroundWrapper
        
        layer.enabled: root.active
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.activeGlow
            shadowBlur: 0.5
            shadowVerticalOffset: 0
            shadowHorizontalOffset: 0
        }

        Rectangle {
            id: panelBg
            anchors.fill: parent
            radius: Theme.radius
            color: themeController.isDark ? Theme.surface : Theme.bg
            border.color: root.active ? Theme.activeAccent : Theme.border
            border.width: root.active ? 3 : 1

            // Subtle overlay for the whole panel
            Rectangle {
                anchors.fill: parent
                radius: Theme.radius
                color: root.active 
                       ? Qt.rgba(Theme.activeAccent.r, Theme.activeAccent.g, Theme.activeAccent.b, themeController.isDark ? 0.03 : 0.05)
                       : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.015 : 0.03)
            }

            // --- PERFECTLY ROUNDED TOP ACCENT ---
            // We use an Item to clip a full-sized rounded rectangle
            Item {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: root.active ? 4 : 1 // The visible height of the accent
                clip: true 
                visible: root.active
                
                Rectangle {
                    anchors.top: parent.top
                    width: panelBg.width
                    height: panelBg.height // Full height to match parent radius
                    radius: Theme.radius
                    color: Theme.activeAccent
                    antialiasing: true
                }
            }

            Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
            Behavior on border.width { NumberAnimation { duration: Theme.motionFast } }
        }
    }

    function contextRow() {
        return root.viewMode === 0 ? listView.currentIndex : gridView.currentIndex
    }

    function startRename() {
        let idx = contextRow()
        if (idx < 0) return
        
        if (root.viewMode === 0) {
            if (listView.currentItem) listView.currentItem.startRename()
        } else {
            if (gridView.currentItem) gridView.currentItem.startRename()
        }
    }

    function focusContent() {
        if (root.viewMode === 0) {
            listView.forceActiveFocus()
        } else {
            gridView.forceActiveFocus()
        }
    }

    function loadingFolderName() {
        const parts = root.controller.currentPath.split(/[/\\]/).filter(part => part.length > 0)
        if (parts.length === 0) {
            return "this folder"
        }
        return parts[parts.length - 1]
    }

    signal activated()

    readonly property string revealInOsLabel: Qt.platform.os === "windows" ? "Show in Explorer"
            : Qt.platform.os === "osx" ? "Reveal in Finder"
            : "Open Containing Folder"

    ThemedContextMenu {
        id: contextMenu
        ThemedMenuItem {
            text: "Open"
            icon.source: "../assets/icons/folder-plus.svg"
            iconColor: "#22c55e"
            enabled: contextRow() >= 0
            onTriggered: root.controller.openItem(contextRow())
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Cut to Clipboard"
            icon.source: "../assets/icons/move.svg"
            iconColor: "#f59e0b"
            enabled: root.controller.directoryModel.selectedCount > 0
                     && !workspaceController.operationQueue.busy
            onTriggered: workspaceController.cutToClipboard()
        }
        ThemedMenuItem {
            text: "Copy to Clipboard"
            icon.source: "../assets/icons/copy.svg"
            iconColor: "#3b82f6"
            enabled: root.controller.directoryModel.selectedCount > 0
                     && !workspaceController.operationQueue.busy
            onTriggered: workspaceController.copyToClipboard()
        }
        ThemedMenuItem {
            text: "Paste from Clipboard"
            icon.source: "../assets/icons/paste.svg"
            iconColor: "#14b8a6"
            enabled: workspaceController.hasClipboard && !workspaceController.operationQueue.busy
            onTriggered: workspaceController.pasteFromClipboard()
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Rename"
            icon.source: "../assets/icons/rename.svg"
            iconColor: "#a855f7"
            enabled: contextRow() >= 0
            onTriggered: root.startRename()
        }
        ThemedMenuItem {
            text: "Delete"
            icon.source: "../assets/icons/delete.svg"
            destructive: true
            iconColor: "#ef4444"
            enabled: root.controller.directoryModel.selectedCount > 0
                     && !workspaceController.operationQueue.busy
            onTriggered: workspaceController.requestDelete(root.controller.selectedPaths(), root.controller.currentPath)
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Refresh"
            icon.source: "../assets/icons/refresh.svg"
            iconColor: "#14b8a6"
            onTriggered: root.controller.refresh()
        }
        ThemedMenuItem {
            text: revealInOsLabel
            icon.source: "../assets/icons/reveal.svg"
            iconColor: "#22c55e"
            enabled: contextRow() >= 0
            onTriggered: root.controller.revealInFileManager(contextRow())
        }
        ThemedMenuItem {
            text: "Properties"
            icon.source: "../assets/icons/info.svg"
            iconColor: "#0ea5e9"
            enabled: contextRow() >= 0
            onTriggered: root.controller.showProperties(contextRow())
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Open in PowerShell"
            icon.source: "../assets/icons/terminal.svg"
            iconColor: "#6366f1"
            visible: Qt.platform.os === "windows"
            enabled: root.controller.currentPath.length > 0
            onTriggered: root.controller.openInTerminal()
        }
    }

    ThemedContextMenu {
        id: emptyContextMenu
        ThemedMenuItem {
            text: "Open in PowerShell"
            icon.source: "../assets/icons/terminal.svg"
            iconColor: "#6366f1"
            visible: Qt.platform.os === "windows"
            enabled: root.controller.currentPath.length > 0
            onTriggered: root.controller.openInTerminal()
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "New Folder"
            icon.source: "../assets/icons/folder-plus.svg"
            iconColor: "#22c55e"
            onTriggered: root.controller.createFolder("New Folder")
        }
        ThemedMenuItem {
            text: "New Text File"
            icon.source: "../assets/icons/document.svg"
            iconColor: "#f59e0b"
            onTriggered: root.controller.createFile("New Text File.txt")
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Paste from Clipboard"
            icon.source: "../assets/icons/paste.svg"
            iconColor: "#14b8a6"
            enabled: workspaceController.hasClipboard && !workspaceController.operationQueue.busy
            onTriggered: workspaceController.pasteFromClipboard()
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Select All"
            icon.source: "../assets/icons/select-all.svg"
            iconColor: "#8b5cf6"
            onTriggered: root.controller.directoryModel.selectAll()
        }
        ThemedMenuItem {
            text: root.controller.directoryModel.showHidden ? "Hide Hidden Files" : "Show Hidden Files"
            icon.source: root.controller.directoryModel.showHidden ? "../assets/icons/eye-off.svg" : "../assets/icons/eye.svg"
            iconColor: "#10b981"
            onTriggered: root.controller.directoryModel.showHidden = !root.controller.directoryModel.showHidden
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Refresh"
            icon.source: "../assets/icons/refresh.svg"
            iconColor: "#14b8a6"
            onTriggered: root.controller.refresh()
        }
        ThemedMenuItem {
            text: "Properties"
            icon.source: "../assets/icons/info.svg"
            iconColor: "#0ea5e9"
            onTriggered: propertiesController.load(root.controller.currentPath)
        }
    }

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasText) {
                const paths = [drop.text]
                workspaceController.operationQueue.copyTo(paths, root.controller.currentPath)
            }
        }
        
        Rectangle {
            anchors.fill: parent
            color: Theme.accent
            opacity: parent.containsDrag ? 0.1 : 0
            visible: parent.containsDrag
            border.color: Theme.accent
            border.width: 2
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 42
            color: "transparent"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: root.active ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55)
                                  : Theme.border
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        root.activated()
                        emptyContextMenu.popup()
                    } else {
                        root.activated()
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 12
                spacing: 8

                PathBar {
                    id: panelPathBar
                    Layout.fillWidth: true
                    controller: root.controller
                    path: root.controller.currentPath
                    onActiveFocusChanged: if (activeFocus) root.activated()
                }

                Rectangle {
                    implicitHeight: 26
                    implicitWidth: selectionText.implicitWidth + 18
                    radius: 13
                    color: root.controller.directoryModel.selectedCount > 0
                           ? (root.active ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
                           : Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.12)
                    border.color: root.controller.directoryModel.selectedCount > 0
                                  ? (root.active ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                                  : Theme.border
                    border.width: 1

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: 12
                        color: "transparent"
                        border.color: root.controller.directoryModel.selectedCount > 0
                                      ? Qt.rgba(255, 255, 255, themeController.isDark ? 0.10 : 0.14)
                                      : "transparent"
                        border.width: root.controller.directoryModel.selectedCount > 0 ? 1 : 0
                    }

                    Text {
                        id: selectionText
                        anchors.centerIn: parent
                        text: root.controller.directoryModel.selectedCount > 0
                              ? root.controller.directoryModel.selectedCount + " selected"
                              : root.controller.directoryModel.count + " items"
                        color: root.controller.directoryModel.selectedCount > 0 ? Theme.accent : Theme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: listView
                anchors.fill: parent
                visible: root.viewMode === 0
                enabled: visible
                clip: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                pixelAligned: false
                flickableDirection: Flickable.VerticalFlick
                model: root.controller.directoryModel
                currentIndex: -1
                focus: root.active
                cacheBuffer: height * 2
                reuseItems: true
                onMovingChanged: root.updateScrollingState()
                onFlickingChanged: root.updateScrollingState()
                
                highlight: null
                highlightFollowsCurrentItem: false

                add: root.controller.directoryModel.count < 500 ? listAddTransition : null
                remove: root.controller.directoryModel.count < 500 ? listRemoveTransition : null

                Transition {
                    id: listAddTransition
                    NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 140; easing.type: Easing.OutQuad }
                    NumberAnimation { property: "visualOffsetX"; from: -18; to: 0; duration: 160; easing.type: Easing.OutCubic }
                }
                Transition {
                    id: listRemoveTransition
                    NumberAnimation { property: "opacity"; to: 0.0; duration: 110; easing.type: Easing.InQuad }
                    NumberAnimation { property: "visualOffsetX"; to: -10; duration: 110; easing.type: Easing.InQuad }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                        if (currentIndex >= 0 && listView.currentItem && !listView.currentItem.isRenaming)
                            root.controller.openItem(currentIndex)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Backspace) {
                        root.controller.goUp()
                        event.accepted = true
                    } else if (event.key === Qt.Key_F2) {
                        root.startRename()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        root.controller.directoryModel.clearSelection()
                        workspaceController.focusActivePanel()
                        event.accepted = true
                    }
                }

                delegate: FileDelegate {
                    id: fileDelegate
                    width: listView.width
                    controller: root.controller
                    currentItem: ListView.isCurrentItem
                    panelActive: root.active
                    scrolling: root.scrolling
                    
                    onClicked: (mouse) => {
                        root.activated()
                        listView.currentIndex = index
                        if (mouse.modifiers & Qt.ControlModifier) {
                            root.controller.directoryModel.toggleSelected(index)
                        } else {
                            root.controller.directoryModel.selectOnly(index)
                        }
                    }
                    onRightClicked: {
                        root.activated()
                        listView.currentIndex = index
                        if (!root.controller.directoryModel.selectedCount || !root.controller.directoryModel.selectedPaths().includes(path)) {
                            root.controller.directoryModel.selectOnly(index)
                        }
                        contextMenu.popup()
                    }
                    onDoubleClicked: root.controller.openItem(index)
                }

                ScrollBar.vertical: ScrollBar {}
            }

            GridView {
                id: gridView
                anchors.fill: parent
                anchors.margins: 10
                anchors.bottomMargin: root.viewMode === 1 ? (root.statusRailVisible ? 92 : 56) : 10
                visible: root.viewMode === 1
                enabled: visible
                clip: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                pixelAligned: false
                flickableDirection: Flickable.VerticalFlick
                cellWidth: root.gridCellWidth
                cellHeight: root.gridCellHeight
                model: root.controller.directoryModel
                currentIndex: -1
                focus: root.active
                cacheBuffer: Math.max(0, height * 1.5)
                reuseItems: true
                onMovingChanged: root.updateScrollingState()
                onFlickingChanged: root.updateScrollingState()
                
                highlight: null
                highlightFollowsCurrentItem: false

                add: root.controller.directoryModel.count < 500 ? gridAddTransition : null
                remove: root.controller.directoryModel.count < 500 ? gridRemoveTransition : null

                Transition {
                    id: gridAddTransition
                    NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutQuad }
                    NumberAnimation { property: "visualOffsetY"; from: 12; to: 0; duration: 170; easing.type: Easing.OutCubic }
                }
                Transition {
                    id: gridRemoveTransition
                    NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InQuad }
                    NumberAnimation { property: "visualOffsetY"; to: 8; duration: 120; easing.type: Easing.InQuad }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                        if (currentIndex >= 0 && gridView.currentItem && !gridView.currentItem.isRenaming)
                            root.controller.openItem(currentIndex)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Backspace) {
                        root.controller.goUp()
                        event.accepted = true
                    } else if (event.key === Qt.Key_F2) {
                        root.startRename()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        root.controller.directoryModel.clearSelection()
                        workspaceController.focusActivePanel()
                        event.accepted = true
                    }
                }

                delegate: Item {
                    id: gridDelegate
                    width: gridView.cellWidth
                    height: gridView.cellHeight

                    required property int index
                    required property string name
                    required property string path
                    required property string suffix
                    required property bool isDirectory
                    required property bool isSelected
                    required property bool isImage

                    property bool isRenaming: false
                    property bool currentItem: GridView.isCurrentItem
                    property bool panelActive: root.active
                    property real visualOffsetY: 0

                    onPathChanged: {
                        isRenaming = false
                        visualOffsetY = 0
                    }

                    GridView.onPooled: {
                        isRenaming = false
                        visualOffsetY = 0
                    }

                    GridView.onReused: {
                        isRenaming = false
                        visualOffsetY = 0
                    }

                    function startRename() {
                        isRenaming = true
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: 6
                        color: isSelected
                               ? (root.active ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
                               : (currentItem
                                  ? Theme.itemCurrentFill
                                  : (hoverGrid.hovered ? Theme.itemHoverFill : "transparent"))
                        border.color: isSelected
                                      ? (root.active ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                                      : (currentItem ? Theme.itemCurrentBorder : "transparent")
                        border.width: isSelected || currentItem ? 1 : 0
                        transform: Translate { y: gridDelegate.visualOffsetY }
                    }

                    HoverHandler { 
                        id: hoverGrid 
                        enabled: !root.scrolling
                        onHoveredChanged: {
                            if (hovered) {
                                root.controller.hoveredPath = path
                            } else if (root.controller.hoveredPath === path) {
                                root.controller.hoveredPath = ""
                            }
                        }
                    }

                    Loader {
                        id: gridRenameLoader
                        anchors.top: parent.top
                        anchors.topMargin: root.gridIconSize + 26
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        height: 24
                        active: isRenaming
                        visible: isRenaming
                        sourceComponent: TextField {
                            text: name
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 12
                            color: Theme.textPrimary
                            selectByMouse: true
                            background: Rectangle {
                                color: Theme.surface
                                radius: 4
                                border.color: Theme.accent
                            }
                            onAccepted: {
                                if (index >= 0) {
                                    const idx = index
                                    const txt = text
                                    const ctrl = root.controller
                                    Qt.callLater(function() {
                                        if (ctrl.rename(idx, txt)) {
                                            isRenaming = false
                                        } else {
                                            if (gridRenameLoader.item) {
                                                gridRenameLoader.item.forceActiveFocus()
                                                gridRenameLoader.item.selectAll()
                                            }
                                        }
                                    })
                                }
                            }
                            onActiveFocusChanged: if (!activeFocus) isRenaming = false
                            
                            Component.onCompleted: {
                                forceActiveFocus()
                                let lastDot = name.lastIndexOf(".")
                                if (!isDirectory && lastDot > 0) {
                                    select(0, lastDot)
                                } else {
                                    selectAll()
                                }
                            }
                        }
                    }

                    ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6
                    transform: Translate { y: gridDelegate.visualOffsetY }

                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: root.gridIconSize
                        Layout.preferredHeight: root.gridIconSize

                        Image {
                            anchors.centerIn: parent
                            width: Math.max(28, Math.round(root.gridIconSize * 0.8))
                            height: width
                            source: isImage ? "../assets/icons/image.svg" : "image://icon/" + path
                            sourceSize: Qt.size(width, height)
                            visible: !isImage
                            opacity: isImage ? 0.72 : 1.0
                            smooth: true
                            mipmap: false
                        }

                        Image {
                            anchors.centerIn: parent
                            width: Math.round(root.gridIconSize * 0.9)
                            height: width
                            source: "../assets/icons/image.svg"
                            sourceSize: Qt.size(width, height)
                            visible: isImage
                            opacity: 0.74
                            smooth: true
                            mipmap: false
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            visible: isImage && thumbnail.status !== Image.Ready
                            color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, themeController.isDark ? 0.18 : 0.12)
                        }

                        Image {
                            id: thumbnail
                            anchors.fill: parent
                            source: isImage ? "image://thumbnail/" + path : ""
                            sourceSize: Qt.size(Math.min(192, root.gridIconSize * 2), Math.min(192, root.gridIconSize * 2))
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                            smooth: true
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !isRenaming
                        text: name
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    font.pixelSize: 12
                        color: Theme.textPrimary
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                    }
                    }
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        
                        onClicked: (mouse) => {
                            if (!root.visible) return
                            root.activated()
                            gridView.currentIndex = index
                            if (mouse.button === Qt.RightButton) {
                                if (!root.controller.directoryModel.selectedCount || !root.controller.directoryModel.selectedPaths().includes(path)) {
                                    root.controller.directoryModel.selectOnly(index)
                                }
                                contextMenu.popup()
                            } else {
                                if (mouse.modifiers & Qt.ControlModifier) root.controller.directoryModel.toggleSelected(index)
                                else root.controller.directoryModel.selectOnly(index)
                            }
                        }
                        onDoubleClicked: root.controller.openItem(index)
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }

            Rectangle {
                id: gridDensityBar
                visible: root.viewMode === 1
                z: 5
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 12
                anchors.bottomMargin: root.statusRailVisible ? 44 : 12
                width: 292
                height: 42
                radius: 13
                color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.065)
                                              : Qt.rgba(0, 0, 0, 0.04)
                border.color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, themeController.isDark ? 0.95 : 0.85)
                border.width: 1
                layer.enabled: false

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Label {
                        text: "Icon size"
                        Layout.preferredWidth: 58
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                    }

                    Slider {
                        id: gridIconSlider
                        Layout.fillWidth: true
                        from: root.gridIconMinSize
                        to: root.gridIconMaxSize
                        stepSize: 4
                        snapMode: Slider.SnapAlways
                        focusPolicy: Qt.StrongFocus
                        value: root.gridIconSize

                        onMoved: {
                            const snapped = Math.round(value / stepSize) * stepSize
                            if (snapped !== root.gridIconSize) {
                                root.gridIconSize = snapped
                            }
                        }

                        background: Item {
                            anchors.fill: parent
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: 4
                                radius: 2
                                color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, themeController.isDark ? 0.65 : 0.5)
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: gridIconSlider.visualPosition * parent.width
                                height: 4
                                radius: 2
                                color: Theme.accent
                            }
                        }

                        handle: Rectangle {
                            width: 0
                            height: 0
                            opacity: 0
                        }
                    }

                    Label {
                        text: root.gridIconSize + " px"
                        Layout.preferredWidth: 34
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.RightButton
                onClicked: (mouse) => {
                    root.activated()
                    emptyContextMenu.popup()
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: root.controller.directoryModel.loading ? 52 : 36
                visible: root.statusMessage.length > 0 || root.controller.directoryModel.loading
                color: Theme.statusRailFill
                border.color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, themeController.isDark ? 0.75 : 0.85)
                border.width: 1
                radius: 0

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    color: Theme.accent
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8
                    visible: root.statusMessage.length > 0 || root.controller.directoryModel.loading
                    opacity: root.active ? 1.0 : 0.9

                    BusyIndicator {
                        implicitWidth: 16
                        implicitHeight: 16
                        running: root.controller.directoryModel.loading
                        visible: running
                    }

                    Rectangle {
                        implicitWidth: 8
                        implicitHeight: 8
                        radius: 4
                        visible: !root.controller.directoryModel.loading
                        color: Theme.accent
                        opacity: 0.9
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Label {
                            Layout.fillWidth: true
                            text: root.controller.directoryModel.loading
                                  ? "Scanning folder"
                                  : root.statusMessage
                            color: root.controller.directoryModel.loading ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: root.controller.directoryModel.loading ? 12 : 12
                            font.weight: root.controller.directoryModel.loading ? Font.Medium : Font.Normal
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.controller.directoryModel.loading
                            text: "Reading items from " + root.loadingFolderName()
                            color: Theme.textSecondary
                            opacity: 0.85
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }
}

