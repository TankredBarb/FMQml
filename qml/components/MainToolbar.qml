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
    property real pathEditProgress: 0.0

    Behavior on pathEditProgress {
        NumberAnimation {
            duration: 150
            easing.type: Easing.InOutCubic
        }
    }
    
    height: 64
    
    background: Rectangle {
        color: Theme.panelSurface

        Rectangle {
            anchors.fill: parent
            radius: 0
            color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.08 : 0.04)
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: themeController.isDark
                ? Theme.withAlpha(Theme.accentText, 0.09)
                : Theme.withAlpha(Theme.border, 0.5)
        }
        
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.accentText, themeController.isDark ? 0.08 : 0.06) }
            GradientStop { position: 0.52; color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.04 : 0.02) }
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
        root.pathEditProgress = 1.0
    }

    function acceptPathEdit() {
        const path = pathEditor.text.trim()
        if (path.length > 0) {
            if (root.activeController.openPath(path)) {
                root.pathEditError = ""
                suggestionsPopup.close()
                root.pathEditing = false
                root.pathEditProgress = 0.0
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
        root.pathEditError = ""
        suggestionsPopup.close()
        if (root.pathEditing || root.pathEditProgress > 0.0) {
            root.pathEditing = false
            root.pathEditProgress = 0.0
            workspaceController.focusActivePanel()
        } else {
            workspaceController.focusActivePanel()
        }
    }

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function openThemeSelector() {
        themeMenu.openAt(themeBtn)
    }

    function openThemeImportDialog() {
        themeMenu.openImportDialog()
    }

    function openThemeExportDialog() {
        themeMenu.openExportDialog()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 6

        // --- LEFT: Navigation & Core ---
        RowLayout {
            spacing: 6
            
            // Segmented Navigation Group
            Rectangle {
                Layout.preferredHeight: 32
                Layout.preferredWidth: 32 * 3 + 2
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.surface, themeController.isDark ? 0.32 : 0.18)
                border.color: Theme.withAlpha(Theme.border, 0.85)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    IconButton {
                        id: backBtn
                        iconSource: "../assets/lucide-toolbar/arrow-left.svg"
                        iconTone: "back"
                        enabled: root.activeController.canGoBack
                        onClicked: root.activeController.goBack()
                        ToolTip.visible: hovered
                        ToolTip.text: "Back (Alt+Left)"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: backBtn.pressed ? Theme.surfaceActive : (backBtn.hovered ? Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.14 : 0.10) : "transparent")
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                    Rectangle {
                        width: 1
                        Layout.fillHeight: true
                        Layout.topMargin: 6
                        Layout.bottomMargin: 6
                        color: Theme.withAlpha(Theme.border, 0.35)
                    }
                    IconButton {
                        id: forwardBtn
                        iconSource: "../assets/lucide-toolbar/arrow-right.svg"
                        iconTone: "forward"
                        enabled: root.activeController.canGoForward
                        onClicked: root.activeController.goForward()
                        ToolTip.visible: hovered
                        ToolTip.text: "Forward (Alt+Right)"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: forwardBtn.pressed ? Theme.surfaceActive : (forwardBtn.hovered ? Theme.withAlpha(Theme.categoryNavigation, themeController.isDark ? 0.14 : 0.10) : "transparent")
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                    Rectangle {
                        width: 1
                        Layout.fillHeight: true
                        Layout.topMargin: 6
                        Layout.bottomMargin: 6
                        color: Theme.withAlpha(Theme.border, 0.35)
                    }
                    IconButton {
                        id: upBtn
                        iconSource: "../assets/lucide-toolbar/arrow-up.svg"
                        iconTone: "up"
                        onClicked: root.activeController.goUp()
                        ToolTip.visible: hovered
                        ToolTip.text: "Up (Alt+Up)"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: upBtn.pressed ? Theme.surfaceActive : (upBtn.hovered ? Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.14 : 0.10) : "transparent")
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                }
            }
            
            // Segmented View/Refresh Group
            Rectangle {
                Layout.preferredHeight: 32
                Layout.preferredWidth: 32 * 3 + 2
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.surface, themeController.isDark ? 0.32 : 0.18)
                border.color: Theme.withAlpha(Theme.border, 0.85)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    IconButton {
                        id: viewBtn
                        iconSource: root.activeController.viewMode === 0
                                    ? "../assets/lucide-toolbar/layout-grid.svg"
                                    : (root.activeController.viewMode === 1
                                       ? "../assets/lucide-toolbar/layout-list.svg"
                                       : "../assets/lucide-toolbar/list.svg")
                        iconTone: root.activeController.viewMode === 0
                                  ? "view-grid"
                                  : (root.activeController.viewMode === 1
                                     ? "view-brief"
                                     : "view-details")
                        onClicked: root.activeController.viewMode = (root.activeController.viewMode + 1) % 3
                        ToolTip.visible: hovered
                        ToolTip.text: root.activeController.viewMode === 0 
                                      ? "Switch to Grid" 
                                      : (root.activeController.viewMode === 1 
                                         ? "Switch to Brief" 
                                         : "Switch to Details")
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: viewBtn.pressed ? Theme.surfaceActive : (viewBtn.hovered ? Theme.withAlpha(viewBtn.baseTone, themeController.isDark ? 0.20 : 0.14) : "transparent")
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                    Rectangle {
                        width: 1
                        Layout.fillHeight: true
                        Layout.topMargin: 6
                        Layout.bottomMargin: 6
                        color: Theme.border
                        opacity: 0.35
                    }
                    IconButton {
                        id: eyeBtn
                        iconSource: root.activeController.directoryModel.showHidden ? "../assets/lucide-toolbar/eye-off.svg" : "../assets/lucide-toolbar/eye.svg"
                        iconTone: "hidden"
                        onClicked: {
                            const newValue = !root.activeController.directoryModel.showHidden
                            root.activeController.directoryModel.showHidden = newValue
                            workspaceController.treeModel.showHidden = newValue
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: root.activeController.directoryModel.showHidden ? "Hide Hidden Files" : "Show Hidden Files"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: eyeBtn.pressed ? Theme.surfaceActive : (eyeBtn.hovered ? Theme.withAlpha(Theme.categoryUtility, themeController.isDark ? 0.14 : 0.10) : "transparent")
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                    Rectangle {
                        width: 1
                        Layout.fillHeight: true
                        Layout.topMargin: 6
                        Layout.bottomMargin: 6
                        color: Theme.border
                        opacity: 0.35
                    }
                    IconButton {
                        id: refreshBtn
                        iconSource: "../assets/lucide-toolbar/refresh-cw.svg"
                        iconTone: "refresh"
                        onClicked: root.activeController.refresh()
                        ToolTip.visible: hovered
                        ToolTip.text: "Refresh (F5)"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: refreshBtn.pressed ? Theme.surfaceActive : (refreshBtn.hovered ? Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.14 : 0.10) : "transparent")
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                }
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
                radius: Theme.panelRadius

                color: root.pathEditing
                       ? Theme.panelSurfaceStrong
                       : (islandHover.hovered
                          ? Theme.panelSurfaceSoft
                          : Theme.panelSurface)

                border.color: root.pathEditing
                              ? (root.pathEditError.length > 0 ? Theme.danger : Theme.focusRing)
                              : (islandHover.hovered ? Theme.withAlpha(Theme.accent, 0.36) : Theme.panelBorder)
                border.width: root.pathEditing ? 2 : 1

                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }

                HoverHandler {
                    id: islandHover
                }

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Theme.glassShadow
                    shadowBlur: 10 + (root.pathEditProgress * 4)
                    shadowVerticalOffset: 2 + (root.pathEditProgress * 2)
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 4
                    radius: Theme.panelRadius
                    color: root.pathEditing
                           ? (root.pathEditError.length > 0 ? Theme.danger : Theme.categoryInfo)
                           : Theme.withAlpha(Theme.categoryInfo, islandHover.hovered ? 0.9 : 0.65)
                    opacity: root.pathEditing ? 1.0 : 0.85
                }

                Rectangle {
                    id: editGlow
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"
                    visible: root.pathEditing
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.withAlpha(Theme.categoryInfo, 0.14) }
                        GradientStop { position: 0.42; color: "transparent" }
                        GradientStop { position: 1.0; color: Theme.withAlpha(Theme.warmAccent, 0.06) }
                    }
                }

                Label {
                    id: inlinePathKind
                    anchors.left: parent.left
                    anchors.leftMargin: 18 + (10 * root.pathEditProgress)
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.pathEditing || root.pathEditProgress > 0.0
                    readonly property string kindValue: {
                        const currentPath = root.activeController && root.activeController.currentPath
                            ? String(root.activeController.currentPath).toLowerCase()
                            : ""
                        if (currentPath.startsWith("archive://")) {
                            return "archive"
                        }
                        if (currentPath.startsWith("devices://")) {
                            return "devices"
                        }
                        return "path"
                    }
                    readonly property color kindColor: {
                        if (kindValue === "archive") {
                            return Theme.warmAccent
                        }
                        if (kindValue === "devices") {
                            return Theme.categorySystem
                        }
                        return Theme.categoryInfo
                    }
                    text: {
                        return kindValue
                    }
                    color: kindColor
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    opacity: 0.78
                    padding: 0
                }

                PathBar {
                    id: pathBar
                    anchors.fill: parent
                    anchors.leftMargin: 18 + (10 * root.pathEditProgress)
                    anchors.rightMargin: 4
                    anchors.topMargin: 1
                    anchors.bottomMargin: 1
                    controller: root.activeController
                    path: root.activePath
                    readOnly: false
                    onEditRequested: root.focusPath()
                    opacity: 1.0 - root.pathEditProgress
                    visible: root.pathEditProgress < 0.99
                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
                }

                PremiumTextField {
                    id: pathEditor
                    property string originalText: ""
                    anchors.fill: parent
                    anchors.leftMargin: 58 + (18 * root.pathEditProgress)
                    anchors.rightMargin: 42
                    opacity: root.pathEditProgress
                    visible: root.pathEditing || root.pathEditProgress > 0.01
                    text: root.activePath
                    placeholderText: "Type folder path..."
                    background: null
                    leftPadding: 0
                    rightPadding: 0
                    font.family: "Cascadia Code, Consolas, monospace"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    font.letterSpacing: -0.15
                    selectByMouse: true

                    placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.72)
                    color: Theme.textPrimary
                    selectionColor: Theme.withAlpha(Theme.categoryInfo, 0.30)
                    selectedTextColor: Theme.accentText
                    cursorDelegate: Rectangle {
                        width: 2
                        radius: 1
                        color: root.pathEditError.length > 0 ? Theme.danger : Theme.categoryInfo
                    }

                    onActiveFocusChanged: {
                        if (!activeFocus && root.pathEditing) {
                            Qt.callLater(() => {
                                if (root.pathEditing && !pathEditor.activeFocus) {
                                    root.cancelPathEdit()
                                }
                            })
                        }
                    }

                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
                    Behavior on anchors.leftMargin { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

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

                Rectangle {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    visible: (root.pathEditing || root.pathEditProgress > 0.0) && root.pathEditError.length === 0 && suggestionsPopup.visible
                    width: 128
                    height: 22
                    radius: 11
                    color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.14 : 0.10)
                    border.color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.42 : 0.34)
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Rectangle {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            radius: 5
                            color: Theme.categoryInfo

                            Label {
                                anchors.centerIn: parent
                                text: "Tab"
                                color: Theme.accentText
                                font.pixelSize: 8
                                font.weight: Font.Bold
                            }
                        }

                        Label {
                            text: "autocomplete"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeCaption
                            font.weight: Font.Medium
                        }
                    }
                }

                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.pathEditError
                    visible: opacity > 0
                    opacity: (root.pathEditError.length > 0 && (root.pathEditing || root.pathEditProgress > 0.0)) ? 1.0 : 0.0
                    color: Theme.danger
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.Medium
                    
                    background: Rectangle {
                        color: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.15 : 0.10)
                        border.color: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.30 : 0.40)
                        border.width: 1
                        radius: Theme.radiusSm
                    }
                    padding: 3
                    leftPadding: 10
                    rightPadding: 10
                    topPadding: 4
                    bottomPadding: 4
                    
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
                    border.color: Theme.withAlpha(Theme.border, 0.85)
                    border.width: 1
                    radius: Theme.radiusSm
                    
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
                            radius: Theme.radiusSm
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
            spacing: 6

            // Segmented Copy/Move Selection Group
            Rectangle {
                Layout.preferredHeight: 32
                Layout.preferredWidth: 32 * 2 + 1
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.surface, themeController.isDark ? 0.32 : 0.18)
                border.color: Theme.withAlpha(Theme.border, 0.85)
                border.width: 1
                visible: workspaceController.splitEnabled

                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    IconButton {
                        id: copyBtn
                        iconSource: "../assets/lucide-toolbar/copy.svg"
                        iconTone: "copy"
                        enabled: workspaceController.splitEnabled 
                                 && root.activeController.directoryModel.selectedCount > 0
                                 && !workspaceController.operationQueue.busy
                        onClicked: workspaceController.copyActiveSelectionToOpposite()
                        isHighlighted: enabled && hovered
                        ToolTip.visible: hovered
                        ToolTip.text: "Copy to other panel"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: copyBtn.pressed ? Theme.surfaceActive : (copyBtn.hovered ? Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.14 : 0.10) : "transparent")
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                    Rectangle {
                        width: 1
                        Layout.fillHeight: true
                        Layout.topMargin: 6
                        Layout.bottomMargin: 6
                        color: Theme.border
                        opacity: 0.35
                    }
                    IconButton {
                        id: moveBtn
                        iconSource: "../assets/lucide-toolbar/move.svg"
                        iconTone: "move"
                        enabled: workspaceController.splitEnabled 
                                 && root.activeController.directoryModel.selectedCount > 0
                                 && !workspaceController.operationQueue.busy
                        onClicked: workspaceController.moveActiveSelectionToOpposite()
                        isHighlighted: enabled && hovered
                        ToolTip.visible: hovered
                        ToolTip.text: "Move to other panel"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: moveBtn.pressed ? Theme.surfaceActive : (moveBtn.hovered ? Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.14 : 0.10) : "transparent")
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                }
            }

            // Create Folder Button (independent, subtle border)
            IconButton {
                iconSource: "../assets/lucide-toolbar/folder-plus.svg"
                iconTone: "folder"
                enabled: root.activeController.currentPath ? !root.activeController.currentPath.toLowerCase().startsWith("archive://") : true
                onClicked: root.activeController.createFolder("New Folder")
                ToolTip.visible: hovered
                ToolTip.text: "Create Folder"
            }

            // Segmented Panel Toggles (Split & Preview)
            Rectangle {
                Layout.preferredHeight: 32
                Layout.preferredWidth: 32 * 2 + 1
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.surface, themeController.isDark ? 0.32 : 0.18)
                border.color: Theme.withAlpha(Theme.border, 0.85)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    IconButton {
                        id: layoutSplitBtn
                        iconSource: "../assets/lucide-toolbar/columns-2.svg"
                        iconTone: "split"
                        isHighlighted: workspaceController.splitEnabled
                        onClicked: workspaceController.toggleSplit()
                        ToolTip.visible: hovered
                        ToolTip.text: "Toggle Split View (F3)"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: layoutSplitBtn.pressed ? Theme.surfaceActive : (layoutSplitBtn.hovered || layoutSplitBtn.isHighlighted ? Theme.withAlpha(Theme.categoryNavigation, themeController.isDark ? 0.16 : 0.12) : "transparent")
                            border.color: layoutSplitBtn.isHighlighted ? Theme.accent : "transparent"
                            border.width: layoutSplitBtn.isHighlighted ? 1 : 0
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                    Rectangle {
                        width: 1
                        Layout.fillHeight: true
                        Layout.topMargin: 6
                        Layout.bottomMargin: 6
                        color: Theme.border
                        opacity: 0.35
                    }
                    IconButton {
                        id: layoutPreviewBtn
                        iconSource: "../assets/lucide-toolbar/panel-right.svg"
                        iconTone: "info"
                        isHighlighted: root.previewVisible
                        onClicked: root.previewToggleRequested(!root.previewVisible)
                        ToolTip.visible: hovered
                        ToolTip.text: root.previewVisible ? "Hide Preview" : "Show Preview"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: layoutPreviewBtn.pressed ? Theme.surfaceActive : (layoutPreviewBtn.hovered || layoutPreviewBtn.isHighlighted ? Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.16 : 0.12) : "transparent")
                            border.color: layoutPreviewBtn.isHighlighted ? Theme.accent : "transparent"
                            border.width: layoutPreviewBtn.isHighlighted ? 1 : 0
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                }
            }

            // Segmented Meta Actions (Theme & Help)
            Rectangle {
                Layout.preferredHeight: 32
                Layout.preferredWidth: 32 * 2 + 1
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.surface, themeController.isDark ? 0.32 : 0.18)
                border.color: Theme.withAlpha(Theme.border, 0.85)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    IconButton {
                        id: themeBtn
                        iconSource: "../assets/icons/settings.svg"
                        iconTone: "theme"
                        onClicked: root.openThemeSelector()
                        ToolTip.visible: hovered
                        ToolTip.text: themeController.customThemeLoaded ? ("Theme Schemes · Custom") : ("Theme Schemes · " + themeController.schemeName)
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: themeBtn.pressed ? Theme.surfaceActive : (themeBtn.hovered ? Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.14 : 0.10) : "transparent")
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                    ThemeSelectorMenu {
                        id: themeMenu
                    }
                    Rectangle {
                        width: 1
                        Layout.fillHeight: true
                        Layout.topMargin: 6
                        Layout.bottomMargin: 6
                        color: Theme.border
                        opacity: 0.35
                    }
                    IconButton {
                        id: helpBtn
                        iconSource: "../assets/lucide-toolbar/info.svg"
                        iconTone: "info"
                        onClicked: helpDialog.open()
                        ToolTip.visible: hovered
                        ToolTip.text: "Help (F1)"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: helpBtn.pressed ? Theme.surfaceActive : (helpBtn.hovered ? Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.14 : 0.10) : "transparent")
                            anchors.fill: parent
                            anchors.margins: 1
                        }
                    }
                }
            }

            // Search Field
            Rectangle {
                Layout.preferredWidth: searchField.activeFocus ? 200 : 140
                Layout.preferredHeight: 32
                radius: Theme.controlRadius
                color: Theme.panelSurfaceSoft
                border.color: searchField.activeFocus ? Theme.focusRing : Theme.withAlpha(Theme.border, 0.5)
                border.width: 1
                
                Behavior on Layout.preferredWidth { 
                    NumberAnimation { 
                        duration: 200
                        easing.type: Easing.OutQuint 
                    } 
                }

                Image {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: 14
                    source: "../assets/lucide-toolbar/search.svg"
                    sourceSize: Qt.size(16, 16)
                    smooth: true
                    mipmap: false
                    opacity: 0.8
                }

                PremiumTextField {
                    id: searchField
                    anchors.fill: parent
                    anchors.leftMargin: 30
                    anchors.rightMargin: 8
                    placeholderText: "Search..."
                    text: root.activeController.directoryModel.filterText
                    onTextChanged: root.activeController.directoryModel.filterText = text
                    background: null

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
