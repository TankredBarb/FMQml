import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"
import "dialogs"
import "filepanel"

Dialog {
    id: root

    title: "Disk Usage"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 48 : 860, 860)
    height: Math.min(parent ? parent.height - 48 : 620, 620)
    padding: 0

    property var appRoot: null
    property int activeTab: 0
    readonly property bool scanning: diskUsageController && diskUsageController.busy
    readonly property bool hasError: diskUsageController && diskUsageController.error.length > 0
    readonly property var activeModel: activeTab === 0
                                       ? diskUsageController.summaryModel
                                       : (activeTab === 1
                                          ? diskUsageController.rootChildrenModel
                                          : (activeTab === 2
                                             ? diskUsageController.largestFoldersModel
                                             : diskUsageController.largestFilesModel))
    readonly property var activeTabButton: activeTab === 0
                                           ? tabBtnSummary
                                           : (activeTab === 1
                                              ? tabBtnBreakdown
                                              : (activeTab === 2 ? tabBtnFolders : tabBtnFiles))
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

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())
    onClosed: {
        if (diskUsageController && diskUsageController.busy) {
            diskUsageController.cancel()
        }
    }

    function openFor(path) {
        root.open()
        diskUsageController.scan(path)
    }

    function activePanelController() {
        return root.appRoot && root.appRoot.activePanelController
            ? root.appRoot.activePanelController()
            : null
    }

    function openInActivePanel(path) {
        const panel = activePanelController()
        if (!panel || !path || path.length === 0) {
            return
        }
        if (panel.openPath(path)) {
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

    component DiskUsageTabButton : Button {
        id: tabBtn

        property bool active: false

        Layout.fillWidth: true
        implicitHeight: 30
        leftPadding: 10
        rightPadding: 10
        topPadding: 0
        bottomPadding: 0

        background: Rectangle {
            radius: Theme.radiusSm
            color: !tabBtn.active && tabBtn.hovered
                   ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.05 : 0.035)
                   : "transparent"
            border.color: "transparent"
            border.width: 1
        }

        contentItem: Label {
            text: tabBtn.text
            color: tabBtn.active ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: 11
            font.weight: tabBtn.active ? Font.DemiBold : Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }
    }

    component SortHeaderButton : Button {
        id: sortBtn

        property int sortKeyValue: 0
        property int align: Text.AlignLeft

        flat: true
        padding: 0
        implicitHeight: 26

        background: Rectangle {
            radius: Theme.radiusSm
            color: sortBtn.hovered ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.05 : 0.035) : "transparent"
        }

        contentItem: Label {
            text: sortBtn.text
            color: root.activeModel && root.activeModel.sortKey === sortBtn.sortKeyValue
                   ? Theme.textPrimary
                   : Theme.textSecondary
            font.pixelSize: 11
            font.weight: Font.DemiBold
            horizontalAlignment: sortBtn.align
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        onClicked: root.setSort(sortKeyValue)
    }

    background: DialogShell {
        accentColor: Theme.accent
        shellBorderColor: Theme.panelBorder
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/hard-drive.svg"
        iconTint: Theme.accent
        accentColor: Theme.accent
        title: root.title
        subtitle: diskUsageController ? diskUsageController.displayRootPath : ""
        closeText: "x"
        onCloseRequested: root.accept()
    }

    footer: DialogFooter {
        Item { Layout.fillWidth: true }

        DialogActionButton {
            visible: root.scanning
            text: "Cancel"
            highlighted: false
            onClicked: diskUsageController.cancel()
        }

        DialogActionButton {
            text: "Rescan"
            enabled: diskUsageController && diskUsageController.rootPath.length > 0 && !root.scanning
            highlighted: false
            onClicked: diskUsageController.rescan()
        }

        DialogActionButton {
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
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Button {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    text: "x"
                    flat: true
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: Theme.radiusSm
                        color: parent.hovered ? Theme.menuItemHover : "transparent"
                    }
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
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    model: diskUsageController.skippedDetailEntries
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: ItemDelegate {
                        width: ListView.view.width
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
                                font.pixelSize: 11
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
            Layout.preferredHeight: 94
            color: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.54 : 0.72)
            border.width: 0

            ColumnLayout {
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
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 18

                    Label {
                        text: "Seen " + diskUsageController.totalBytesText
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    Label {
                        visible: diskUsageController.storageUsedText.length > 0
                        text: "Used " + diskUsageController.storageUsedText
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }

                    Label {
                        visible: diskUsageController.storageTotalText.length > 0
                        text: "Total " + diskUsageController.storageTotalText
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }

                    Label {
                        text: diskUsageController.scannedFiles + " files"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }

                    Label {
                        text: diskUsageController.scannedFolders + " folders"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }

                    Button {
                        visible: diskUsageController.coverageStatusText.length > 0
                        enabled: root.skippedDetailCount > 0
                        flat: true
                        padding: 0
                        text: diskUsageController.coverageStatusText
                        contentItem: Label {
                            text: parent.text
                            color: diskUsageController.inaccessiblePaths > 0 ? Theme.warning : Theme.textSecondary
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: parent.enabled && parent.hovered ? Theme.panelSurfaceSoft : "transparent"
                        }
                        onClicked: skippedDetailsPopup.open()
                        ToolTip.visible: enabled && hovered
                        ToolTip.text: "Show skipped paths"
                    }

                    Item { Layout.fillWidth: true }
                }

                Label {
                    Layout.fillWidth: true
                    visible: diskUsageController.lastError.length > 0
                    text: diskUsageController.lastError
                    color: Theme.warning
                    font.pixelSize: 11
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

        Rectangle {
            id: tabContainer
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 10
            Layout.bottomMargin: 10
            implicitHeight: 40
            radius: Theme.radiusSm
            color: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.92 : 0.98)
            border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.90 : 0.78)
            border.width: 1

            Rectangle {
                x: root.activeTabButton ? root.activeTabButton.x + tabRow.x : 0
                y: root.activeTabButton ? root.activeTabButton.y + tabRow.y : 0
                width: root.activeTabButton ? root.activeTabButton.width : 0
                height: root.activeTabButton ? root.activeTabButton.height : 0
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.16 : 0.10)
                border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.34 : 0.22)
                border.width: 1

                Behavior on x {
                    NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                }
                Behavior on width {
                    NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                }
            }

            RowLayout {
                id: tabRow
                anchors.fill: parent
                anchors.margins: 4
                spacing: 4

                DiskUsageTabButton {
                    id: tabBtnSummary
                    active: root.activeTab === 0
                    onClicked: root.activeTab = 0
                    text: "Summary (" + diskUsageController.summaryModel.count + ")"
                }
                DiskUsageTabButton {
                    id: tabBtnBreakdown
                    active: root.activeTab === 1
                    onClicked: root.activeTab = 1
                    text: "Direct children (" + diskUsageController.rootChildrenModel.count + ")"
                }
                DiskUsageTabButton {
                    id: tabBtnFolders
                    active: root.activeTab === 2
                    onClicked: root.activeTab = 2
                    text: "Largest folders (" + diskUsageController.largestFoldersModel.count + ")"
                }
                DiskUsageTabButton {
                    id: tabBtnFiles
                    active: root.activeTab === 3
                    onClicked: root.activeTab = 3
                    text: "Largest files (" + diskUsageController.largestFilesModel.count + ")"
                }
            }
        }

        Rectangle {
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
                font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.panelSurface
            border.width: 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 6

                IconButton {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 26
                    enabled: diskUsageController.canGoBack && !root.scanning
                    iconSource: "../assets/lucide-toolbar/arrow-left.svg"
                    iconTone: "back"
                    iconSize: 14
                    onClicked: diskUsageController.navigateBack()
                    ToolTip.visible: hovered
                    ToolTip.text: "Back"
                }

                IconButton {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 26
                    enabled: diskUsageController.canGoUp && !root.scanning
                    iconSource: "../assets/lucide-toolbar/arrow-up.svg"
                    iconTone: "up"
                    iconSize: 14
                    onClicked: diskUsageController.navigateUp()
                    ToolTip.visible: hovered
                    ToolTip.text: "Up"
                }

                Flickable {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    contentWidth: breadcrumbRow.implicitWidth
                    contentHeight: height
                    interactive: contentWidth > width
                    clip: true

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
                                    font.pixelSize: 12
                                    verticalAlignment: Text.AlignVCenter
                                }

                                Button {
                                    id: breadcrumbButton

                                    text: modelData.label
                                    enabled: !root.scanning && modelData.path !== diskUsageController.rootPath
                                    flat: true
                                    Layout.preferredHeight: 26
                                    contentItem: RowLayout {
                                        spacing: 5

                                        RecolorSvgIcon {
                                            Layout.preferredWidth: 13
                                            Layout.preferredHeight: 13
                                            sourcePath: modelData.isDrive ? "../assets/icons/hard-drive.svg" : "../assets/icons/folder.svg"
                                            sourceSize: Qt.size(26, 26)
                                            recolorEnabled: true
                                            recolorColor: Theme.actionIconColor(modelData.isDrive ? "drive" : "folder")
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: breadcrumbButton.text
                                            color: breadcrumbButton.enabled ? Theme.textPrimary : Theme.textSecondary
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                    background: Rectangle {
                                        radius: Theme.radiusSm
                                        color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.panelSurfaceSoft : "transparent")
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
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.activeModel
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                width: ListView.view.width
                height: 58
                ToolTip.visible: hovered
                ToolTip.delay: 650
                ToolTip.text: root.displayPath(model.path)

                onDoubleClicked: {
                    if (model.isDirectory) {
                        diskUsageController.navigateTo(model.path)
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
                                font.pixelSize: 13
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
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                            maximumLineCount: 1
                        }

                        LinearProgress {
                            Layout.fillWidth: true
                            value: model.percentOfRoot
                            barHeight: 3
                            fillColor: Theme.accent
                            trackColor: Theme.withAlpha(Theme.panelBorder, 0.48)
                            animationDuration: 120
                        }
                    }

                    RowLayout {
                        Layout.preferredWidth: root.rowActionColumnWidth
                        Layout.minimumWidth: root.rowActionColumnWidth
                        Layout.maximumWidth: root.rowActionColumnWidth
                        Layout.alignment: Qt.AlignTop
                        spacing: root.rowActionButtonSpacing

                        IconButton {
                            enabled: model.isDirectory
                            opacity: model.isDirectory ? 1 : 0
                            Layout.preferredWidth: root.rowActionButtonSize
                            Layout.minimumWidth: root.rowActionButtonSize
                            Layout.maximumWidth: root.rowActionButtonSize
                            Layout.preferredHeight: root.rowActionButtonSize
                            Layout.minimumHeight: root.rowActionButtonSize
                            Layout.maximumHeight: root.rowActionButtonSize
                            iconSource: "../assets/lucide-toolbar/search.svg"
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

                        IconButton {
                            enabled: model.isDirectory
                            opacity: model.isDirectory ? 1 : 0
                            Layout.preferredWidth: root.rowActionButtonSize
                            Layout.minimumWidth: root.rowActionButtonSize
                            Layout.maximumWidth: root.rowActionButtonSize
                            Layout.preferredHeight: root.rowActionButtonSize
                            Layout.minimumHeight: root.rowActionButtonSize
                            Layout.maximumHeight: root.rowActionButtonSize
                            iconSource: "../assets/icons/open.svg"
                            iconTone: "open"
                            iconSize: 13
                            onClicked: {
                                if (model.isDirectory) {
                                    root.openInActivePanel(model.path)
                                }
                            }
                            ToolTip.visible: enabled && hovered
                            ToolTip.text: "Open in panel"
                        }

                        IconButton {
                            Layout.preferredWidth: root.rowActionButtonSize
                            Layout.minimumWidth: root.rowActionButtonSize
                            Layout.maximumWidth: root.rowActionButtonSize
                            Layout.preferredHeight: root.rowActionButtonSize
                            Layout.minimumHeight: root.rowActionButtonSize
                            Layout.maximumHeight: root.rowActionButtonSize
                            iconSource: "../assets/icons/clipboard-copy.svg"
                            iconTone: "copy"
                            iconSize: 13
                            onClicked: root.copyPath(model.path)
                            ToolTip.visible: hovered
                            ToolTip.text: "Copy path"
                        }

                        IconButton {
                            Layout.preferredWidth: root.rowActionButtonSize
                            Layout.minimumWidth: root.rowActionButtonSize
                            Layout.maximumWidth: root.rowActionButtonSize
                            Layout.preferredHeight: root.rowActionButtonSize
                            Layout.minimumHeight: root.rowActionButtonSize
                            Layout.maximumHeight: root.rowActionButtonSize
                            iconSource: "../assets/icons/reveal.svg"
                            iconTone: "forward"
                            iconSize: 13
                            onClicked: root.revealPath(model.path)
                            ToolTip.visible: hovered
                            ToolTip.text: Qt.platform.os === "windows" ? "Show in Explorer" : "Reveal in file manager"
                        }

                        IconButton {
                            Layout.preferredWidth: root.rowActionButtonSize
                            Layout.minimumWidth: root.rowActionButtonSize
                            Layout.maximumWidth: root.rowActionButtonSize
                            Layout.preferredHeight: root.rowActionButtonSize
                            Layout.minimumHeight: root.rowActionButtonSize
                            Layout.maximumHeight: root.rowActionButtonSize
                            iconSource: "../assets/lucide-toolbar/info.svg"
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
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignRight
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.activeTab !== 3 && model.percentOfRootText.length > 0
                            text: model.percentOfRootText
                            color: Theme.textSecondary
                            font.pixelSize: 10
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
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: resultsView.count === 0 && !root.scanning
                text: root.hasError ? diskUsageController.error : "No results yet"
                color: root.hasError ? Theme.danger : Theme.textSecondary
                font.pixelSize: 13
            }
        }
    }
}
