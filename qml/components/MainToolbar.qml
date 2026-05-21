import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

ToolBar {
    id: root
    
    property alias pathEditorField: pathEditor
    property bool pathEditing: false
    property string pathEditError: ""
    property bool previewVisible: false
    signal previewToggleRequested(bool visible)
    readonly property bool textEditingActive: pathEditing || searchField.activeFocus
    
    height: 64
    
    background: Rectangle {
        readonly property color tintedBase: themeController.isDark
            ? Qt.rgba(
                Theme.surface.r * 0.78 + Theme.accent.r * 0.22,
                Theme.surface.g * 0.78 + Theme.accent.g * 0.22,
                Theme.surface.b * 0.78 + Theme.accent.b * 0.22,
                1.0)
            : Qt.rgba(
                Theme.surface.r * 0.94 + Theme.accent.r * 0.06,
                Theme.surface.g * 0.94 + Theme.accent.g * 0.06,
                Theme.surface.b * 0.94 + Theme.accent.b * 0.06,
                1.0)

        color: tintedBase

        Rectangle {
            anchors.fill: parent
            radius: 0
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                          themeController.isDark ? 0.08 : 0.04)
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: themeController.isDark
                ? Qt.rgba(1, 1, 1, 0.09)
                : Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.5)
        }
        
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, themeController.isDark ? 0.08 : 0.06) }
            GradientStop { position: 0.52; color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.04 : 0.02) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    property bool ignoreTextChange: false

    readonly property var activeController: workspaceController.activePanel === 0
                                            ? workspaceController.leftPanel
                                            : workspaceController.rightPanel
    readonly property string activePath: workspaceController.activePanel === 0
                                         ? workspaceController.leftPanel.currentPath
                                         : workspaceController.rightPanel.currentPath

    function focusPath() {
        root.pathEditError = ""
        pathEditor.text = root.activePath
        root.pathEditing = true
        pathEditor.forceActiveFocus()
        pathEditor.selectAll()
    }

    function acceptPathEdit() {
        const path = pathEditor.text.trim()
        if (path.length > 0) {
            if (root.activeController.openPath(path)) {
                root.pathEditError = ""
                root.pathEditing = false
                workspaceController.focusActivePanel()
                return
            }
            root.pathEditError = "Path not found"
        } else {
            root.pathEditError = "Enter a valid path"
        }
        pathEditor.forceActiveFocus()
        pathEditor.selectAll()
    }

    function cancelPathEdit() {
        root.pathEditing = false
        root.pathEditError = ""
        workspaceController.focusActivePanel()
    }

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function toolbarToneFor(role, active, hovered) {
        let base = Theme.accent
        switch (String(role)) {
        case "back":
            base = "#3b82f6"
            break
        case "forward":
            base = "#8b5cf6"
            break
        case "up":
            base = "#0ea5e9"
            break
        case "view":
            base = "#8b5cf6"
            break
        case "hidden":
            base = "#10b981"
            break
        case "refresh":
            base = "#14b8a6"
            break
        case "copy":
            base = "#3b82f6"
            break
        case "move":
            base = "#f59e0b"
            break
        case "folder":
            base = "#22c55e"
            break
        case "split":
            base = "#a855f7"
            break
        case "theme":
            base = themeController.isDark ? "#f59e0b" : "#6366f1"
            break
        case "info":
            base = "#0ea5e9"
            break
        case "search":
            base = Theme.textSecondary
            break
        }

        if (active) {
            return Qt.lighter(base, themeController.isDark ? 1.14 : 1.08)
        }
        if (hovered) {
            return Qt.lighter(base, themeController.isDark ? 1.10 : 1.05)
        }
        return base
    }

    // Modern Button Component
    component IconButton: ToolButton {
        id: btn
        property string iconSource
        property string iconTone: "default"
        property bool isHighlighted: false
        property int iconSize: 16
        readonly property color hoverFill: {
            if (btn.pressed) {
                return Theme.surfaceActive
            }
            if (btn.hovered || btn.isHighlighted) {
                return themeController.isDark
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                    : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20)
            }
            return "transparent"
        }
        readonly property color hoverBorder: (btn.hovered || btn.isHighlighted)
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.36 : 0.42)
            : "transparent"
        clip: true
        padding: 0

        implicitWidth: 32
        implicitHeight: 32

        background: Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 7
            color: btn.hoverFill
            border.color: btn.hoverBorder
            border.width: btn.hovered || btn.isHighlighted || btn.pressed ? 1 : 0
        }

        contentItem: Item {
            implicitWidth: btn.iconSize
            implicitHeight: btn.iconSize
            Image {
                anchors.centerIn: parent
                width: btn.iconSize
                height: btn.iconSize
                source: btn.iconSource
                sourceSize: Qt.size(32, 32)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: false
                opacity: btn.enabled ? 1.0 : 0.35
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 4

        // --- LEFT: Navigation & Core ---
        RowLayout {
            spacing: 3
            IconButton {
                iconSource: "../assets/lucide-toolbar/arrow-left.svg"
                iconTone: "back"
                enabled: root.activeController.canGoBack
                onClicked: root.activeController.goBack()
                ToolTip.visible: hovered
                ToolTip.text: "Back (Alt+Left)"
            }
            IconButton {
                iconSource: "../assets/lucide-toolbar/arrow-right.svg"
                iconTone: "forward"
                enabled: root.activeController.canGoForward
                onClicked: root.activeController.goForward()
                ToolTip.visible: hovered
                ToolTip.text: "Forward (Alt+Right)"
            }
            IconButton {
                iconSource: "../assets/lucide-toolbar/arrow-up.svg"
                iconTone: "up"
                onClicked: root.activeController.goUp()
                ToolTip.visible: hovered
                ToolTip.text: "Up (Alt+Up)"
            }
            
            Rectangle { width: 1; height: 22; color: Theme.border; opacity: 0.35; Layout.leftMargin: 3; Layout.rightMargin: 3 }
            
            IconButton {
                iconSource: root.activeController.viewMode === 0 
                            ? "../assets/lucide-toolbar/layout-grid.svg" 
                            : (root.activeController.viewMode === 1 
                               ? "../assets/lucide-toolbar/list.svg" 
                               : "../assets/lucide-toolbar/layout-list.svg")
                iconTone: "view"
                onClicked: root.activeController.viewMode = (root.activeController.viewMode + 1) % 3
                ToolTip.visible: hovered
                ToolTip.text: root.activeController.viewMode === 0 
                              ? "Switch to Grid" 
                              : (root.activeController.viewMode === 1 
                                 ? "Switch to Brief" 
                                 : "Switch to Details")
            }
            IconButton {
                iconSource: root.activeController.directoryModel.showHidden ? "../assets/lucide-toolbar/eye-off.svg" : "../assets/lucide-toolbar/eye.svg"
                iconTone: "hidden"
                onClicked: {
                    const newValue = !root.activeController.directoryModel.showHidden
                    root.activeController.directoryModel.showHidden = newValue
                    workspaceController.treeModel.showHidden = newValue
                }
                ToolTip.visible: hovered
                ToolTip.text: root.activeController.directoryModel.showHidden ? "Hide Hidden Files" : "Show Hidden Files"
            }
            IconButton {
                iconSource: "../assets/lucide-toolbar/refresh-cw.svg"
                iconTone: "refresh"
                onClicked: root.activeController.refresh()
                ToolTip.visible: hovered
                ToolTip.text: "Refresh (F5)"
            }
        }

        // --- CENTER: Path Bar Island (Expanded) ---
        Item {
            id: pathIslandContainer
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            
            Connections {
                target: root
                function onPathEditingChanged() {
                    if (!root.pathEditing) {
                        suggestionsPopup.close()
                    }
                }
            }

            Rectangle {
                id: pathIsland
                anchors.centerIn: parent
                width: Math.min(parent.width - 20, 800)
                height: 40
                radius: 12
                
                // Glassmorphic background
                color: root.pathEditing 
                       ? (themeController.isDark ? Qt.rgba(0, 0, 0, 0.45) : Qt.rgba(255, 255, 255, 0.85))
                       : (islandHover.hovered 
                          ? (themeController.isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(0, 0, 0, 0.08))
                          : (themeController.isDark ? Qt.rgba(255, 255, 255, 0.04) : Qt.rgba(0, 0, 0, 0.04)))
                          
                border.color: root.pathEditing 
                              ? (root.pathEditError.length > 0 ? "#f59e0b" : Theme.accent) 
                              : (islandHover.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4) : Theme.border)
                border.width: root.pathEditing ? 2 : 1
                
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }

                HoverHandler {
                    id: islandHover
                }

                // Shadow for premium layered look
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Theme.glassShadow
                    shadowBlur: 8
                    shadowVerticalOffset: 2
                }

                PathBar {
                    id: pathBar
                    anchors.fill: parent
                    anchors.margins: 1
                    controller: root.activeController
                    path: root.activePath
                    readOnly: false
                    onEditRequested: root.focusPath()
                    opacity: root.pathEditing ? 0.0 : 1.0
                    visible: !root.pathEditing || opacity > 0.01
                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
                }

                TextField {
                    id: pathEditor
                    property string originalText: ""
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 40
                    opacity: root.pathEditing ? 1.0 : 0.0
                    visible: root.pathEditing || opacity > 0.01
                    text: root.activePath
                    placeholderText: "Type folder path..."
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textSecondary
                    font.pixelSize: 13
                    verticalAlignment: TextInput.AlignVCenter
                    background: null
                    selectByMouse: true

                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }

                    onTextChanged: {
                        if (root.pathEditing && activeFocus && !root.ignoreTextChange) {
                            originalText = text
                            updateSuggestions()
                        }
                    }

                    function updateSuggestions() {
                        suggestionsModel.clear()
                        const text = pathEditor.text.trim()
                        if (text.length > 0) {
                            const list = root.activeController.getDirectorySuggestions(text)
                            if (list.length > 0) {
                                for (let i = 0; i < list.length; ++i) {
                                    suggestionsModel.append({ "path": list[i] })
                                }
                                suggestionsList.currentIndex = -1
                                suggestionsPopup.open()
                            } else {
                                suggestionsPopup.close()
                            }
                        } else {
                            suggestionsPopup.close()
                        }
                    }

                    Keys.onShortcutOverride: (event) => {
                        if (event.matches(StandardKey.Paste) && workspaceController.hasClipboard) {
                            workspaceController.pasteFromClipboard()
                            event.accepted = true
                        }
                    }
                    
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (suggestionsPopup.visible) {
                                suggestionsPopup.close()
                            }
                            root.acceptPathEdit()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape) {
                            if (suggestionsPopup.visible) {
                                suggestionsPopup.close()
                            } else {
                                root.cancelPathEdit()
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_Down && suggestionsPopup.visible) {
                            let nextIndex = suggestionsList.currentIndex + 1
                            if (nextIndex >= suggestionsModel.count) nextIndex = -1
                            suggestionsList.currentIndex = nextIndex
                            
                            root.ignoreTextChange = true
                            if (nextIndex === -1) {
                                pathEditor.text = pathEditor.originalText
                            } else {
                                pathEditor.text = suggestionsModel.get(nextIndex).path
                            }
                            pathEditor.cursorPosition = pathEditor.text.length
                            root.ignoreTextChange = false
                            
                            event.accepted = true
                        } else if (event.key === Qt.Key_Up && suggestionsPopup.visible) {
                            let nextIndex = suggestionsList.currentIndex - 1
                            if (nextIndex < -1) nextIndex = suggestionsModel.count - 1
                            suggestionsList.currentIndex = nextIndex
                            
                            root.ignoreTextChange = true
                            if (nextIndex === -1) {
                                pathEditor.text = pathEditor.originalText
                            } else {
                                pathEditor.text = suggestionsModel.get(nextIndex).path
                            }
                            pathEditor.cursorPosition = pathEditor.text.length
                            root.ignoreTextChange = false
                            
                            event.accepted = true
                        } else if (event.key === Qt.Key_Tab) {
                            if (suggestionsPopup.visible && suggestionsModel.count > 0) {
                                let index = suggestionsList.currentIndex >= 0 ? suggestionsList.currentIndex : 0
                                let selectedPath = suggestionsModel.get(index).path
                                
                                root.ignoreTextChange = true
                                pathEditor.text = selectedPath
                                pathEditor.cursorPosition = selectedPath.length
                                pathEditor.originalText = selectedPath
                                root.ignoreTextChange = false
                                
                                updateSuggestions()
                                event.accepted = true
                            }
                        }
                    }
                }

                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Press Tab to autocomplete"
                    visible: root.pathEditing && root.pathEditError.length === 0 && suggestionsPopup.visible
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    opacity: 0.6
                }

                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.pathEditError
                    visible: opacity > 0
                    opacity: (root.pathEditError.length > 0 && root.pathEditing) ? 1.0 : 0.0
                    color: themeController.isDark ? "#fcd34d" : "#d97706"
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    
                    background: Rectangle {
                        color: themeController.isDark ? Qt.rgba(245/255, 158/255, 11/255, 0.15) : Qt.rgba(245/255, 158/255, 11/255, 0.1)
                        border.color: themeController.isDark ? Qt.rgba(245/255, 158/255, 11/255, 0.3) : Qt.rgba(245/255, 158/255, 11/255, 0.4)
                        border.width: 1
                        radius: 6
                    }
                    padding: 3
                    leftPadding: 10
                    rightPadding: 10
                    
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }

            Popup {
                id: suggestionsPopup
                property var toolbarRoot: root
                x: pathIsland.x
                y: pathIsland.y + pathIsland.height + 4
                width: pathIsland.width
                height: Math.min(suggestionsList.contentHeight + 10, 200)
                padding: 5
                focus: false
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                
                background: Rectangle {
                    color: Theme.glassSurfaceStrong
                    border.color: Theme.border
                    border.width: 1
                    radius: Theme.radius
                    
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Theme.glassShadow
                        shadowBlur: 10
                        shadowVerticalOffset: 4
                    }
                }
                
                contentItem: ListView {
                    id: suggestionsList
                    property var popup: suggestionsPopup
                    property var editor: root.pathEditorField
                    model: ListModel { id: suggestionsModel }
                    clip: true
                    
                    delegate: ItemDelegate {
                        width: ListView.view ? ListView.view.width : 0
                        height: 32
                        hoverEnabled: true

                        onHoveredChanged: {
                            if (hovered && ListView.view) {
                                ListView.view.currentIndex = index
                            }
                        }
                        
                        background: Rectangle {
                            color: (ListView.view && ListView.view.currentIndex === index)
                                   ? Theme.itemHoverFill 
                                   : "transparent"
                            radius: 4
                        }
                        
                        contentItem: RowLayout {
                            spacing: 8
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            
                            Image {
                                source: "../assets/icons/folder.svg"
                                Layout.preferredWidth: 14
                                Layout.preferredHeight: 14
                                sourceSize: Qt.size(28, 28)
                                layer.enabled: true
                                layer.effect: MultiEffect {
                                    colorization: 1.0
                                    colorizationColor: Theme.textSecondary
                                }
                            }
                            
                            Label {
                                text: model.path
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                font.family: "Consolas"
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                            }
                        }
                        
                        onClicked: {
                            let view = ListView.view
                            if (view) {
                                let ed = view.editor
                                if (ed) {
                                    if (ed.toolbarRoot) ed.toolbarRoot.ignoreTextChange = true
                                    let selectedPath = view.model.get(index).path
                                    ed.text = selectedPath
                                    ed.cursorPosition = selectedPath.length
                                    if (ed.toolbarRoot) ed.toolbarRoot.ignoreTextChange = false
                                }
                                if (view.popup && view.popup.toolbarRoot) {
                                    view.popup.toolbarRoot.acceptPathEdit()
                                }
                            }
                        }
                    }
                }
            }

            SequentialAnimation {
                id: shakeAnimation
                loops: 1
                
                NumberAnimation { target: pathIsland; property: "anchors.horizontalCenterOffset"; to: -8; duration: 50; easing.type: Easing.OutQuad }
                NumberAnimation { target: pathIsland; property: "anchors.horizontalCenterOffset"; to: 8; duration: 50; easing.type: Easing.InOutQuad }
                NumberAnimation { target: pathIsland; property: "anchors.horizontalCenterOffset"; to: -5; duration: 50; easing.type: Easing.InOutQuad }
                NumberAnimation { target: pathIsland; property: "anchors.horizontalCenterOffset"; to: 5; duration: 50; easing.type: Easing.InOutQuad }
                NumberAnimation { target: pathIsland; property: "anchors.horizontalCenterOffset"; to: 0; duration: 50; easing.type: Easing.InQuad }
            }

            Connections {
                target: root
                function onPathEditErrorChanged() {
                    if (root.pathEditError.length > 0) {
                        shakeAnimation.start()
                    }
                }
            }
        }

        // --- RIGHT: Tools & Selection Actions ---
        RowLayout {
            spacing: 3

            // Selection-specific actions (Copy/Move to opposite)
            IconButton {
                iconSource: "../assets/lucide-toolbar/copy.svg"
                iconTone: "copy"
                enabled: workspaceController.splitEnabled 
                         && root.activeController.directoryModel.selectedCount > 0
                         && !workspaceController.operationQueue.busy
                onClicked: workspaceController.copyActiveSelectionToOpposite()
                visible: workspaceController.splitEnabled
                isHighlighted: enabled && hovered
                ToolTip.visible: hovered
                ToolTip.text: "Copy to other panel"
            }
            IconButton {
                iconSource: "../assets/lucide-toolbar/move.svg"
                iconTone: "move"
                enabled: workspaceController.splitEnabled 
                         && root.activeController.directoryModel.selectedCount > 0
                         && !workspaceController.operationQueue.busy
                onClicked: workspaceController.moveActiveSelectionToOpposite()
                visible: workspaceController.splitEnabled
                isHighlighted: enabled && hovered
                ToolTip.visible: hovered
                ToolTip.text: "Move to other panel"
            }

            Rectangle { 
                width: 1; height: 22; color: Theme.border; opacity: 0.35; 
                Layout.leftMargin: 3; Layout.rightMargin: 3;
                visible: workspaceController.splitEnabled
            }

            IconButton {
                iconSource: "../assets/lucide-toolbar/folder-plus.svg"
                iconTone: "folder"
                onClicked: root.activeController.createFolder("New Folder")
                ToolTip.visible: hovered
                ToolTip.text: "Create Folder"
            }

            IconButton {
                id: splitBtn
                iconSource: "../assets/lucide-toolbar/columns-2.svg"
                iconTone: "split"
                isHighlighted: workspaceController.splitEnabled
                onClicked: workspaceController.toggleSplit()
                ToolTip.visible: hovered
                ToolTip.text: "Toggle Split View (F3)"
            }

            IconButton {
                iconSource: "../assets/lucide-toolbar/panel-right.svg"
                iconTone: "info"
                isHighlighted: root.previewVisible
                onClicked: root.previewToggleRequested(!root.previewVisible)
                ToolTip.visible: hovered
                ToolTip.text: root.previewVisible ? "Hide Preview" : "Show Preview"
            }

            IconButton {
                iconSource: themeController.isDark ? "../assets/lucide-toolbar/sun.svg" : "../assets/lucide-toolbar/moon.svg"
                iconTone: "theme"
                onClicked: themeController.mode = themeController.isDark ? 0 : 1
                ToolTip.visible: hovered
                ToolTip.text: "Toggle Theme"
            }

            IconButton {
                iconSource: "../assets/lucide-toolbar/info.svg"
                iconTone: "info"
                onClicked: helpDialog.open()
                ToolTip.visible: hovered
                ToolTip.text: "Help (F1)"
            }

            // Search Field
            Rectangle {
                Layout.preferredWidth: searchField.activeFocus ? 200 : 140
                Layout.preferredHeight: 36
                radius: 10
                color: themeController.isDark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(0,0,0,0.05)
                border.color: searchField.activeFocus ? Theme.accent : "transparent"
                border.width: 1
                
                Behavior on Layout.preferredWidth { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                Image {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: "../assets/lucide-toolbar/search.svg"
                    sourceSize: Qt.size(16, 16)
                    smooth: true
                    mipmap: false
                    opacity: 1
                }

                TextField {
                    id: searchField
                    anchors.fill: parent
                    anchors.leftMargin: 34
                    anchors.rightMargin: 8
                    placeholderText: "Search..."
                    text: root.activeController.directoryModel.filterText
                    onTextChanged: root.activeController.directoryModel.filterText = text
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textSecondary
                    font.pixelSize: 13
                    background: null
                    verticalAlignment: TextInput.AlignVCenter

                    Keys.onShortcutOverride: (event) => {
                        if (event.matches(StandardKey.Paste) && workspaceController.hasClipboard) {
                            workspaceController.pasteFromClipboard()
                            event.accepted = true
                        }
                    }
                    
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Escape) {
                            text = ""
                            root.activeController.directoryModel.filterText = ""
                            workspaceController.focusActivePanel()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            workspaceController.focusActivePanel()
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }
}
