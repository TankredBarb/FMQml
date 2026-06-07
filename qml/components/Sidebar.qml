import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import "common"
import "../style"

Pane {
    id: root

    padding: 0

    property alias placesList: placesList
    property alias foldersTree: foldersTree
    property bool lastFocusedTree: false
    property bool trapTabNavigation: false
    property int selectedPlaceIndex: -2
    property string selectedPlacePath: ""
    property bool liveResizeActive: false
    property bool treeScrollActive: false
    property string pendingTreePreviewPath: ""
    property int treeSyncRequestId: 0
    property string treeSyncTargetPath: ""
    property var activePanelViewProvider: null
    readonly property bool ultraLightMode: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.ultraLightMode
                                           : false
    readonly property bool effectsReduced: root.liveResizeActive || root.ultraLightMode
    readonly property color sidebarSelectedFill: Theme.withAlpha(
        Theme.activeAccent,
        themeController.isDark ? 0.34 : 0.28)
    readonly property color sidebarCurrentFill: Theme.withAlpha(
        Theme.activeAccent,
        themeController.isDark ? 0.18 : 0.14)

    function sidebarStateFill(selected, current, hovered, pressed) {
        if (selected) {
            return root.sidebarSelectedFill
        }
        if (current) {
            return root.sidebarCurrentFill
        }
        if (pressed) {
            return Theme.surfaceActive
        }
        if (hovered) {
            return Theme.itemNeutralHoverFill
        }
        return "transparent"
    }

    function focusSidebar(trapTab) {
        trapTabNavigation = trapTab === true
        if (lastFocusedTree) {
            foldersTree.forceActiveFocus()
        } else {
            placesList.forceActiveFocus()
        }
    }

    function clearPlaceSelection() {
        root.selectedPlaceIndex = -2
        root.selectedPlacePath = ""
    }

    function placePathForIndex(index) {
        if (index === -1) {
            return "devices://"
        }
        if (index < 0 || index >= placesList.count) {
            return ""
        }
        const modelIndex = workspaceController.placesModel.index(index, 0)
        return workspaceController.placesModel.data(modelIndex, Qt.UserRole + 2 /* PathRole */) || ""
    }

    function activePanelMatchesSelectedPlace() {
        if (root.selectedPlaceIndex === -2 || root.selectedPlacePath.length === 0) {
            return true
        }

        const panel = root.activePanelController()
        if (!panel) {
            return false
        }

        if (root.selectedPlacePath === "devices://") {
            return panel.isDeviceRoot
        }

        return !panel.isDeviceRoot && root.pathsEqual(panel.currentPath, root.selectedPlacePath)
    }

    function updatePlaceSelectionForActivePath() {
        if (!root.activePanelMatchesSelectedPlace()) {
            root.clearPlaceSelection()
        }
    }

    function setSelectedPlaceIndex(index) {
        const path = root.placePathForIndex(index)
        if (path.length === 0) {
            root.clearPlaceSelection()
            return false
        }

        root.selectedPlaceIndex = index
        root.selectedPlacePath = path
        return true
    }

    function syncTreeToActivePath() {
        syncTimer.restart()
    }

    function treeScrollInputActive() {
        return foldersTree.activeFocus
            || foldersTreeHover.hovered
            || foldersTreeVerticalScrollBar.pressed
    }

    function markTreeScrollActivity() {
        if (!root.treeScrollInputActive()) {
            return
        }
        root.treeScrollActive = true
        treeScrollStopTimer.restart()
    }

    function clearTreeSelection() {
        if (foldersTree.selectionModel) {
            foldersTree.selectionModel.clear()
        }
    }

    function selectTreeIndex(index) {
        if (!index || !index.valid) return false

        // QML TreeView doesn't have expandToIndex. We must expand ancestors top-down.
        let current = index
        let ancestors = []
        while (current && current.valid) {
            let p = workspaceController.treeModel.parentIndex(current)
            if (p && p.valid) {
                ancestors.unshift(p)
                current = p
            } else {
                break
            }
        }

        for (let i = 0; i < ancestors.length; ++i) {
            let r = foldersTree.rowAtIndex(ancestors[i])
            if (r >= 0) {
                if (!foldersTree.isExpanded(r)) {
                    foldersTree.expand(r)
                    foldersTree.forceLayout()
                }
            }
        }

        let finalRow = foldersTree.rowAtIndex(index)
        if (finalRow < 0) {
            foldersTree.forceLayout()
            finalRow = foldersTree.rowAtIndex(index)
        }

        if (finalRow < 0) return false

        if (foldersTree.selectionModel) {
            foldersTree.selectionModel.setCurrentIndex(index,
                ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows | ItemSelectionModel.Current)
        }
        foldersTree.positionViewAtRow(finalRow, TableView.Contain)
        return true
    }

    function activePanelController() {
        return workspaceController.activePanel === 0
            ? workspaceController.leftPanel
            : workspaceController.rightPanel
    }

    function activePanelView() {
        return root.activePanelViewProvider ? root.activePanelViewProvider() : null
    }

    function openPathInActivePanel(path) {
        if (!path) return
        const panelView = root.activePanelView()
        if (panelView && panelView.openPath) {
            panelView.openPath(path)
            return
        }
        const panel = root.activePanelController()
        if (panel) panel.openPath(path)
    }

    function prepareNavigation(reason) {
        const panelView = root.activePanelView()
        if (panelView && panelView.cancelInlineRenameForNavigation) {
            panelView.cancelInlineRenameForNavigation(reason)
        }
    }

    function previewPath(path) {
        if (!path || typeof quickLookController === "undefined" || !quickLookController) return
        if (root.treeScrollActive) {
            root.pendingTreePreviewPath = path
            return
        }
        quickLookController.preview(path)
    }

    function previewCurrentPlace() {
        if (!placesList.activeFocus) return

        if (placesList.currentIndex === -1) {
            root.previewPath("devices://")
            return
        }

        if (placesList.currentIndex < 0 || placesList.currentIndex >= placesList.count) return

        const modelIndex = workspaceController.placesModel.index(placesList.currentIndex, 0)
        const path = workspaceController.placesModel.data(modelIndex, Qt.UserRole + 2 /* PathRole */)
        root.previewPath(path)
    }

    function setPlaceCurrentIndex(index) {
        const validIndex = index === -1 || (index >= 0 && index < placesList.count)
        placesList.currentIndex = validIndex ? index : -1
        if (index === -1) {
            placesList.positionViewAtBeginning()
        } else if (index >= 0 && index < placesList.count) {
            placesList.positionViewAtIndex(index, ListView.Contain)
        }
    }

    function previewCurrentFolderTreeItem() {
        if (!foldersTree.activeFocus || !foldersTree.selectionModel) return

        const idx = foldersTree.selectionModel.currentIndex
        if (idx === undefined || idx === null || !idx.valid) return

        root.previewPath(workspaceController.treeModel.pathForIndex(idx))
    }

    function selectPlace(index) {
        root.trapTabNavigation = false
        placesList.forceActiveFocus()
        root.setPlaceCurrentIndex(index)
        if (!root.setSelectedPlaceIndex(index)) {
            return
        }
        root.previewCurrentPlace()
    }

    function openSelectedPlace() {
        const index = placesList.currentIndex
        if (index < -1 || index >= placesList.count) {
            return
        }

        if (!root.setSelectedPlaceIndex(index)) {
            return
        }

        if (index === -1) {
            root.openPathInActivePanel("devices://")
            return
        }

        const modelIndex = workspaceController.placesModel.index(index, 0)
        const path = workspaceController.placesModel.data(modelIndex, Qt.UserRole + 2 /* PathRole */)
        root.openPathInActivePanel(path)
    }

    function resetPlaceDriveMenu() {
        placeDriveContextMenu.reset()
    }

    function closePlaceDriveMenuForPath(path) {
        if (root.pathsEqual(placeDriveContextMenu.drivePath, path)) {
            placeDriveContextMenu.close()
            root.resetPlaceDriveMenu()
        }
    }

    function openPlaceDriveMenu(index, path, driveType, canEject, isDrive) {
        if (!isDrive || !path) return

        placeDriveContextMenu.driveIndex = index
        placeDriveContextMenu.drivePath = path
        placeDriveContextMenu.driveType = driveType || ""
        placeDriveContextMenu.canEject = canEject === true
        placeDriveContextMenu.managedIsoMount = workspaceController.isManagedIsoMountRoot(path)
        placeDriveContextMenu.popup()
    }

    Timer {
        id: syncTimer
        interval: 120
        repeat: false
        onTriggered: {
            let panel = workspaceController.activePanel === 0
                ? workspaceController.leftPanel
                : workspaceController.rightPanel
            
            if (!panel) return;

            root.treeSyncRequestId += 1
            root.treeSyncTargetPath = panel.currentPath || ""

            // Handle virtual root (This PC)
            if (panel.isDeviceRoot) {
                root.clearTreeSelection()
                return
            }

            let targetPath = root.treeSyncTargetPath

            // Keep panel navigation independent from folder enumeration.
            let index = workspaceController.treeModel.nearestLoadedIndexForPath(targetPath, 0)
            
            if (!index || !index.valid) {
                root.clearTreeSelection()
                return
            }

            root.selectTreeIndex(index)
            workspaceController.treeModel.revealPathAsync(targetPath, root.treeSyncRequestId)
        }
    }

    Timer {
        id: treeScrollStopTimer
        interval: 160
        repeat: false
        onTriggered: {
            root.treeScrollActive = false
            if (root.pendingTreePreviewPath.length > 0) {
                const path = root.pendingTreePreviewPath
                root.pendingTreePreviewPath = ""
                root.previewPath(path)
            }
        }
    }

    function pathsEqual(lhs, rhs) {
        if (!lhs || !rhs) return false;
        
        // Strip trailing slashes
        let cleanLhs = lhs.replace(/[/\\]$/, "")
        let cleanRhs = rhs.replace(/[/\\]$/, "")

        if (Qt.platform.os === "windows") {
            let eq = cleanLhs.toLowerCase() === cleanRhs.toLowerCase()
            // console.log("[Sidebar] pathsEqual(win) lhs:", lhs, "rhs:", rhs, "eq:", eq) // too noisy, let's keep quiet for now or only log if close
            return eq
        }
        return cleanLhs === cleanRhs
    }

    function iconSourceFor(name) {
        const iconName = String(name || "")
        if (iconName.length === 0) {
            return ""
        }
        if (iconName === "drive") {
            return "../assets/icons/hard-drive.svg"
        }
        return "../assets/icons/" + iconName + ".svg"
    }

    function iconToneFor(name, active, hovered) {
        let base = Theme.textSecondary
        switch (String(name)) {
        case "computer":
            base = Theme.actionIconColor("system")
            break
        case "home":
            base = Theme.actionIconColor("folder")
            break
        case "desktop":
            base = Theme.actionIconColor("navigation")
            break
        case "download":
            base = Theme.actionIconColor("action")
            break
        case "document":
            base = Theme.actionIconColor("document")
            break
        case "image":
            base = Theme.actionIconColor("image")
            break
        case "music":
        case "video":
            base = Theme.actionIconColor("media")
            break
        case "drive":
        case "hard-drive":
            base = Theme.actionIconColor("drive")
            break
        case "folder":
        case "file-manager":
            base = Theme.actionIconColor("folder")
            break
        case "star":
            base = Theme.actionIconColor("favorite")
            break
        default:
            base = Theme.actionIconColor("default")
            break
        }

        if (active) {
            return Qt.lighter(base, themeController.isDark ? 1.12 : 1.05)
        }
        if (hovered) {
            return Qt.lighter(base, themeController.isDark ? 1.08 : 1.03)
        }
        return base
    }

    background: Rectangle {
        radius: Theme.panelRadius
        topLeftRadius: 0
        bottomLeftRadius: 0
        color: Theme.panelSurface

        Rectangle {
            anchors.fill: parent
            radius: Theme.innerRadius(parent.radius, 1)
            topLeftRadius: 0
            bottomLeftRadius: 0
            color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.028 : 0.018)
        }

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: Theme.panelRadius
            anchors.bottomMargin: Theme.panelRadius
            width: 1
            color: themeController.isDark
                ? Theme.withAlpha(Theme.accent, 0.10)
                : Theme.panelStrokeStrong
        }

        border.color: themeController.isDark
            ? Theme.withAlpha(Theme.accent, 0.12)
            : Theme.panelStroke
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 8
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 7
            spacing: 10

            Rectangle {
                width: 10
                height: 10
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.accent, 0.92)
                border.color: Theme.withAlpha(Theme.accent, 0.28)
                border.width: 1
            }

            Label {
                text: "Places"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.2
                color: Theme.textPrimary
                opacity: 0.82
            }
        }

        ListView {
            id: placesList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 1
            model: workspaceController.placesModel
            clip: true
            interactive: contentHeight > height
            focus: true
            focusPolicy: Qt.StrongFocus

            onActiveFocusChanged: {
                if (activeFocus) {
                    lastFocusedTree = false
                    root.previewCurrentPlace()
                }
            }

            onCurrentIndexChanged: root.previewCurrentPlace()

            Keys.onTabPressed: function(event) {
                if (root.trapTabNavigation) {
                    foldersTree.forceActiveFocus()
                    event.accepted = true
                }
            }

            Keys.onBacktabPressed: function(event) {
                if (root.trapTabNavigation) {
                    foldersTree.forceActiveFocus()
                    event.accepted = true
                }
            }

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Up) {
                    if (placesList.currentIndex > 0) {
                        root.setPlaceCurrentIndex(placesList.currentIndex - 1)
                    } else if (placesList.currentIndex === 0) {
                        root.setPlaceCurrentIndex(-1) // Focus "This PC" (header)
                    } else {
                        root.setPlaceCurrentIndex(placesList.count - 1) // Wrap around
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Down) {
                    if (placesList.currentIndex === -1) {
                        root.setPlaceCurrentIndex(0)
                    } else if (placesList.currentIndex < placesList.count - 1) {
                        root.setPlaceCurrentIndex(placesList.currentIndex + 1)
                    } else {
                        root.setPlaceCurrentIndex(-1) // Wrap to "This PC"
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.openSelectedPlace()
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    workspaceController.focusActivePanel()
                    event.accepted = true
                }
            }

            header: Item {
                width: placesList.width
                height: 40

                readonly property bool isActive: {
                    return root.selectedPlaceIndex === -1
                }

                readonly property bool hasKeyboardCurrent: placesList.activeFocus && placesList.currentIndex === -1

                Rectangle {
                    id: thisPcBg
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    radius: Theme.radiusMd

                    color: root.sidebarStateFill(parent.isActive,
                                                 parent.hasKeyboardCurrent,
                                                 thisPcMouse.containsMouse,
                                                 thisPcMouse.containsPress)
                    border.color: "transparent"
                    border.width: 0

                    Behavior on color {
                        enabled: !root.effectsReduced
                        ColorAnimation { duration: Theme.motionFast }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 6
                        spacing: 10

                        RecolorSvgIcon {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            sourcePath: "../assets/icons/computer.svg"
                            recolorColor: root.iconToneFor("computer", thisPcBg.parent.isActive || thisPcBg.parent.hasKeyboardCurrent, thisPcMouse.containsMouse)
                            cacheKey: "sidebar"
                            sourceSize: Qt.size(40, 40)
                            asynchronous: true
                            cache: true
                            opacity: thisPcBg.parent.isActive || thisPcBg.parent.hasKeyboardCurrent || thisPcMouse.containsMouse ? 1 : 0.86
                        }

                        Label {
                            text: "This PC"
                            Layout.fillWidth: true
                            font.pixelSize: 13
                            font.weight: thisPcBg.parent.isActive || thisPcBg.parent.hasKeyboardCurrent ? Font.Medium : Font.Normal
                            color: Theme.textPrimary
                            opacity: thisPcBg.parent.isActive || thisPcBg.parent.hasKeyboardCurrent ? 1.0 : 0.92
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: thisPcMouse
                        anchors.fill: parent
                        hoverEnabled: !root.effectsReduced
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor
                        onPressed: function(mouse) {
                            root.selectPlace(-1)
                        }
                        onClicked: function(mouse) {
                            mouse.accepted = true
                        }
                        onDoubleClicked: function(mouse) {
                            root.openPathInActivePanel("devices://")
                            mouse.accepted = true
                        }
                    }
                }
            }

            delegate: ItemDelegate {
                id: placeDelegate
                width: placesList.width
                height: 40
                padding: 0
                focusPolicy: Qt.NoFocus

                readonly property bool isActive: root.selectedPlaceIndex === index

                readonly property bool hasKeyboardCurrent: placesList.activeFocus && placesList.currentIndex === index

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 12
                    spacing: 10

                    RecolorSvgIcon {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        sourcePath: root.iconSourceFor(model.icon)
                        recolorColor: root.iconToneFor(model.icon, placeDelegate.isActive || placeDelegate.hasKeyboardCurrent, placeMouse.containsMouse)
                        cacheKey: "sidebar"
                        sourceSize: Qt.size(40, 40)
                        asynchronous: true
                        cache: true
                        opacity: placeDelegate.isActive || placeDelegate.hasKeyboardCurrent || placeMouse.containsMouse ? 1 : 0.86
                    }

                    Label {
                        text: model.name
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        font.weight: placeDelegate.isActive || placeDelegate.hasKeyboardCurrent ? Font.Medium : Font.Normal
                        color: Theme.textPrimary
                        opacity: placeDelegate.isActive || placeDelegate.hasKeyboardCurrent ? 1.0 : 0.92
                        elide: Text.ElideRight
                    }
                }

                background: Rectangle {
                    radius: Theme.radiusMd
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6

                    color: root.sidebarStateFill(placeDelegate.isActive,
                                                 placeDelegate.hasKeyboardCurrent,
                                                 placeMouse.containsMouse,
                                                 placeMouse.pressed)
                    border.color: "transparent"
                    border.width: 0

                    Behavior on color {
                        enabled: !root.effectsReduced
                        ColorAnimation { duration: Theme.motionFast }
                    }
                }

                MouseArea {
                    id: placeMouse
                    anchors.fill: parent
                    hoverEnabled: !root.effectsReduced
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    z: 10
                    onPressed: {
                        root.prepareNavigation("sidebar-place-press")
                        root.selectPlace(index)
                    }

                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            root.openPlaceDriveMenu(index, model.path, model.driveType, model.canEject, model.isDrive)
                        }
                        mouse.accepted = true
                    }

                    onDoubleClicked: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            root.openPathInActivePanel(model.path)
                        }
                        mouse.accepted = true
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.topMargin: 10
            Layout.bottomMargin: 10
            color: Theme.panelStrokeStrong
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 7
            spacing: 10

            Rectangle {
                width: 10
                height: 10
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.accent, 0.92)
                border.color: Theme.withAlpha(Theme.accent, 0.28)
                border.width: 1
            }

            Label {
                text: "Folders"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.2
                color: Theme.textPrimary
                opacity: 0.82
            }
        }

        TreeView {
            id: foldersTree
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 1
            model: workspaceController.treeModel
            selectionModel: ItemSelectionModel {
                model: workspaceController.treeModel
            }
            clip: true
            focus: true
            focusPolicy: Qt.StrongFocus

            onContentYChanged: root.markTreeScrollActivity()
            onContentXChanged: root.markTreeScrollActivity()

            HoverHandler {
                id: foldersTreeHover
            }

            onActiveFocusChanged: {
                if (activeFocus) {
                    lastFocusedTree = true
                    let idx = foldersTree.selectionModel ? foldersTree.selectionModel.currentIndex : null
                    if (idx === undefined || idx === null || !idx.valid) {
                        let firstIdx = workspaceController.treeModel.index(0, 0)
                        if (firstIdx && firstIdx.valid) {
                            if (foldersTree.selectionModel) {
                                foldersTree.selectionModel.setCurrentIndex(firstIdx, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows | ItemSelectionModel.Current)
                            }
                        }
                    }
                    root.previewCurrentFolderTreeItem()
                }
            }

            Connections {
                target: foldersTree.selectionModel
                function onCurrentChanged(current, previous) {
                    root.previewCurrentFolderTreeItem()
                }
            }

            Keys.onTabPressed: function(event) {
                if (root.trapTabNavigation) {
                    placesList.forceActiveFocus()
                    event.accepted = true
                }
            }

            Keys.onBacktabPressed: function(event) {
                if (root.trapTabNavigation) {
                    placesList.forceActiveFocus()
                    event.accepted = true
                }
            }

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    let idx = foldersTree.selectionModel ? foldersTree.selectionModel.currentIndex : null
                    if (idx !== undefined && idx !== null && idx.valid) {
                        let path = workspaceController.treeModel.pathForIndex(idx)
                        if (path) {
                            root.openPathInActivePanel(path)
                        }
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Space) {
                    let idx = foldersTree.selectionModel ? foldersTree.selectionModel.currentIndex : null
                    if (idx !== undefined && idx !== null && idx.valid) {
                        let row = foldersTree.rowAtIndex(idx)
                        if (row >= 0) {
                            foldersTree.toggleExpanded(row)
                        }
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Right) {
                    let idx = foldersTree.selectionModel ? foldersTree.selectionModel.currentIndex : null
                    if (idx !== undefined && idx !== null && idx.valid) {
                        let row = foldersTree.rowAtIndex(idx)
                        if (row >= 0) {
                            if (!foldersTree.isExpanded(row)) {
                                foldersTree.expand(row)
                            }
                        }
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Left) {
                    let idx = foldersTree.selectionModel ? foldersTree.selectionModel.currentIndex : null
                    if (idx !== undefined && idx !== null && idx.valid) {
                        let row = foldersTree.rowAtIndex(idx)
                        if (row >= 0) {
                            if (foldersTree.isExpanded(row)) {
                                foldersTree.collapse(row)
                            } else {
                                let parentIdx = workspaceController.treeModel.parentIndex(idx)
                                if (parentIdx && parentIdx.valid) {
                                    if (foldersTree.selectionModel) {
                                        foldersTree.selectionModel.setCurrentIndex(parentIdx, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows | ItemSelectionModel.Current)
                                    }
                                }
                            }
                        }
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    workspaceController.focusActivePanel()
                    event.accepted = true
                }
            }

            delegate: ItemDelegate {
                id: folderDelegate
                required property TreeView treeView
                required property int row
                required property bool isTreeNode
                required property bool expanded
                required property bool hasChildren
                required property bool loading
                required property int depth

                width: foldersTree.width
                implicitWidth: foldersTree.width > 0 ? foldersTree.width : 1
                implicitHeight: 40
                height: implicitHeight
                padding: 0
                focusPolicy: Qt.NoFocus

                readonly property bool isActive: root.pathsEqual(model.path, (
                    workspaceController.activePanel === 0
                        ? workspaceController.leftPanel.currentPath
                        : workspaceController.rightPanel.currentPath
                ))

                readonly property bool isCurrent: {
                    if (!treeView.activeFocus) return false;
                    let model = treeView.selectionModel;
                    if (!model) return false;
                    let cur = model.currentIndex;
                    if (cur === undefined || cur === null) return false;
                    return treeView.rowAtIndex(cur) === row;
                }

                readonly property real baseIndent: 14
                readonly property real indentStep: 20
                readonly property real indicatorSlot: 18
                readonly property real iconSize: 20

                background: Rectangle {
                    radius: Theme.radiusMd
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6

                    color: root.sidebarStateFill(folderDelegate.isActive,
                                                 folderDelegate.isCurrent,
                                                 rowMouse.containsMouse,
                                                 rowMouse.down)
                    border.color: "transparent"
                    border.width: 0

                    Behavior on color {
                        enabled: !root.effectsReduced
                        ColorAnimation { duration: Theme.motionFast }
                    }
                }

                contentItem: Item {
                    anchors.fill: parent

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: !root.effectsReduced
                        cursorShape: Qt.PointingHandCursor
                        z: 1
                        onPressed: root.prepareNavigation("sidebar-tree-press")
                        onClicked: function(mouse) {
                            root.openPathInActivePanel(model.path)
                            root.trapTabNavigation = false
                            foldersTree.forceActiveFocus()
                            mouse.accepted = true
                        }
                    }

                    Rectangle {
                        id: depthGuide
                        visible: folderDelegate.isTreeNode && folderDelegate.depth > 0 && !root.effectsReduced
                        x: folderDelegate.baseIndent + (folderDelegate.depth * folderDelegate.indentStep) - 8
                        y: 4
                        width: 1
                        height: parent.height - 8
                        color: Theme.panelStrokeSubtle
                        opacity: folderDelegate.isActive || folderDelegate.isCurrent
                                 ? 0.72
                                 : (rowMouse.containsMouse ? 0.58 : 0.42)
                    }

                    Item {
                        id: disclosureArea
                        z: 2
                        x: folderDelegate.baseIndent + (folderDelegate.isTreeNode ? folderDelegate.depth * folderDelegate.indentStep : 0)
                        y: 0
                        width: folderDelegate.indicatorSlot
                        height: parent.height
                        visible: folderDelegate.isTreeNode && folderDelegate.hasChildren
                        opacity: folderDelegate.isActive || folderDelegate.isCurrent
                                 ? 1
                                 : (rowMouse.containsMouse ? 0.96 : 0.78)

                        Canvas {
                            id: chevronCanvas
                            anchors.centerIn: parent
                            width: 12
                            height: 12
                            visible: !root.effectsReduced && !folderDelegate.loading
                            rotation: folderDelegate.expanded ? 90 : 0
                            opacity: folderDelegate.hasChildren ? 1 : 0.35
                            
                            Behavior on rotation {
                                enabled: !root.effectsReduced
                                NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutQuad }
                            }

                            Behavior on opacity {
                                enabled: !root.effectsReduced
                                NumberAnimation { duration: Theme.motionFast }
                            }
                            
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                ctx.strokeStyle = folderDelegate.isActive || folderDelegate.isCurrent || rowMouse.containsMouse
                                    ? Theme.textPrimary
                                    : Theme.textSecondary;
                                ctx.lineWidth = 1.25;
                                ctx.lineCap = "round";
                                ctx.lineJoin = "round";
                                ctx.beginPath();
                                ctx.moveTo(4, 2.5);
                                ctx.lineTo(7.5, 6);
                                ctx.lineTo(4, 9.5);
                                ctx.stroke();
                            }
                            
                            Connections {
                                target: folderDelegate
                                function onIsActiveChanged() {
                                    if (!root.effectsReduced) chevronCanvas.requestPaint();
                                }
                            }
                            Connections {
                                target: rowMouse
                                function onContainsMouseChanged() {
                                    if (!root.effectsReduced) chevronCanvas.requestPaint();
                                }
                            }
                            Connections {
                                target: themeController
                                function onThemeChanged() {
                                    if (!root.effectsReduced) chevronCanvas.requestPaint();
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: root.effectsReduced && !folderDelegate.loading
                            text: folderDelegate.expanded ? ">" : ">"
                            rotation: folderDelegate.expanded ? 90 : 0
                            color: folderDelegate.isActive || folderDelegate.isCurrent ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                            opacity: folderDelegate.hasChildren ? 0.85 : 0.35
                        }

                        BusyIndicator {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            running: folderDelegate.loading
                            visible: folderDelegate.loading
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: function(mouse) {
                                folderDelegate.treeView.toggleExpanded(folderDelegate.row)
                                root.trapTabNavigation = false
                                foldersTree.forceActiveFocus()
                                mouse.accepted = true
                            }
                        }
                    }

                    Item {
                        id: rowArea
                        z: 1
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: folderDelegate.baseIndent
                            + (folderDelegate.isTreeNode ? folderDelegate.depth * folderDelegate.indentStep : 0)
                            + folderDelegate.indicatorSlot + 8
                        anchors.rightMargin: 12

                        RowLayout {
                            anchors.fill: parent
                            spacing: 10

                            RecolorSvgIcon {
                                Layout.preferredWidth: folderDelegate.iconSize
                                Layout.preferredHeight: folderDelegate.iconSize
                                sourcePath: root.iconSourceFor(model.icon)
                                recolorColor: root.iconToneFor(model.icon, folderDelegate.isActive || folderDelegate.isCurrent, rowMouse.containsMouse)
                                cacheKey: "sidebar"
                                sourceSize: Qt.size(folderDelegate.iconSize * 2, folderDelegate.iconSize * 2)
                                asynchronous: true
                                cache: true
                                opacity: folderDelegate.isActive || folderDelegate.isCurrent || rowMouse.containsMouse ? 1 : 0.84
                            }

                            Label {
                                text: model.name || ""
                                Layout.fillWidth: true
                                font.pixelSize: 13
                                font.letterSpacing: 0.2
                                font.weight: isActive || folderDelegate.isCurrent || rowMouse.containsMouse ? Font.Medium : Font.Normal
                                color: Theme.textPrimary
                                opacity: isActive || folderDelegate.isCurrent || rowMouse.containsMouse ? 1.0 : 0.92
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                readonly property var panel: workspaceController.activePanel === 0
                    ? workspaceController.leftPanel
                    : workspaceController.rightPanel
            }

            ScrollBar.vertical: ScrollBar {
                id: foldersTreeVerticalScrollBar
                policy: ScrollBar.AsNeeded
            }
        }
    }

    DriveContextMenu {
        id: placeDriveContextMenu

        onOpenRequested: function(path) {
            root.openPathInActivePanel(path)
        }

        onAnalyzeRequested: function(path) {
            if (root.Window.window && root.Window.window.openDiskUsage) {
                root.Window.window.openDiskUsage(path)
            }
        }

        onEjectRequested: function(path, managedIsoMount) {
            if (managedIsoMount) {
                workspaceController.unmountIsoRoot(path)
            } else {
                const panel = root.activePanelController()
                if (panel) panel.ejectDrive(path)
            }
        }

        onPropertiesRequested: function(path) {
            propertiesController.load(path)
        }
    }

    Connections {
        target: workspaceController
        function onActivePanelChanged() {
            root.updatePlaceSelectionForActivePath()
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.treeModel
        function onPathRevealReady(requestId, index, exact) {
            if (requestId !== root.treeSyncRequestId) return
            if (!index || !index.valid) {
                root.clearTreeSelection()
                return
            }
            root.selectTreeIndex(index)
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onCurrentPathChanged() {
            root.updatePlaceSelectionForActivePath()
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onPathNavigated() {
            root.updatePlaceSelectionForActivePath()
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onCurrentPathChanged() {
            root.updatePlaceSelectionForActivePath()
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onPathNavigated() {
            root.updatePlaceSelectionForActivePath()
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.isoMountManager
        function onUnmountStarted(rootPath) {
            root.closePlaceDriveMenuForPath(rootPath)
        }

        function onUnmountFinished(rootPath, success, error) {
            root.closePlaceDriveMenuForPath(rootPath)
        }
    }

    Connections {
        target: workspaceController.placesModel
        function onModelReset() {
            root.clearPlaceSelection()
            placeDriveContextMenu.close()
            root.resetPlaceDriveMenu()
        }

        function onRowsRemoved(removedParent, first, last) {
            root.clearPlaceSelection()
            placeDriveContextMenu.close()
            root.resetPlaceDriveMenu()
        }
    }

    Component.onCompleted: syncTreeToActivePath()
}
