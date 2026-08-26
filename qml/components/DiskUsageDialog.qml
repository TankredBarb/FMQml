import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "common"
import "framework"
import "dialogs"
import "filepanel"

Dialog {
    id: root

    title: "Disk Usage"
    modal: true
    focus: true
    width: Math.min(parent ? parent.width - 48 : 860, 860)
    height: Math.min(parent ? parent.height - 48 : 620, 620)
    padding: 0

    property var appRoot: null
    property var backdropSource: null
    property real dragOriginX: 0
    property real dragOriginY: 0
    property int activeTab: 0
    property int viewMode: 2
    property int mapContentMode: 0
    property bool returnedFromPanel: false
    readonly property bool scanning: diskUsageController && diskUsageController.busy
    readonly property bool hasError: diskUsageController && diskUsageController.error.length > 0
    readonly property var activeModel: activeTab === 0
                                       ? diskUsageController.summaryModel
                                       : (activeTab === 1
                                          ? diskUsageController.rootChildrenModel
                                          : (activeTab === 2
                                             ? diskUsageController.largestFoldersModel
                                             : diskUsageController.largestFilesModel))
    readonly property var mapModel: mapContentMode === 0
                                    ? diskUsageController.largestFoldersModel
                                    : diskUsageController.largestFilesModel
    readonly property int skippedDetailCount: diskUsageController
                                             ? diskUsageController.skippedDetailEntries.length
                                             : 0
    readonly property string activeTabDescription: activeTab === 0
        ? "Top-level items plus largest individual files. Folder sizes include their contents; file rows point to concrete space consumers."
        : (activeTab === 1
           ? "Top-level items only. Folder sizes include their contents and add up cleanly within the scanned root."
           : (activeTab === 2
              ? "Largest folders anywhere under this scan. Parent and child folders can overlap."
              : "Largest individual files anywhere under this scan."))
    readonly property int rowActionButtonSize: 24
    readonly property int rowActionButtonSpacing: 6
    readonly property int rowActionColumnWidth: rowActionButtonSize * 5 + rowActionButtonSpacing * 4
    readonly property int rowSizeColumnWidth: 98
    readonly property int rowItemsColumnWidth: 92
    readonly property int breadcrumbButtonHeight: Math.max(28, Theme.fontSizeCaption + 16)
    readonly property int breadcrumbBarHeight: root.breadcrumbButtonHeight + 12
    readonly property int breadcrumbIconSize: Math.max(13, Math.min(20, Theme.fontSizeCaption + 2))

    signal resultOpened()
    signal resultsReset()

    onOpened: {
        root.centerInParent()
        Qt.callLater(() => contentItem.forceActiveFocus())
    }
    onWidthChanged: root.clampDialogPosition()
    onHeightChanged: root.clampDialogPosition()
    onClosed: {
        if (diskUsageController && diskUsageController.busy) {
            diskUsageController.cancel()
        }
    }

    function openFor(path) {
        root.returnedFromPanel = false
        root.open()
        diskUsageController.scan(path)
    }

    function reopenResults() {
        root.returnedFromPanel = true
        root.open()
        Qt.callLater(() => resultsView.forceActiveFocus())
    }

    function clearResults() {
        root.returnedFromPanel = false
        root.resultsReset()
        if (diskUsageController) {
            diskUsageController.clear()
        }
    }

    function centerInParent() {
        if (!parent) {
            return
        }
        root.setDialogPosition((parent.width - root.width) / 2,
                               (parent.height - root.height) / 2)
    }

    function clampDialogPosition() {
        if (!root.opened || !parent) {
            return
        }
        root.setDialogPosition(root.x, root.y)
    }

    function setDialogPosition(nextX, nextY) {
        if (!parent) {
            root.x = nextX
            root.y = nextY
            return
        }
        const margin = 8
        const maxX = Math.max(margin, parent.width - root.width - margin)
        const maxY = Math.max(margin, parent.height - root.height - margin)
        root.x = Math.max(margin, Math.min(maxX, nextX))
        root.y = Math.max(margin, Math.min(maxY, nextY))
    }

    function activePanelController() {
        return root.appRoot && root.appRoot.activePanelController
            ? root.appRoot.activePanelController()
            : null
    }

    function openInActivePanel(path, isDirectory) {
        const panel = activePanelController()
        if (!panel || !path || path.length === 0) {
            return
        }
        if (panel.openInPanelTarget(path, isDirectory)) {
            root.resultOpened()
            root.accept()
        }
    }

    function copyPath(path) {
        if (!workspaceController || !path || path.length === 0) {
            return
        }
        workspaceController.copyTextToClipboard(root.displayPath(path))
        if (root.appRoot && root.appRoot.showTransientInfo) {
            root.appRoot.showTransientInfo("Path copied to clipboard")
        }
    }

    function showProperties(path) {
        if (typeof propertiesController === "undefined" || !propertiesController || !path || path.length === 0) {
            return
        }
        propertiesController.load(path)
    }

    function revealPath(path) {
        if (!diskUsageController || !path || path.length === 0) {
            return
        }
        diskUsageController.revealPath(path)
    }

    function suffixForPath(path, isDirectory) {
        if (isDirectory || !path) {
            return ""
        }
        const name = String(path).split(/[\\/]/).pop()
        const dot = name.lastIndexOf(".")
        return dot > 0 && dot < name.length - 1 ? name.slice(dot + 1) : ""
    }

    function displayPath(path) {
        if (!path || String(path).length === 0) {
            return ""
        }
        if (typeof workspaceController !== "undefined" && workspaceController && workspaceController.displayPath) {
            return workspaceController.displayPath(String(path))
        }
        return String(path).replace(/\//g, "\\")
    }

    function setSort(key) {
        if (!root.activeModel) {
            return
        }
        const ascending = root.activeModel.sortKey === key ? !root.activeModel.sortAscending : (key === 1)
        root.activeModel.setSort(key, ascending)
    }

    function sortLabel(label, key) {
        if (!root.activeModel || root.activeModel.sortKey !== key) {
            return label
        }
        return label + (root.activeModel.sortAscending ? " ^" : " v")
    }

    component SortHeaderButton : FmButton {
        id: sortBtn

        property int sortKeyValue: 0
        property int align: Text.AlignLeft

        flat: true
        padding: 0
        implicitHeight: 26

        contentItem: Label {
            text: sortBtn.text
            color: root.activeModel && root.activeModel.sortKey === sortBtn.sortKeyValue
                   ? Theme.textPrimary
                   : Theme.textSecondary
            font.pixelSize: Theme.fontSizeCaption
            font.weight: Font.DemiBold
            horizontalAlignment: sortBtn.align
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        onClicked: root.setSort(sortKeyValue)
    }

    background: DialogShell {
        translucent: typeof appSettings !== "undefined" && appSettings
                     ? appSettings.workspaceDialogsTransparency
                     : false
        backdropSource: root.backdropSource
        backdropTransformItem: root
        accentColor: Theme.accent
        shellBorderColor: Theme.panelBorder
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/hard-drive.svg"
        iconTint: Theme.accent
        accentColor: Theme.accent
        title: root.title
        subtitle: diskUsageController ? diskUsageController.displayRootPath : ""
        closeText: "x"
        onCloseRequested: root.accept()

        DragHandler {
            target: null
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (active) {
                    root.dragOriginX = root.x
                    root.dragOriginY = root.y
                }
            }
            onTranslationChanged: {
                if (active) {
                    root.setDialogPosition(root.dragOriginX + translation.x,
                                           root.dragOriginY + translation.y)
                }
            }
        }
    }

    footer: DialogFooter {
        Item { Layout.fillWidth: true }

        FmButton {
            visible: root.scanning
            text: "Cancel"
            highlighted: false
            onClicked: diskUsageController.cancel()
        }

        FmButton {
            visible: root.returnedFromPanel
            text: "Reset Results"
            highlighted: false
            onClicked: root.clearResults()
            ToolTip.visible: hovered
            ToolTip.delay: 350
            ToolTip.text: "Clears this analysis and hides the toolbar Disk Usage button."
        }

        FmButton {
            text: "Rescan"
            enabled: diskUsageController && diskUsageController.rootPath.length > 0 && !root.scanning
            highlighted: false
            onClicked: diskUsageController.rescan()
        }

        FmButton {
            text: "Close"
            highlighted: true
            primaryColor: Theme.accent
            onClicked: root.accept()
        }
    }

    Popup {
        id: skippedDetailsPopup

        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(root.width - 48, 640)
        height: Math.min(root.height - 96, 360)
        x: (root.width - width) / 2
        y: 92
        padding: 0

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.menuSurface
            border.color: Theme.menuBorder
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                Layout.leftMargin: 12
                Layout.rightMargin: 8
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: "Skipped paths"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                }

                FmIconButton {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/close.svg"
                    iconSize: 16
                    svgRecolorColor: Theme.actionIconColor("close")
                    onClicked: skippedDetailsPopup.close()
                    ToolTip.visible: hovered
                    ToolTip.text: "Close"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.menuSeparator
            }

            ScrollView {
                id: skippedDetailsScrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical: FmScrollBar {
                    id: skippedDetailsScrollBar
                    parent: skippedDetailsScrollView.contentItem
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    policy: ScrollBar.AsNeeded
                }

                ListView {
                    id: skippedDetailsList
                    model: diskUsageController.skippedDetailEntries
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: ItemDelegate {
                        width: skippedDetailsScrollBar.scrollNeeded
                               ? Math.max(0, skippedDetailsScrollBar.x - 6)
                               : skippedDetailsList.width
                        height: 42

                        background: Rectangle {
                            color: parent.hovered ? Theme.menuItemHover : "transparent"
                        }

                        contentItem: RowLayout {
                            spacing: 10

                            InlineBadge {
                                text: modelData.label
                                textColor: modelData.kind === "inaccessible" ? Theme.warning : Theme.textSecondary
                                fillColor: Theme.withAlpha(modelData.kind === "inaccessible" ? Theme.warning : Theme.textSecondary, 0.10)
                                strokeColor: Theme.withAlpha(modelData.kind === "inaccessible" ? Theme.warning : Theme.textSecondary, 0.22)
                                horizontalPadding: 8
                                badgeHeight: 18
                                fontSize: 9
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.path
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeCaption
                                elide: Text.ElideMiddle
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }
        }
    }

    contentItem: ColumnLayout {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        spacing: 0
        clip: true
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                root.accept()
                event.accepted = true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(94, summaryColumn.implicitHeight + 28)
            color: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.54 : 0.72)
            border.width: 0

            ColumnLayout {
                id: summaryColumn

                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    InlineBadge {
                        text: root.scanning ? "SCANNING" : (root.hasError ? "FAILED" : (diskUsageController.cached ? "CACHED" : "READY"))
                        textColor: root.hasError ? Theme.danger : Theme.accent
                        fillColor: Theme.withAlpha(root.hasError ? Theme.danger : Theme.accent, 0.10)
                        strokeColor: Theme.withAlpha(root.hasError ? Theme.danger : Theme.accent, 0.24)
                        fontSize: 9
                        fontWeight: Font.Bold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.scanning
                              ? ("Reading " + diskUsageController.currentDisplayPath)
                              : (root.hasError
                                 ? diskUsageController.error
                                 : (diskUsageController.cached && diskUsageController.cacheStatusText.length > 0
                                    ? diskUsageController.cacheStatusText
                                    : "Largest folders and files"))
                        color: root.hasError ? Theme.danger : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeLabel
                        elide: Text.ElideMiddle
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 18

                    Label {
                        text: "Seen " + diskUsageController.totalBytesText
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                    }

                    Label {
                        visible: diskUsageController.storageUsedText.length > 0
                        text: "Used " + diskUsageController.storageUsedText
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeLabel
                    }

                    Label {
                        visible: diskUsageController.storageTotalText.length > 0
                        text: "Total " + diskUsageController.storageTotalText
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeLabel
                    }

                    Label {
                        text: diskUsageController.scannedFiles + " files"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeLabel
                    }

                    Label {
                        text: diskUsageController.scannedFolders + " folders"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeLabel
                    }

                    FmButton {
                        visible: diskUsageController.coverageStatusText.length > 0
                        enabled: root.skippedDetailCount > 0
                        flat: true
                        primaryColor: diskUsageController.inaccessiblePaths > 0 ? Theme.warning : Theme.accent
                        width: Math.min(implicitWidth, Math.max(160, root.width * 0.34))
                        padding: 0
                        text: diskUsageController.coverageStatusText
                        contentItem: Label {
                            text: parent.text
                            color: diskUsageController.inaccessiblePaths > 0 ? Theme.warning : Theme.textSecondary
                            font.pixelSize: Theme.fontSizeLabel
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: skippedDetailsPopup.open()
                        ToolTip.visible: enabled && hovered
                        ToolTip.text: "Show skipped paths"
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: diskUsageController.lastError.length > 0
                    text: diskUsageController.lastError
                    color: Theme.warning
                    font.pixelSize: Theme.fontSizeCaption
                    elide: Text.ElideMiddle
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.panelBorder
                opacity: 0.45
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 10
            Layout.bottomMargin: 10
            spacing: 10

            FmTabBar {
                Layout.preferredWidth: 290
                implicitHeight: 40
                currentIndex: root.viewMode === 2 ? 0 : (root.viewMode === 0 ? 1 : 2)
                model: [
                    { text: "List", value: 2 },
                    { text: "Map", value: 0 },
                    { text: "Chart", value: 1 }
                ]
                onActivated: (index, value) => root.viewMode = value
            }

            FmTabBar {
                visible: root.viewMode === 0
                Layout.preferredWidth: 270
                implicitHeight: 40
                currentIndex: root.mapContentMode
                model: [
                    { text: "Largest folders", value: 0 },
                    { text: "Largest files", value: 1 }
                ]
                onActivated: (index, value) => root.mapContentMode = value
            }

            Label {
                Layout.fillWidth: true
                text: root.viewMode === 0
                      ? (treemap.hiddenCount > 0
                         ? (treemap.displayedCount + " of " + root.mapModel.count
                            + " · Softer = smaller · Rest in List")
                         : ("All " + treemap.displayedCount + " · Softer = smaller"))
                      : (root.viewMode === 1
                         ? "Three hierarchy levels · Click a folder to inspect it"
                         : "Sortable detailed results and file actions")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
            }
        }

        FmTabBar {
            id: tabContainer
            visible: root.viewMode === 2
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 8
            implicitHeight: 40
            currentIndex: root.activeTab
            model: [
                { text: "Summary (" + diskUsageController.summaryModel.count + ")", value: 0 },
                { text: "Direct children (" + diskUsageController.rootChildrenModel.count + ")", value: 1 },
                { text: "Largest folders (" + diskUsageController.largestFoldersModel.count + ")", value: 2 },
                { text: "Largest files (" + diskUsageController.largestFilesModel.count + ")", value: 3 }
            ]
            onActivated: (index, value) => root.activeTab = value
        }

        Rectangle {
            visible: root.viewMode === 2
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 8
            implicitHeight: 28
            radius: Theme.radiusSm
            color: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.50 : 0.70)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.62)
            border.width: 1

            Label {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                text: root.activeTabDescription
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.breadcrumbBarHeight
            color: Theme.panelSurface
            border.width: 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 6

                FmIconButton {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: root.breadcrumbButtonHeight
                    enabled: diskUsageController.canGoBack && !root.scanning
                    iconSource: "../assets/icons-classic/arrow-left.svg"
                    iconTone: "back"
                    iconSize: 14
                    onClicked: diskUsageController.navigateBack()
                    ToolTip.visible: hovered
                    ToolTip.text: "Back"
                }

                FmIconButton {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: root.breadcrumbButtonHeight
                    enabled: diskUsageController.canGoUp && !root.scanning
                    iconSource: "../assets/icons-classic/arrow-up.svg"
                    iconTone: "up"
                    iconSize: 14
                    onClicked: diskUsageController.navigateUp()
                    ToolTip.visible: hovered
                    ToolTip.text: "Up"
                }

                Flickable {
                    id: breadcrumbFlickable
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.breadcrumbButtonHeight
                    contentWidth: breadcrumbRow.implicitWidth
                    contentHeight: height
                    interactive: contentWidth > width
                    flickableDirection: Flickable.HorizontalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    function revealCurrentFolder() {
                        contentX = Math.max(0, contentWidth - width)
                    }

                    onContentWidthChanged: Qt.callLater(revealCurrentFolder)
                    onWidthChanged: Qt.callLater(revealCurrentFolder)
                    Component.onCompleted: Qt.callLater(revealCurrentFolder)

                    Connections {
                        target: diskUsageController
                        function onRootPathChanged() {
                            Qt.callLater(breadcrumbFlickable.revealCurrentFolder)
                        }
                    }

                    RowLayout {
                        id: breadcrumbRow
                        height: parent.height
                        spacing: 4

                        Repeater {
                            model: diskUsageController.breadcrumbEntries

                            RowLayout {
                                spacing: 4

                                Label {
                                    visible: index > 0
                                    text: ">"
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeLabel
                                    Layout.preferredHeight: root.breadcrumbButtonHeight
                                    verticalAlignment: Text.AlignVCenter
                                }

                                FmButton {
                                    id: breadcrumbButton
                                    readonly property bool isCurrent: index === diskUsageController.breadcrumbEntries.length - 1

                                    text: modelData.label
                                    enabled: !root.scanning && modelData.path !== diskUsageController.rootPath
                                    flat: true
                                    padding: 0
                                    topPadding: 0
                                    bottomPadding: 0
                                    Layout.preferredHeight: root.breadcrumbButtonHeight
                                    Layout.preferredWidth: Math.min(isCurrent ? 180 : 116,
                                                                    Math.max(72, implicitWidth))
                                    Layout.maximumWidth: isCurrent ? 180 : 116
                                    contentItem: RowLayout {
                                        spacing: 5
                                        clip: true

                                        RecolorSvgIcon {
                                            Layout.preferredWidth: root.breadcrumbIconSize
                                            Layout.preferredHeight: root.breadcrumbIconSize
                                            sourcePath: modelData.isDrive ? "../assets/icons-classic/hard-drive.svg" : "../assets/icons-classic/folder.svg"
                                            sourceSize: Qt.size(root.breadcrumbIconSize * 2, root.breadcrumbIconSize * 2)
                                            recolorEnabled: true
                                            recolorColor: Theme.chromeIconColor(modelData.isDrive ? "drive" : "folder")
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: breadcrumbButton.text
                                            color: breadcrumbButton.enabled ? Theme.textPrimary : Theme.textSecondary
                                            font.pixelSize: Theme.fontSizeCaption
                                            elide: Text.ElideMiddle
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            lineHeightMode: Text.FixedHeight
                                            lineHeight: Math.ceil(Theme.fontSizeCaption * 1.25)
                                        }
                                    }
                                    onClicked: diskUsageController.navigateTo(modelData.path)
                                    ToolTip.visible: hovered
                                    ToolTip.text: modelData.path
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.panelBorder
                opacity: 0.35
            }
        }

        Rectangle {
            visible: root.viewMode === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 10
            radius: Theme.radiusMd
            color: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.42 : 0.64)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.72)
            border.width: 1
            clip: true

            DiskUsageTreemapItem {
                id: treemap
                anchors.fill: parent
                anchors.margins: 6
                model: root.mapModel
                folderColor: Theme.categoryNavigation
                fileColor: Theme.categoryInfo
                surfaceColor: Theme.panelSurfaceSoft
                tilePalette: [
                    Theme.categoryNavigation,
                    Theme.categoryInfo,
                    Theme.categoryAction,
                    Theme.categoryUtility,
                    Theme.categorySystem,
                    Theme.secondaryAccent,
                    Theme.warmAccent,
                    Theme.success
                ]
                borderColor: Theme.withAlpha(Theme.panelBorder, 0.82)
                textColor: Theme.textPrimary
                secondaryTextColor: Theme.textSecondary
                fontPixelSize: Theme.fontSizeLabel
            }

            MouseArea {
                id: treemapMouse
                anchors.fill: treemap
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                property var entry: ({})
                property var pendingEntry: ({})

                Timer {
                    id: treemapSingleClickTimer
                    interval: Application.styleHints.mouseDoubleClickInterval
                    repeat: false
                    onTriggered: {
                        if (treemapMouse.pendingEntry.isDirectory && !root.scanning)
                            diskUsageController.navigateTo(treemapMouse.pendingEntry.path)
                        treemapMouse.pendingEntry = ({})
                    }
                }

                function updateEntry(mouse) {
                    entry = treemap.entryAt(mouse.x, mouse.y)
                    treemap.hoveredIndex = entry.index === undefined ? -1 : entry.index
                }

                onPositionChanged: (mouse) => updateEntry(mouse)
                onExited: {
                    entry = ({})
                    treemap.hoveredIndex = -1
                }
                onClicked: (mouse) => {
                    updateEntry(mouse)
                    if (entry.isDirectory && !root.scanning) {
                        pendingEntry = entry
                        treemapSingleClickTimer.restart()
                    }
                }
                onDoubleClicked: (mouse) => {
                    treemapSingleClickTimer.stop()
                    pendingEntry = ({})
                    updateEntry(mouse)
                    if (entry.path && !root.scanning)
                        root.openInActivePanel(entry.path, entry.isDirectory)
                }

                ToolTip {
                    id: treemapToolTip
                    parent: treemapMouse
                    visible: treemapMouse.containsMouse && treemapMouse.entry.path !== undefined
                    delay: 350
                    padding: 9
                    width: Math.min(440, Math.max(220, root.width - 32))
                    x: Math.max(0, Math.min(treemapMouse.width - width,
                                           treemapMouse.entry.rectX
                                           + treemapMouse.entry.rectWidth / 2 - width / 2))
                    y: treemapMouse.entry.rectY - height - 6

                    contentItem: ColumnLayout {
                        spacing: 3

                        Label {
                            Layout.fillWidth: true
                            text: treemapMouse.entry.name === undefined ? "" : treemapMouse.entry.name
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeCaption
                            font.weight: Font.DemiBold
                            elide: Text.ElideMiddle
                        }

                        Label {
                            Layout.fillWidth: true
                            text: treemapMouse.entry.path === undefined
                                  ? "" : root.displayPath(treemapMouse.entry.path)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideMiddle
                        }

                        Label {
                            Layout.fillWidth: true
                            text: treemapMouse.entry.sizeText === undefined ? "" :
                                  treemapMouse.entry.sizeText + " · " + treemapMouse.entry.percentText
                                  + (treemapMouse.entry.isDirectory
                                     ? " · " + treemapMouse.entry.fileCount + " files · "
                                       + treemapMouse.entry.folderCount + " folders"
                                     : "")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Column {
                anchors.centerIn: treemap
                spacing: 6
                visible: root.mapModel.count === 0
                width: Math.max(0, Math.min(parent.width - 40, 420))

                Label {
                    width: parent.width
                    text: root.scanning ? "Building size map…"
                         : (root.hasError ? diskUsageController.error : "Nothing to visualize here")
                    color: root.hasError ? Theme.danger : Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    visible: root.scanning
                    width: parent.width
                    text: diskUsageController.currentDisplayPath
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                }
            }

        }

        Rectangle {
            visible: root.viewMode === 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 10
            radius: Theme.radiusMd
            color: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.42 : 0.64)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.72)
            border.width: 1
            clip: true

            DiskUsageSunburstItem {
                id: sunburst
                visible: diskUsageController.sunburstRoot.name !== undefined
                anchors.fill: parent
                anchors.margins: 6
                rootNode: diskUsageController.sunburstRoot
                surfaceColor: Theme.panelSurfaceSoft
                borderColor: Theme.withAlpha(Theme.panelBorder, 0.82)
                textColor: Theme.textPrimary
                secondaryTextColor: Theme.textSecondary
                ringPalette: [
                    Theme.categoryNavigation,
                    Theme.categoryInfo,
                    Theme.categoryAction,
                    Theme.categoryUtility,
                    Theme.categorySystem,
                    Theme.secondaryAccent,
                    Theme.warmAccent,
                    Theme.success
                ]
                fontPixelSize: Theme.fontSizeLabel
            }

            MouseArea {
                id: sunburstMouse
                visible: sunburst.visible
                anchors.fill: sunburst
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                property var entry: ({})
                property var pendingEntry: ({})

                Timer {
                    id: sunburstSingleClickTimer
                    interval: Application.styleHints.mouseDoubleClickInterval
                    repeat: false
                    onTriggered: {
                        if (sunburstMouse.pendingEntry.isDirectory
                                && !sunburstMouse.pendingEntry.aggregate
                                && !root.scanning) {
                            diskUsageController.navigateTo(sunburstMouse.pendingEntry.path)
                        }
                        sunburstMouse.pendingEntry = ({})
                    }
                }

                function updateEntry(mouse) {
                    entry = sunburst.entryAt(mouse.x, mouse.y)
                    sunburst.hoveredIndex = entry.index === undefined ? -1 : entry.index
                }

                onPositionChanged: (mouse) => updateEntry(mouse)
                onExited: {
                    entry = ({})
                    sunburst.hoveredIndex = -1
                }
                onClicked: (mouse) => {
                    updateEntry(mouse)
                    if (entry.isDirectory && !entry.aggregate && entry.path && !root.scanning) {
                        pendingEntry = entry
                        sunburstSingleClickTimer.restart()
                    }
                }
                onDoubleClicked: (mouse) => {
                    sunburstSingleClickTimer.stop()
                    pendingEntry = ({})
                    updateEntry(mouse)
                    if (entry.isDirectory && !entry.aggregate && entry.path && !root.scanning)
                        root.openInActivePanel(entry.path, true)
                }

                ToolTip {
                    parent: sunburstMouse
                    visible: sunburstMouse.containsMouse && sunburstMouse.entry.name !== undefined
                    delay: 350
                    padding: 9
                    width: Math.min(440, Math.max(220, root.width - 32))
                    x: Math.max(0, Math.min(sunburstMouse.width - width,
                                           sunburstMouse.mouseX - width / 2))
                    y: Math.max(0, sunburstMouse.mouseY - height - 10)

                    contentItem: ColumnLayout {
                        spacing: 3

                        Label {
                            Layout.fillWidth: true
                            text: sunburstMouse.entry.name === undefined ? "" : sunburstMouse.entry.name
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeCaption
                            font.weight: Font.DemiBold
                            elide: Text.ElideMiddle
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: sunburstMouse.entry.path === undefined
                                  ? "" : root.displayPath(sunburstMouse.entry.path)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideMiddle
                        }

                        Label {
                            Layout.fillWidth: true
                            text: sunburstMouse.entry.sizeText === undefined
                                  ? "" : sunburstMouse.entry.sizeText
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Rectangle {
                visible: root.scanning && sunburst.visible
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 12
                width: sunburstScanningLabel.implicitWidth + 20
                height: 28
                radius: 14
                color: Theme.withAlpha(Theme.panelSurface, 0.90)
                border.color: Theme.withAlpha(Theme.panelBorder, 0.82)
                border.width: 1

                Label {
                    id: sunburstScanningLabel
                    anchors.centerIn: parent
                    text: "Scanning · sizes updating"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeMicro
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 6
                visible: diskUsageController.sunburstRoot.name === undefined
                width: Math.max(0, Math.min(parent.width - 40, 420))

                Label {
                    width: parent.width
                    text: root.scanning ? "Building hierarchy…"
                         : (root.hasError ? diskUsageController.error : "Nothing to visualize here")
                    color: root.hasError ? Theme.danger : Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    visible: root.scanning
                    width: parent.width
                    text: diskUsageController.currentDisplayPath
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                }
            }
        }

        Rectangle {
            visible: root.viewMode === 2
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: Theme.panelSurfaceSoft

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                SortHeaderButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.sortLabel("Name", 1)
                    sortKeyValue: 1
                    align: Text.AlignLeft
                }
                Item {
                    Layout.preferredWidth: root.rowActionColumnWidth
                    Layout.minimumWidth: root.rowActionColumnWidth
                    Layout.maximumWidth: root.rowActionColumnWidth
                }
                SortHeaderButton {
                    Layout.preferredWidth: root.rowSizeColumnWidth
                    Layout.minimumWidth: root.rowSizeColumnWidth
                    Layout.maximumWidth: root.rowSizeColumnWidth
                    text: root.sortLabel("Size", 0)
                    sortKeyValue: 0
                    align: Text.AlignRight
                }
                SortHeaderButton {
                    Layout.preferredWidth: root.rowItemsColumnWidth
                    Layout.minimumWidth: root.rowItemsColumnWidth
                    Layout.maximumWidth: root.rowItemsColumnWidth
                    text: root.activeTab === 3 ? root.sortLabel("% of seen", 0) : root.sortLabel("Items", 2)
                    sortKeyValue: root.activeTab === 3 ? 0 : 2
                    align: Text.AlignRight
                }
            }
        }

        ListView {
            id: resultsView
            visible: root.viewMode === 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.activeModel
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: FmScrollBar {
                id: resultsScrollBar
            }

            delegate: ItemDelegate {
                id: row
                width: resultsScrollBar.scrollNeeded
                       ? Math.max(0, resultsScrollBar.x - 6)
                       : resultsView.width
                height: 58
                ToolTip.visible: hovered
                ToolTip.delay: 650
                ToolTip.text: root.displayPath(model.path)

                onDoubleClicked: {
                    if (model.isDirectory) {
                        diskUsageController.navigateTo(model.path)
                    } else {
                        root.openInActivePanel(model.path, false)
                    }
                }

                background: Rectangle {
                    color: row.pressed ? Theme.surfaceActive : (row.hovered ? Theme.itemHoverFill : "transparent")
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.panelBorder
                        opacity: 0.35
                    }
                }

                contentItem: RowLayout {
                    spacing: 12

                    FileIconCell {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        path: model.path
                        suffix: root.suffixForPath(model.path, model.isDirectory)
                        isDirectory: model.isDirectory
                        useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
                        iconSize: 20
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: model.name
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeBody
                                font.weight: Font.Medium
                                elide: Text.ElideMiddle
                                maximumLineCount: 1
                            }

                            InlineBadge {
                                text: model.isDirectory ? "folder" : "file"
                                textColor: model.isDirectory ? Theme.categoryNavigation : Theme.categoryInfo
                                fillColor: Theme.withAlpha(model.isDirectory ? Theme.categoryNavigation : Theme.categoryInfo, 0.10)
                                strokeColor: Theme.withAlpha(model.isDirectory ? Theme.categoryNavigation : Theme.categoryInfo, 0.22)
                                horizontalPadding: 8
                                badgeHeight: 18
                                fontSize: 9
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: root.displayPath(model.path)
                            color: Theme.textSecondary
                            opacity: 0.88
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideMiddle
                            maximumLineCount: 1
                        }

                        FmProgressBar {
                            Layout.fillWidth: true
                            value: model.percentOfRoot
                            implicitHeight: 3
                            trackHeight: 3
                            fillColor: Theme.accent
                            trackColor: Theme.withAlpha(Theme.panelBorder, 0.48)
                            trackBorderColor: "transparent"
                            animationDuration: 120
                        }
                    }

                    RowLayout {
                        Layout.preferredWidth: root.rowActionColumnWidth
                        Layout.minimumWidth: root.rowActionColumnWidth
                        Layout.maximumWidth: root.rowActionColumnWidth
                        Layout.alignment: Qt.AlignTop
                        spacing: root.rowActionButtonSpacing

                        FmIconButton {
                            Layout.preferredWidth: root.rowActionButtonSize
                            Layout.minimumWidth: root.rowActionButtonSize
                            Layout.maximumWidth: root.rowActionButtonSize
                            Layout.preferredHeight: root.rowActionButtonSize
                            Layout.minimumHeight: root.rowActionButtonSize
                            Layout.maximumHeight: root.rowActionButtonSize
                            iconSource: "../assets/icons-classic/search.svg"
                            iconTone: "info"
                            iconSize: 13
                            onClicked: {
                                if (model.isDirectory) {
                                    diskUsageController.navigateTo(model.path)
                                }
                            }
                            ToolTip.visible: enabled && hovered
                            ToolTip.text: "Analyze folder"
                        }

                        FmIconButton {
                            Layout.preferredWidth: root.rowActionButtonSize
                            Layout.minimumWidth: root.rowActionButtonSize
                            Layout.maximumWidth: root.rowActionButtonSize
                            Layout.preferredHeight: root.rowActionButtonSize
                            Layout.minimumHeight: root.rowActionButtonSize
                            Layout.maximumHeight: root.rowActionButtonSize
                            iconSource: "../assets/icons-classic/folder-open.svg"
                            iconTone: "open"
                            iconSize: 13
                            onClicked: {
                                root.openInActivePanel(model.path, model.isDirectory)
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: model.isDirectory ? "Open folder in panel" : "Open containing folder in panel"
                        }

                        FmIconButton {
                            visible: model.isDirectory
                            enabled: visible
                            Layout.preferredWidth: root.rowActionButtonSize
                            Layout.minimumWidth: root.rowActionButtonSize
                            Layout.maximumWidth: root.rowActionButtonSize
                            Layout.preferredHeight: root.rowActionButtonSize
                            Layout.minimumHeight: root.rowActionButtonSize
                            Layout.maximumHeight: root.rowActionButtonSize
                            iconSource: "../assets/icons-classic/copy.svg"
                            iconTone: "copy"
                            iconSize: 13
                            onClicked: root.copyPath(model.path)
                            ToolTip.visible: hovered
                            ToolTip.text: "Copy path"
                        }

                        FmIconButton {
                            Layout.preferredWidth: root.rowActionButtonSize
                            Layout.minimumWidth: root.rowActionButtonSize
                            Layout.maximumWidth: root.rowActionButtonSize
                            Layout.preferredHeight: root.rowActionButtonSize
                            Layout.minimumHeight: root.rowActionButtonSize
                            Layout.maximumHeight: root.rowActionButtonSize
                            iconSource: "../assets/icons-classic/reveal.svg"
                            iconTone: "forward"
                            iconSize: 13
                            onClicked: root.revealPath(model.path)
                            ToolTip.visible: enabled && hovered
                            ToolTip.text: Qt.platform.os === "windows" ? "Show in Explorer" : "Reveal in file manager"
                        }

                        FmIconButton {
                            Layout.preferredWidth: root.rowActionButtonSize
                            Layout.minimumWidth: root.rowActionButtonSize
                            Layout.maximumWidth: root.rowActionButtonSize
                            Layout.preferredHeight: root.rowActionButtonSize
                            Layout.minimumHeight: root.rowActionButtonSize
                            Layout.maximumHeight: root.rowActionButtonSize
                            iconSource: "../assets/icons-classic/info.svg"
                            iconTone: "info"
                            iconSize: 13
                            onClicked: root.showProperties(model.path)
                            ToolTip.visible: hovered
                            ToolTip.text: "Properties"
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: root.rowSizeColumnWidth
                        Layout.minimumWidth: root.rowSizeColumnWidth
                        Layout.maximumWidth: root.rowSizeColumnWidth
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: model.sizeDetailText
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignRight
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.activeTab !== 3 && model.percentOfRootText.length > 0
                            text: model.percentOfRootText
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeMicro
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    Label {
                        Layout.preferredWidth: root.rowItemsColumnWidth
                        Layout.minimumWidth: root.rowItemsColumnWidth
                        Layout.maximumWidth: root.rowItemsColumnWidth
                        text: model.isDirectory
                              ? (model.fileCount + "/" + model.folderCount)
                              : (root.activeTab === 3 ? model.percentOfRootText : "file")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeCaption
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: resultsView.count === 0 && !root.scanning
                text: root.hasError ? diskUsageController.error : "No results yet"
                color: root.hasError ? Theme.danger : Theme.textSecondary
                font.pixelSize: Theme.fontSizeBody
            }
        }
    }
}
