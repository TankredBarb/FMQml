import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../framework"
import "../../style"

Popup {
    id: root

    required property var controller
    property var backdropSource: null
    property int viewMode: 0
    required property var panel
    readonly property bool peekOpen: controller && controller.open
    readonly property bool translucentSurface: appSettings ? appSettings.folderPeekTransparency : false
    readonly property real transparencyStrength: appSettings ? appSettings.commandPaletteTransparencyStrength / 100.0 : 0.6
    readonly property real surfaceAlpha: themeController.isDark ? 1.0 - transparencyStrength * 0.32 : 1.0 - transparencyStrength * 0.26
    readonly property bool blurSurface: translucentSurface && appSettings && appSettings.surfaceBlur && backdropSource
    readonly property var currentBreadcrumbs: controller ? controller.breadcrumbs : []
    readonly property string currentFolderName: currentBreadcrumbs.length > 0
                                                 ? String(currentBreadcrumbs[currentBreadcrumbs.length - 1].name)
                                                 : String(controller ? controller.currentPath : "")

    function quickLookMoveToPath(path) {
        return contentRoot.quickLookMoveToPath(path)
    }

    function benchmarkSnapshot() {
        const view = root.viewMode === 0 ? gridView : listView
        let instantiated = 0
        let visibleDelegates = 0
        let eligibleThumbnails = 0
        let scheduledThumbnails = 0
        let readyThumbnails = 0
        let visiblePathsMatch = true
        if (view && view.itemAtIndex) {
            for (let row = 0; row < view.count; ++row) {
                const item = view.itemAtIndex(row)
                if (!item) continue
                ++instantiated
                const point = item.mapToItem(view, 0, 0)
                const visible = point.x + item.width > 0 && point.y + item.height > 0
                                && point.x < view.width && point.y < view.height
                if (!visible) continue
                ++visibleDelegates
                const itemPath = String(item.entryPath || "")
                const currentPath = String(root.controller.currentPath || "")
                if (itemPath.length === 0
                        || (itemPath !== currentPath
                            && itemPath.indexOf(currentPath + "/") !== 0)) {
                    visiblePathsMatch = false
                }
                if (item.benchmarkThumbnailEligible) {
                    ++eligibleThumbnails
                    if (item.benchmarkThumbnailScheduled) ++scheduledThumbnails
                    if (item.benchmarkThumbnailReady) ++readyThumbnails
                }
            }
        }
        return {
            "viewMode": root.viewMode,
            "open": root.controller.open,
            "state": root.controller.state,
            "currentPath": root.controller.currentPath,
            "entryCount": root.controller.entries.length,
            "instantiatedDelegateCount": instantiated,
            "visibleDelegateCount": visibleDelegates,
            "eligibleThumbnailCount": eligibleThumbnails,
            "scheduledThumbnailCount": scheduledThumbnails,
            "readyThumbnailCount": readyThumbnails,
            "visiblePathsMatch": visiblePathsMatch
        }
    }

    function breadcrumbIcon(pathKind, isDrive, isArchive, iconName) {
        if (iconName) return "qrc:/qt/qml/FM/qml/assets/filetypes-next/" + iconName + ".svg"
        if (isDrive) return "qrc:/qt/qml/FM/qml/assets/icons-classic/hard-drive.svg"
        if (isArchive) return "qrc:/qt/qml/FM/qml/assets/icons-classic/archive.svg"
        if (pathKind === "ftp") return "qrc:/qt/qml/FM/qml/assets/icons-classic/ftp.svg"
        if (pathKind === "gdrive") return "qrc:/qt/qml/FM/qml/assets/filetypes-next/gdrive.svg"
        if (pathKind === "mega") return "qrc:/qt/qml/FM/qml/assets/filetypes-next/mega.svg"
        if (pathKind === "telegram") return "qrc:/qt/qml/FM/qml/assets/filetypes-next/telegram.svg"
        if (pathKind === "instagram") return "qrc:/qt/qml/FM/qml/assets/filetypes-next/instagram.svg"
        if (pathKind === "remote") return "qrc:/qt/qml/FM/qml/assets/icons-classic/computer.svg"
        return "qrc:/qt/qml/FM/qml/assets/icons-classic/folder.svg"
    }

    function breadcrumbIconColor(pathKind, isDrive, isArchive, isCurrent, isHovered) {
        let role = "folder"
        if (pathKind === "ftp" || pathKind === "gdrive" || pathKind === "mega"
                || pathKind === "instagram" || pathKind === "telegram" || pathKind === "remote") {
            role = "navigation"
        } else if (isArchive) {
            role = "archive"
        } else if (isDrive) {
            role = "drive"
        }
        const base = Theme.actionIconColor(role)
        if (isCurrent) return Qt.lighter(base, themeController.isDark ? 1.2 : 1.1)
        if (isHovered) return Qt.lighter(base, themeController.isDark ? 1.1 : 1.05)
        return base
    }

    signal viewModeRequested(int mode)

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    padding: 0
    modal: true
    dim: false
    focus: true
    closePolicy: Popup.NoAutoClose
    background: Item {}

    onOpened: {
        Qt.callLater(function() {
            contentRoot.focusActiveView()
        })
    }
    onClosed: {
        if (root.controller && root.controller.open) root.controller.close()
    }

    Connections {
        target: root.controller
        function onOpenChanged() {
            if (root.controller.open) root.open()
            else root.close()
        }
    }

    Component.onCompleted: {
        if (root.peekOpen) root.open()
    }

    contentItem: Item {
        id: contentRoot

        implicitWidth: root.width
        implicitHeight: root.height
        property int selectedIndex: -1
        property var selectedPaths: ({})

        function focusActiveView() {
            const view = root.viewMode === 0 ? gridView : listView
            if (selectedIndex >= 0 && selectedIndex < view.count) {
                view.currentIndex = selectedIndex
            }
            view.forceActiveFocus()
        }

        function pathAt(index) {
            if (index < 0 || index >= root.controller.entries.length) return ""
            return String(root.controller.entries[index].path || "")
        }

        function isSelected(path) {
            return selectedPaths[String(path || "")] === true
        }

        function firstSelectedIndex() {
            for (let i = 0; i < root.controller.entries.length; ++i) {
                if (isSelected(pathAt(i))) return i
            }
            return -1
        }

        function setRangeSelection(origin, target) {
            const start = Math.min(origin, target)
            const end = Math.max(origin, target)
            let next = Object.assign({}, selectedPaths)
            let rangeAlreadySelected = true
            for (let i = start; i <= end; ++i) {
                if (!next[pathAt(i)]) {
                    rangeAlreadySelected = false
                    break
                }
            }
            if (!rangeAlreadySelected) {
                for (let i = start; i <= end; ++i) next[pathAt(i)] = true
            } else {
                let selectedStart = start
                while (selectedStart > 0 && next[pathAt(selectedStart - 1)]) --selectedStart
                let selectedEnd = end
                while (selectedEnd + 1 < root.controller.entries.length
                       && next[pathAt(selectedEnd + 1)]) ++selectedEnd
                for (let i = selectedStart; i <= selectedEnd; ++i) {
                    if (i >= start && i <= end) next[pathAt(i)] = true
                    else delete next[pathAt(i)]
                }
            }
            selectedPaths = next
        }

        function selectEntry(view, index, modifiers) {
            const path = pathAt(index)
            if (!path) return
            if (modifiers & Qt.ShiftModifier) {
                let origin = firstSelectedIndex()
                if (origin < 0) origin = view.currentIndex >= 0 ? view.currentIndex : 0
                setRangeSelection(origin, index)
            } else if (modifiers & Qt.ControlModifier) {
                let next = Object.assign({}, selectedPaths)
                if (next[path]) delete next[path]
                else next[path] = true
                selectedPaths = next
            } else {
                let next = ({})
                next[path] = true
                selectedPaths = next
            }
            selectedIndex = index
            view.currentIndex = index
            view.forceActiveFocus()
        }

        function selectedPathList() {
            let paths = []
            for (let i = 0; i < root.controller.entries.length; ++i) {
                const path = pathAt(i)
                if (isSelected(path)) paths.push(path)
            }
            if (paths.length === 0) {
                const view = root.viewMode === 0 ? gridView : listView
                const currentPath = pathAt(view.currentIndex)
                if (currentPath) paths.push(currentPath)
            }
            return paths
        }

        function copySelection() {
            const paths = selectedPathList()
            if (paths.length === 0 || !root.panel || !root.panel.workspaceController) return
            root.panel.workspaceController.copyPathsToClipboard(paths, root.panel.panelSide)
        }

        onVisibleChanged: {
            if (visible) {
                Qt.callLater(function() { contentRoot.focusActiveView() })
            }
        }

        Connections {
            target: root.controller
            function onCurrentPathChanged() {
                contentRoot.selectedIndex = -1
                contentRoot.selectedPaths = ({})
                Qt.callLater(function() { breadcrumbView.revealCurrentFolder() })
            }
            function onEntriesChanged() {
                if (root.controller.entries.length <= 0 || contentRoot.selectedIndex >= 0) return
                gridView.currentIndex = 0
                listView.currentIndex = 0
                if (root.peekOpen) Qt.callLater(function() { contentRoot.focusActiveView() })
            }
        }

        Connections {
            target: root
            function onViewModeChanged() {
                if (root.peekOpen) Qt.callLater(function() { contentRoot.focusActiveView() })
            }
        }

    function activateCurrentEntry() {
        const view = root.viewMode === 0 ? gridView : listView
        if (view.currentIndex < 0 || view.currentIndex >= root.controller.entries.length) return
        const entry = root.controller.entries[view.currentIndex]
        if (entry.isDirectory) root.controller.navigate(entry.path)
    }

    function quickLookCurrentEntry() {
        const view = root.viewMode === 0 ? gridView : listView
        if (view.currentIndex < 0 || view.currentIndex >= root.controller.entries.length) return
        const entry = root.controller.entries[view.currentIndex]
        const path = String(entry.path || "")
        if (!path || !root.panel || !root.panel.quickLookPopup) return
        root.panel.quickLookPopup.openFromPeek(root, path)
    }

    function quickLookMoveToPath(path) {
        let index = -1
        for (let row = 0; row < root.controller.entries.length; ++row) {
            if (String(root.controller.entries[row].path || "") === String(path || "")) {
                index = row
                break
            }
        }
        if (index < 0) return false
        const view = root.viewMode === 0 ? gridView : listView
        let next = ({})
        next[String(path)] = true
        contentRoot.selectedPaths = next
        contentRoot.selectedIndex = index
        view.currentIndex = index
        view.positionViewAtIndex(index, root.viewMode === 0 ? GridView.Contain : ListView.Contain)
        return true
    }

    function resumeGridThumbnails() {
        for (let i = 0; i < gridView.count; ++i) {
            const item = gridView.itemAtIndex(i)
            if (item && item.resumeThumbnail) item.resumeThumbnail()
        }
    }

    function resumeListThumbnails() {
        for (let i = 0; i < listView.count; ++i) {
            const item = listView.itemAtIndex(i)
            if (item && item.resumeThumbnail) item.resumeThumbnail()
        }
    }

    readonly property bool gridScrolling: gridView.visible
                                                   && (gridView.moving || gridScrollSettleTimer.running)
    readonly property bool listScrolling: listView.visible
                                                   && (listView.moving || listScrollSettleTimer.running)
    readonly property bool thumbnailTraceEnabled: Qt.application.arguments.indexOf("--folder-preview-thumbnail-trace") >= 0
    onGridScrollingChanged: {
        if (thumbnailTraceEnabled) {
            console.log("[FolderPreviewThumb] surface=peek-grid event=scroll-change active=" + gridScrolling
                        + " moving=" + gridView.moving + " contentY=" + Math.round(gridView.contentY))
        }
        if (!gridScrolling) {
            Qt.callLater(function() { contentRoot.resumeGridThumbnails() })
        }
    }
    onListScrollingChanged: {
        if (thumbnailTraceEnabled) {
            console.log("[FolderPreviewThumb] surface=peek-list event=scroll-change active=" + listScrolling
                        + " moving=" + listView.moving + " contentY=" + Math.round(listView.contentY))
        }
        if (!listScrolling) {
            Qt.callLater(function() { contentRoot.resumeListThumbnails() })
        }
    }

    Timer {
        id: gridScrollSettleTimer
        interval: 140
    }

    Timer {
        id: listScrollSettleTimer
        interval: 140
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.withAlpha(Theme.overlayScrim, themeController.isDark ? 0.10 : 0.06)
    }

    TranslucentSurface {
        id: surface
        width: Math.min(548, Math.max(360, root.width - 32))
        height: Math.min(526, Math.max(360, root.height - 48))
        anchors.centerIn: parent
        translucent: false
        active: root.visible
        backgroundBlurEnabled: false
        cornerRadius: 10
        baseColor: Theme.panelSurface
        startColor: Theme.chromeGradientStart
        midColor: Theme.chromeGradientMid
        endColor: Theme.panelSurface
        gradientStrength: 0.32
        borderColor: Theme.panelStroke
        borderWidth: 1
        shadowEnabled: false
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 5
                FmIconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-left.svg"
                    enabled: root.controller.canGoBack
                    Accessible.name: "Back in Folder Peek"
                    onClicked: root.controller.goBack()
                }
                FmIconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-up.svg"
                    Accessible.name: "Up in Folder Peek"
                    onClicked: root.controller.goUp()
                }
                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.currentFolderName
                    color: Theme.textPrimary
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                }
                FmProgressRing {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    visible: root.controller.loading && root.controller.state !== "loading"
                    running: visible
                }
                FmIconButton {
                    iconSource: root.viewMode === 0
                                ? "qrc:/qt/qml/FM/qml/assets/icons-classic/list.svg"
                                : "qrc:/qt/qml/FM/qml/assets/icons-classic/layout-grid.svg"
                    Accessible.name: root.viewMode === 0 ? "Use list view" : "Use grid view"
                    onClicked: root.viewModeRequested(root.viewMode === 0 ? 1 : 0)
                }
                FmButton { text: "Open in panel"; highlighted: true; onClicked: root.controller.openInSourcePanel() }
                FmIconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/close.svg"
                    Accessible.name: "Close Folder Peek"
                    onClicked: root.controller.close()
                }
            }

            TranslucentSurface {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                translucent: root.translucentSurface
                active: root.visible
                backgroundBlurEnabled: root.blurSurface
                backdropSource: root.backdropSource
                backdropTransformItem: root
                cornerRadius: 7
                baseColor: root.translucentSurface ? Theme.withAlpha(Theme.panelSurface, root.surfaceAlpha) : Theme.panelSurface
                startColor: root.translucentSurface ? Theme.withAlpha(Theme.chromeGradientStart, root.surfaceAlpha) : Theme.chromeGradientStart
                midColor: root.translucentSurface ? Theme.withAlpha(Theme.chromeGradientMid, root.surfaceAlpha) : Theme.chromeGradientMid
                endColor: root.translucentSurface
                          ? Theme.withAlpha(Theme.panelSurface, root.surfaceAlpha)
                          : Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.88 : 0.82)
                gradientStrength: 0.32
                borderColor: Theme.panelStroke
                borderWidth: 1
                shadowEnabled: false

                GridView {
                    id: gridView
                    anchors.fill: parent
                    anchors.margins: 8
                    anchors.rightMargin: 8 + (peekGridScrollBar.scrollNeeded ? peekGridScrollBar.width + 6 : 0)
                    visible: root.controller.state === "ready" && root.viewMode === 0
                    model: root.controller.entries
                    currentIndex: count > 0 ? 0 : -1
                    cellWidth: Math.max(96, Math.floor(width / Math.max(1, Math.floor(width / 118))))
                    cellHeight: 92
                    clip: true
                    onContentYChanged: gridScrollSettleTimer.restart()
                    onMovingChanged: gridScrollSettleTimer.restart()
                    ScrollBar.vertical: FmScrollBar {
                        id: peekGridScrollBar
                        parent: gridView.parent
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.topMargin: 8
                        anchors.rightMargin: 8
                        anchors.bottomMargin: 8
                        visible: gridView.visible
                        wheelTarget: gridView
                    }
                    delegate: Item {
                        id: peekGridDelegate
                        required property var modelData
                        required property int index
                        property bool pooled: false
                        property bool currentItem: GridView.isCurrentItem
                        readonly property string entryPath: String(modelData.path || "")
                        readonly property bool benchmarkThumbnailEligible: peekGridIcon.benchmarkThumbnailEligible
                        readonly property bool benchmarkThumbnailScheduled: peekGridIcon.benchmarkThumbnailScheduled
                        readonly property bool benchmarkThumbnailReady: peekGridIcon.benchmarkThumbnailReady
                        function resumeThumbnail() { peekGridIcon.resumeAfterScroll() }
                        width: gridView.cellWidth
                        height: gridView.cellHeight
                        GridView.onPooled: pooled = true
                        GridView.onReused: pooled = false
                        Rectangle {
                            anchors.fill: parent; anchors.margins: 3; radius: 6
                            color: "transparent"
                            FileItemStateLayer {
                                selected: contentRoot.isSelected(modelData.path)
                                panelActive: true
                                currentItem: peekGridDelegate.currentItem
                                hovered: entryHover.hovered
                                scrolling: contentRoot.gridScrolling
                                leftMargin: 1
                                rightMargin: 1
                                topMargin: 1
                                bottomMargin: 1
                            }
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 7; spacing: 3
                                FolderPreviewIcon {
                                    id: peekGridIcon
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 42
                                    Layout.preferredHeight: 42
                                    panel: root.panel
                                    surface: "peek-grid"
                                    active: root.peekOpen && root.controller.state === "ready"
                                            && !peekGridDelegate.pooled
                                    schedulingPaused: contentRoot.gridScrolling
                                    loadingPaused: contentRoot.gridScrolling
                                    entryIndex: index
                                    iconSize: 42
                                    path: modelData.path
                                    name: modelData.name
                                    iconName: modelData.iconName
                                    suffix: modelData.suffix || ""
                                    mimeType: modelData.mimeType || ""
                                    primaryBadgeKind: modelData.primaryBadgeKind || ""
                                    isDirectory: modelData.isDirectory
                                    hasThumbnail: modelData.hasThumbnail === true
                                }
                                Label { Layout.fillWidth: true; text: modelData.name; color: modelData.isDirectory ? TextColors.folderNameText : TextColors.fileNameText; font.pixelSize: Theme.fontSizeCaption; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                            }
                            HoverHandler { id: entryHover }
                            TapHandler {
                                id: entryTap
                                acceptedButtons: Qt.LeftButton
                                onTapped: contentRoot.selectEntry(gridView, index, entryTap.point.modifiers)
                                onDoubleTapped: root.controller.openEntry(modelData.path,
                                                                          modelData.isDirectory === true)
                            }
                        }
                    }
                }

                ListView {
                    id: listView
                    anchors.fill: parent
                    anchors.margins: 8
                    anchors.rightMargin: 8 + (peekListScrollBar.scrollNeeded ? peekListScrollBar.width + 6 : 0)
                    visible: root.controller.state === "ready" && root.viewMode === 1
                    model: root.controller.entries
                    currentIndex: count > 0 ? 0 : -1
                    spacing: 4
                    clip: true
                    onContentYChanged: listScrollSettleTimer.restart()
                    onMovingChanged: listScrollSettleTimer.restart()
                    ScrollBar.vertical: FmScrollBar {
                        id: peekListScrollBar
                        parent: listView.parent
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.topMargin: 8
                        anchors.rightMargin: 8
                        anchors.bottomMargin: 8
                        visible: listView.visible
                        wheelTarget: listView
                    }
                    delegate: Rectangle {
                        id: peekListDelegate
                        required property var modelData
                        required property int index
                        property bool pooled: false
                        property bool currentItem: ListView.isCurrentItem
                        readonly property string entryPath: String(modelData.path || "")
                        readonly property bool benchmarkThumbnailEligible: peekListIcon.benchmarkThumbnailEligible
                        readonly property bool benchmarkThumbnailScheduled: peekListIcon.benchmarkThumbnailScheduled
                        readonly property bool benchmarkThumbnailReady: peekListIcon.benchmarkThumbnailReady
                        function resumeThumbnail() { peekListIcon.resumeAfterScroll() }
                        width: listView.width
                        height: 42
                        ListView.onPooled: pooled = true
                        ListView.onReused: pooled = false
                        radius: 5
                        color: "transparent"
                        FileItemStateLayer {
                            selected: contentRoot.isSelected(modelData.path)
                            panelActive: true
                            currentItem: peekListDelegate.currentItem
                            hovered: rowHover.hovered
                            scrolling: contentRoot.listScrolling
                        }
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 8
                            FolderPreviewIcon {
                                id: peekListIcon
                                Layout.preferredWidth: 26
                                Layout.preferredHeight: 26
                                panel: root.panel
                                surface: "peek-list"
                                active: root.peekOpen && root.controller.state === "ready"
                                        && !peekListDelegate.pooled
                                schedulingPaused: contentRoot.listScrolling
                                loadingPaused: contentRoot.listScrolling
                                entryIndex: index
                                iconSize: 26
                                path: modelData.path
                                name: modelData.name
                                iconName: modelData.iconName
                                suffix: modelData.suffix || ""
                                mimeType: modelData.mimeType || ""
                                primaryBadgeKind: modelData.primaryBadgeKind || ""
                                isDirectory: modelData.isDirectory
                                hasThumbnail: modelData.hasThumbnail === true
                            }
                            Label { Layout.fillWidth: true; text: modelData.name; color: modelData.isDirectory ? TextColors.folderNameText : TextColors.fileNameText; elide: Text.ElideRight }
                            Label { text: modelData.isDirectory ? "Folder" : "File"; color: TextColors.fileSecondaryText; font.pixelSize: Theme.fontSizeMicro }
                        }
                        HoverHandler { id: rowHover }
                        TapHandler {
                            id: rowTap
                            acceptedButtons: Qt.LeftButton
                            onTapped: contentRoot.selectEntry(listView, index, rowTap.point.modifiers)
                            onDoubleTapped: root.controller.openEntry(modelData.path,
                                                                      modelData.isDirectory === true)
                        }
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 48, 320)
                    spacing: 10
                    visible: root.controller.state === "empty"

                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 112
                        Layout.preferredHeight: 112
                        Rectangle {
                            anchors.centerIn: parent
                            width: 104; height: 104; radius: 52
                            color: Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.13 : 0.09)
                            border.color: Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.28 : 0.20)
                            border.width: 1
                            Rectangle {
                                anchors.centerIn: parent
                                width: 78; height: 78; radius: 39
                                color: Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.08 : 0.06)
                            }
                        }
                        RecolorSvgIcon {
                            anchors.centerIn: parent
                            width: 58; height: 58
                            sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg"
                            recolorColor: Theme.activeAccent
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            width: 34; height: 34; radius: 17
                            color: Theme.panelSurfaceStrong
                            border.color: Theme.panelStroke
                            RecolorSvgIcon {
                                anchors.centerIn: parent
                                width: 19; height: 19
                                sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/file-plus.svg"
                                recolorColor: Theme.activeAccent
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Nothing here yet"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "A clean space, ready for your files."
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeLabel
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 32, 280)
                    spacing: 9
                    visible: root.controller.state !== "ready" && root.controller.state !== "empty"
                    FmProgressRing { Layout.alignment: Qt.AlignHCenter; visible: root.controller.state === "loading"; running: visible }
                    RecolorSvgIcon {
                        Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 48; Layout.preferredHeight: 48
                        visible: root.controller.state !== "loading"
                        sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/info.svg"
                        recolorColor: root.controller.state === "error" ? Theme.warning : Theme.textSecondary
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.controller.state === "loading" ? "Loading folder…"
                              : (root.controller.state === "unavailable" ? "Folder could not be loaded" : "Folder is not available")
                        color: Theme.textSecondary; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 3
                Label {
                    visible: root.controller.hasMore
                    text: "Showing first 1000 items"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeMicro
                }
                ListView {
                    id: breadcrumbView
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: 34
                    orientation: ListView.Horizontal
                    spacing: 3
                    clip: true
                    interactive: contentWidth > width
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.controller.breadcrumbs

                    function revealCurrentFolder() {
                        if (count > 0) positionViewAtIndex(count - 1, ListView.End)
                    }

                    onCountChanged: Qt.callLater(revealCurrentFolder)
                    Component.onCompleted: Qt.callLater(revealCurrentFolder)

                    delegate: FmButton {
                        id: breadcrumbButton
                        required property var modelData
                        required property int index
                        readonly property string name: String(modelData.name || "")
                        readonly property string pathKind: String(modelData.pathKind || "")
                        readonly property bool isDrive: Boolean(modelData.isDrive)
                        readonly property bool isArchive: Boolean(modelData.isArchive)
                        readonly property string iconName: String(modelData.iconName || "")
                        readonly property bool iconRecolorAllowed: modelData.iconRecolorAllowed === undefined
                                                                   ? true : Boolean(modelData.iconRecolorAllowed)
                        readonly property bool isCurrent: index === breadcrumbView.count - 1
                        width: Math.min(116, Math.max(72, implicitContentWidth + 12))
                        height: 34
                        leftPadding: 6
                        rightPadding: 6
                        flat: true
                        highlighted: isCurrent
                        Accessible.name: "Open " + name + " in Folder Peek"

                        contentItem: RowLayout {
                            spacing: 4
                            clip: true

                            RecolorSvgIcon {
                                sourcePath: root.breadcrumbIcon(breadcrumbButton.pathKind,
                                                                breadcrumbButton.isDrive,
                                                                breadcrumbButton.isArchive,
                                                                breadcrumbButton.iconName)
                                recolorColor: root.breadcrumbIconColor(breadcrumbButton.pathKind,
                                                                       breadcrumbButton.isDrive,
                                                                       breadcrumbButton.isArchive,
                                                                       breadcrumbButton.isCurrent,
                                                                       breadcrumbButton.hovered)
                                recolorEnabled: breadcrumbButton.iconRecolorAllowed
                                Layout.preferredWidth: 14
                                Layout.preferredHeight: 14
                                sourceSize: Qt.size(28, 28)
                            }

                            Text {
                                Layout.fillWidth: true
                                text: breadcrumbButton.name
                                color: breadcrumbButton.isCurrent ? Theme.accent : Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                font.bold: breadcrumbButton.isCurrent
                                elide: Text.ElideMiddle
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        ToolTip.visible: hovered
                        ToolTip.text: name
                        ToolTip.delay: 350
                        onClicked: if (modelData.path !== root.controller.currentPath) root.controller.navigate(modelData.path)
                    }
                }
            }
        }
    }

    Shortcut { sequence: "Escape"; enabled: root.peekOpen; onActivated: root.controller.close() }
    Shortcut { sequence: "Backspace"; enabled: root.peekOpen && root.controller.canGoBack; onActivated: root.controller.goBack() }
    Shortcut { sequence: "Return"; enabled: root.peekOpen; onActivated: contentRoot.activateCurrentEntry() }
    Shortcut { sequence: "Enter"; enabled: root.peekOpen; onActivated: contentRoot.activateCurrentEntry() }
    Shortcut { sequence: "Space"; enabled: root.peekOpen; onActivated: contentRoot.quickLookCurrentEntry() }
    Shortcut { sequence: "Ctrl+C"; enabled: root.peekOpen; onActivated: contentRoot.copySelection() }
    Shortcut { sequence: "Alt+Left"; enabled: root.peekOpen && root.controller.canGoBack; onActivated: root.controller.goBack() }
    Shortcut { sequence: "Alt+Up"; enabled: root.peekOpen; onActivated: root.controller.goUp() }
    }
}
