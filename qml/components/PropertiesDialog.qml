import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"
import "dialogs"
import "filepanel"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: root.driveMode || root.hasAccessOwnershipTab ? 560 : 420
    padding: 0
    height: Math.min(mainLayout.implicitHeight, parent ? parent.height * 0.95 : 640)
    visible: propertiesController.visible && !root.suppressDialog

    property bool suppressDialog: false
    property bool exportDialogPending: false
    property bool accessOwnershipAdminEditMode: false
    property int pendingUnixMode: 0
    property bool unixModeDirty: false
    property string pendingUnixOwner: ""
    property string pendingUnixGroup: ""
    property bool unixOwnershipDirty: false
    property int confirmedUnixMode: 0
    property var appRoot: null

    function unixModeEnabled(bit) {
        return (pendingUnixMode & bit) !== 0
    }

    function setUnixModeBit(bit, enabled) {
        pendingUnixMode = enabled ? (pendingUnixMode | bit) : (pendingUnixMode & ~bit)
        unixModeDirty = pendingUnixMode !== propertiesController.unixMode
    }

    function unixModeOctal(mode) {
        return (mode & 4095).toString(8).padStart(3, "0")
    }

    function resetUnixMode() {
        pendingUnixMode = propertiesController.unixMode
        unixModeDirty = false
    }

    property bool applyUnixModeRecursively: false

    function applyUnixMode() {
        const newlyEnabledPrivilegeBits = (pendingUnixMode & 3072) & ~(propertiesController.unixMode & 3072)
        const isRegularExecutable = !propertiesController.isDirectory && (propertiesController.unixMode & 73) !== 0
        if (isRegularExecutable && newlyEnabledPrivilegeBits !== 0) {
            confirmedUnixMode = pendingUnixMode
            specialModeConfirmation.open()
            return
        }
        applyConfirmedUnixMode(pendingUnixMode)
    }

    function applyConfirmedUnixMode(mode) {
        if (propertiesController.setUnixMode(mode, accessOwnershipAdminEditMode, applyUnixModeRecursively)) {
            unixModeDirty = false
        }
    }

    function resetUnixOwnership() {
        pendingUnixOwner = propertiesController.unixOwnerName
        pendingUnixGroup = propertiesController.unixGroupName
        unixOwnershipDirty = false
    }

    function applyUnixOwnership() {
        const owner = pendingUnixOwner === propertiesController.unixOwnerName ? "" : pendingUnixOwner
        const group = pendingUnixGroup === propertiesController.unixGroupName ? "" : pendingUnixGroup
        if (propertiesController.setUnixOwnership(owner, group, accessOwnershipAdminEditMode)) {
            unixOwnershipDirty = false
        }
    }

    function copyAll() {
        if (typeof workspaceController !== "undefined" && workspaceController) {
            workspaceController.copyTextToClipboard(propertiesController.exportableText())
            copyAllTooltip.show(copyAllTooltip.text)
        }
    }

    function copyJson() {
        if (typeof workspaceController !== "undefined" && workspaceController) {
            workspaceController.copyTextToClipboard(propertiesController.exportableJson())
            copyJsonTooltip.show(copyJsonTooltip.text)
        }
    }

    function openExportMenu() {
        exportMenu.popup(exportButton, 0, exportButton.height)
    }

    function silentExport(type) {
        root.suppressDialog = true
        root.exportDialogPending = true
        root.exportType = (type && type.toLowerCase() === "txt") ? "txt" : "json"
        fileDialog.selectedFile = "file:///" + propertiesController.name.replace(/\\/g, "/") + "_properties." + root.exportType
        fileDialog.open()
    }

    function activePanelController() {
        return root.appRoot && root.appRoot.activePanelController ? root.appRoot.activePanelController() : null
    }

    function actionPathsText() {
        if (root.multiMode) {
            return Array.from(propertiesController.selectedPaths).map(path => root.displayPath(path)).join("\n")
        }
        return root.displayPath(propertiesController.path)
    }

    function displayPath(path) {
        if (!path || String(path).length === 0) {
            return ""
        }
        if (typeof workspaceController !== "undefined" && workspaceController && workspaceController.displayPath) {
            return workspaceController.displayPath(String(path))
        }
        const value = String(path)
        if (value.indexOf("archive://") === 0 || value.indexOf("devices://") === 0 || value.indexOf("favorites://") === 0) {
            return value
        }
        return Qt.platform.os === "windows" ? value.replace(/\//g, "\\") : value
    }

    function copyActionPaths() {
        if (typeof workspaceController !== "undefined" && workspaceController) {
            workspaceController.copyTextToClipboard(root.actionPathsText())
            actionTooltip.show(root.multiMode ? "Selected paths copied" : "Path copied")
        }
    }

    function revealActionPath() {
        if (root.multiMode || propertiesController.path.length === 0) {
            return
        }
        propertiesController.revealActionTarget()
    }

    function canRevealActionPath() {
        return !root.multiMode
            && propertiesController.path.length > 0
            && (propertiesController.isDirectory || propertiesController.isDrive)
    }

    function openActionTerminal() {
        if (root.multiMode || propertiesController.path.length === 0) {
            return
        }
        propertiesController.openTerminalAtActionTarget()
    }

    Connections {
        target: propertiesController
        function onPropertiesChanged() {
            if (!root.unixModeDirty) {
                root.pendingUnixMode = propertiesController.unixMode
            }
            if (!root.unixOwnershipDirty) {
                root.pendingUnixOwner = propertiesController.unixOwnerName
                root.pendingUnixGroup = propertiesController.unixGroupName
            }
        }
        function onVisibleChanged() {
            if (!propertiesController.visible) {
                root.suppressDialog = false
                root.exportDialogPending = false
            }
        }
    }
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 150; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.97; duration: 120; easing.type: Easing.InCubic }
    }

    background: DialogShell {}

    component PropertyRow : DialogListRow {}

    component SectionCard : DialogSection {}












    readonly property bool multiMode: propertiesController.selectedCount > 1
    readonly property bool driveMode: !root.multiMode && propertiesController.isDrive
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useHighQualitySystemIcons : true
    readonly property bool hasDetailsTab: !root.multiMode && propertiesController.extraProperties.length > 0
    readonly property bool hasAccessOwnershipTab: !root.multiMode
                                                  && root.propertyGroupRows("accessOwnership.unix").length > 0
    readonly property bool hasHashesTab: !root.multiMode && !propertiesController.isDirectory && propertiesController.path !== ""
    readonly property int currentStackIndex: root.currentTab
    property int previousStackIndex: 0
    property int currentTab: 0
    property int requestedTab: -1
    readonly property real minTabStackHeight: 260
    readonly property real maxTabStackHeight: parent ? Math.max(root.minTabStackHeight, parent.height * 0.66) : 520

    readonly property var activeTabButton: {
        if (root.currentTab === 0) return tabBtnGeneral
        if (root.currentTab === 1) return tabBtnDetails
        if (root.currentTab === 2) return tabBtnAccess
        if (root.currentTab === 3) return tabBtnAccessOwnership
        if (root.currentTab === 4) return tabBtnHashes
        return tabBtnGeneral
    }

    onCurrentTabChanged: {
        // Capture old stack index before currentStackIndex recomputes
        previousStackIndex = root.currentStackIndex
    }

    function availableTab(tab) {
        if (tab === 1) {
            return root.hasDetailsTab
        }
        if (tab === 3) {
            return root.hasAccessOwnershipTab
        }
        if (tab === 4) {
            return root.hasHashesTab
        }
        return tab === 0 || tab === 2
    }

    function activeTabImplicitHeight() {
        if (root.currentStackIndex === 0) {
            return generalPage.contentImplicitHeight
        }
        if (root.currentStackIndex === 1 && root.hasDetailsTab) {
            return detailsPage.contentImplicitHeight
        }
        if (root.currentStackIndex === 2) {
            return accessPage.contentImplicitHeight
        }
        if (root.currentStackIndex === 3 && root.hasAccessOwnershipTab) {
            return ownershipPage.contentImplicitHeight
        }
        if (root.currentStackIndex === 4 && root.hasHashesTab) {
            return hashesPage.contentImplicitHeight
        }
        return generalPage.contentImplicitHeight
    }

    function tabContentY(scrollView, layout) {
        if (!scrollView || !layout || layout.implicitHeight >= scrollView.availableHeight) {
            return 4
        }
        return Math.max(4, Math.round((scrollView.availableHeight - layout.implicitHeight) / 2))
    }

    function normalizedTab(tab, fallbackTab) {
        if (root.availableTab(tab)) {
            return tab
        }
        if (root.availableTab(fallbackTab)) {
            return fallbackTab
        }
        return 0
    }

    readonly property real drivePercent: Math.max(0, Math.min(1, propertiesController.driveUsagePercent))
    readonly property color driveAccent: {
        switch (propertiesController.driveType) {
        case "usb": return Theme.actionIconColor("success")
        case "network": return Theme.actionIconColor("navigation")
        case "optical": return Theme.actionIconColor("warning")
        case "nvme": return Theme.actionIconColor("info")
        default: return Theme.accent
        }
    }

    function driveTypeLabel(type) {
        switch (String(type)) {
        case "nvme": return "NVMe SSD"
        case "ssd": return "Solid State Drive"
        case "hdd": return "Hard Disk Drive"
        case "usb": return "Removable USB Drive"
        case "optical": return "Optical / ISO Media"
        case "network": return "Network Drive"
        default: return "Storage Volume"
        }
    }

    function getFiletypeIcon(filePath) {
        if (typeof filePath !== "string" || filePath.length === 0) {
            return "qrc:/qt/qml/FM/qml/assets/filetypes-next/document.svg"
        }

        return fileTypeIconResolver.iconForPathHint(filePath, propertiesController.isPathDir(filePath))
    }

    function fileNameForPath(filePath) {
        if (typeof filePath !== "string" || filePath.length === 0) {
            return ""
        }
        var idx1 = filePath.lastIndexOf("/")
        var idx2 = filePath.lastIndexOf("\\")
        var idx = idx1 > idx2 ? idx1 : idx2
        return filePath.substring(idx + 1)
    }

    function parentPathForPath(filePath) {
        if (typeof filePath !== "string" || filePath.length === 0) {
            return ""
        }
        var idx1 = filePath.lastIndexOf("/")
        var idx2 = filePath.lastIndexOf("\\")
        var idx = idx1 > idx2 ? idx1 : idx2
        return idx > 0 ? filePath.substring(0, idx) : filePath
    }

    function hasAnyHashResult() {
        return propertiesController.checksumCalculator.md5 !== ""
            || propertiesController.checksumCalculator.sha1 !== ""
            || propertiesController.checksumCalculator.sha256 !== ""
    }

    function hashResultsText() {
        var lines = []
        lines.push("File: " + propertiesController.name)
        lines.push("Path: " + root.displayPath(propertiesController.path))
        if (propertiesController.checksumCalculator.md5 !== "") {
            lines.push("MD5: " + propertiesController.checksumCalculator.md5)
        }
        if (propertiesController.checksumCalculator.sha1 !== "") {
            lines.push("SHA-1: " + propertiesController.checksumCalculator.sha1)
        }
        if (propertiesController.checksumCalculator.sha256 !== "") {
            lines.push("SHA-256: " + propertiesController.checksumCalculator.sha256)
        }
        return lines.join("\n")
    }

    function capabilityDescription(label, allowed, state) {
        if (state === "unknown") {
            return "The effective access state could not be verified for this item."
        }
        switch (label) {
            case "Browse":
                return allowed ? "Directory listing is available." : "Directory listing is blocked or unavailable."
            case "Create inside":
                return allowed ? "New items can be created inside this folder." : "Creating items here is blocked."
            case "Traverse":
                return allowed ? "The folder can be traversed by file operations." : "Traversal is blocked by provider or permissions."
            case "Read":
                return allowed ? "File contents can be opened by the app." : "File contents are not readable."
            case "Modify":
                return allowed ? "The file can be written or replaced." : "Write access is blocked or read-only."
            case "Delete":
                return allowed ? "The item can enter the delete flow." : "Delete is blocked by permissions or provider rules."
            case "Execute":
                return allowed ? "The file can be launched as an executable target." : "Launch/execute is unavailable for this item."
            default:
                return allowed ? "Capability is available." : "Capability is unavailable."
        }
    }

    function propertyGroup(key) {
        var groups = propertiesController.propertyGroups
        for (var i = 0; i < groups.length; ++i) {
            var group = groups[i]
            if (group && group.key === key) {
                return group
            }
        }
        return null
    }

    function propertyGroupRows(key) {
        var group = root.propertyGroup(key)
        return group && group.rows ? group.rows : []
    }

    function propertyGroupTitle(key, fallbackTitle) {
        var group = root.propertyGroup(key)
        return group && group.title ? group.title.toUpperCase() : fallbackTitle
    }


    contentItem: ColumnLayout {
        id: mainLayout
        spacing: 0
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                root.close()
                event.accepted = true
            }
        }

        DialogHeader {
            Layout.fillWidth: true
            iconSource: root.multiMode
                ? "qrc:/qt/qml/FM/qml/assets/icons/select-all.svg"
                : (root.driveMode ? "qrc:/qt/qml/FM/qml/assets/icons/hard-drive.svg"
                : (propertiesController.path !== ""
                   ? (root.useNativeIcons
                      ? "image://icon/" + encodeURIComponent(propertiesController.path + "?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
                      : root.getFiletypeIcon(propertiesController.path))
                   : "qrc:/qt/qml/FM/qml/assets/icons/document.svg")
                )
            nativeIconPresentation: !root.multiMode && !root.driveMode && root.useNativeIcons && propertiesController.path !== ""
            title: propertiesController.name
            subtitle: root.driveMode ? root.driveTypeLabel(propertiesController.driveType) : propertiesController.typeText
            closeText: "x"
            onCloseRequested: root.close()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 8
            Layout.preferredHeight: quickActionsColumn.implicitHeight + 18
            radius: Theme.radiusSm
            color: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.72 : 0.86)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.90)
            border.width: 1
            clip: false

            ColumnLayout {
                id: quickActionsColumn

                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                anchors.topMargin: 6
                anchors.bottomMargin: 8
                spacing: 5

                Label {
                    text: "QUICK ACTIONS"
                    Layout.fillWidth: true
                    color: Theme.textSecondary
                    font.pixelSize: Theme.scaledSize(9)
                    font.weight: Font.DemiBold
                    opacity: 0.82
                }

                Flow {
                    id: quickActionsFlow
                    Layout.fillWidth: true
                    Layout.preferredHeight: quickActionsFlow.implicitHeight
                    spacing: 5

                    ActionPill {
                        text: "Path"
                        pillWidth: 56
                        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/clipboard-copy.svg"
                        accentColor: Theme.categoryInfo
                        enabled: root.actionPathsText().length > 0
                        onClicked: root.copyActionPaths()
                        ToolTip.text: root.multiMode ? "Copy selected paths" : "Copy path"
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                    }

                    ActionPill {
                        text: "Reveal"
                        pillWidth: 66
                        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/reveal.svg"
                        accentColor: Theme.categoryInfo
                        visible: root.canRevealActionPath()
                        enabled: root.canRevealActionPath()
                        onClicked: root.revealActionPath()
                        ToolTip.text: Qt.platform.os === "windows" ? "Show in Explorer" : "Reveal in file manager"
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                    }

                    ActionPill {
                        text: "Terminal"
                        pillWidth: 76
                        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/terminal.svg"
                        accentColor: Theme.categoryUtility
                        visible: root.canRevealActionPath()
                        enabled: root.canRevealActionPath()
                        onClicked: root.openActionTerminal()
                        ToolTip.text: Qt.platform.os === "windows" ? "Open PowerShell here" : "Open terminal here"
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                    }

                    ActionPill {
                        text: "JSON"
                        pillWidth: 58
                        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/document.svg"
                        accentColor: Theme.accent
                        onClicked: root.copyJson()
                        ToolTip.text: "Copy properties JSON"
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                    }

                    ActionPill {
                        id: exportButton
                        text: "Export"
                        pillWidth: 70
                        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/download.svg"
                        accentColor: Theme.categoryAction
                        onClicked: root.openExportMenu()
                        ToolTip.text: "Copy JSON or export to file"
                        ToolTip.visible: hovered
                        ToolTip.delay: 500

                        ThemedContextMenu {
                            id: exportMenu
                            implicitWidth: 160
                            onClosed: {
                                if (root.suppressDialog && !root.exportDialogPending) {
                                    propertiesController.visible = false
                                    root.suppressDialog = false
                                }
                            }

                            ThemedMenuItem {
                                text: "Copy Text"
                                onClicked: root.copyAll()
                            }

                            ThemedMenuSeparator {}

                            ThemedMenuItem {
                                text: "Export as TXT..."
                                onClicked: {
                                    root.exportDialogPending = true
                                    root.exportType = "txt"
                                    fileDialog.selectedFile = "file:///" + propertiesController.name.replace(/\\/g, "/") + "_properties.txt"
                                    fileDialog.open()
                                }
                            }
                            ThemedMenuItem {
                                text: "Export as JSON..."
                                onClicked: {
                                    root.exportDialogPending = true
                                    root.exportType = "json"
                                    fileDialog.selectedFile = "file:///" + propertiesController.name.replace(/\\/g, "/") + "_properties.json"
                                    fileDialog.open()
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.panelBorder
            opacity: 0.4
        }

        ScrollView {
            id: driveScrollView
            visible: root.driveMode
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: driveContentColumn.implicitHeight
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            clip: true

            ColumnLayout {
                id: driveContentColumn
                x: 18
                width: driveScrollView.availableWidth - 36
                spacing: 14

                Item { Layout.preferredHeight: 4; Layout.fillWidth: true }

                DrivePropertiesHero {
                    rootPathText: propertiesController.driveRootPath
                    nameText: propertiesController.name
                    typeText: root.driveTypeLabel(propertiesController.driveType)
                    accentColor: root.driveAccent
                    ready: propertiesController.driveReady
                    critical: propertiesController.driveCritical
                    percent: root.drivePercent
                    usedText: propertiesController.driveUsedText
                    freeText: propertiesController.driveFreeText
                    totalText: propertiesController.driveTotalText
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    DriveMetricCard {
                        label: "USED"
                        value: propertiesController.driveUsedText
                        subtext: Math.round(root.drivePercent * 100) + "% of capacity"
                        accentColor: propertiesController.driveCritical ? Theme.danger : root.driveAccent
                    }

                    DriveMetricCard {
                        label: "FREE"
                        value: propertiesController.driveFreeText
                        subtext: propertiesController.driveCritical ? "Low space" : "Available now"
                        accentColor: propertiesController.driveCritical ? Theme.warning : Theme.success
                    }

                    DriveMetricCard {
                        label: "TOTAL"
                        value: propertiesController.driveTotalText
                        subtext: propertiesController.driveFileSystem || "Unknown file system"
                        accentColor: Theme.secondaryAccent
                    }
                }

                SectionCard {
                    title: "VOLUME DETAILS"

                    DriveInfoRow {
                        label: "Root"
                        value: propertiesController.driveRootPath
                    }

                    DriveInfoRow {
                        label: "File system"
                        value: propertiesController.driveFileSystem || "Unknown"
                    }

                    DriveInfoRow {
                        label: "Drive type"
                        value: root.driveTypeLabel(propertiesController.driveType)
                    }

                    DriveInfoRow {
                        label: "Status"
                        value: propertiesController.driveCritical ? "Low free space" : (propertiesController.driveReady ? "Ready" : "Not ready")
                        valueColor: propertiesController.driveCritical ? Theme.danger : (propertiesController.driveReady ? Theme.success : Theme.warning)
                    }
                }

                SectionCard {
                    title: "TECHNICAL"
                    visible: propertiesController.extraProperties.length > 0

                    Repeater {
                        model: propertiesController.extraProperties
                        DriveInfoRow {
                            required property var modelData
                            label: modelData.label
                            value: modelData.value
                        }
                    }
                }

                Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
            }
        }

        ColumnLayout {
            id: fileLayout
            visible: !root.driveMode
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            onVisibleChanged: {
                if (visible && !root.hasDetailsTab && root.currentTab === 1) {
                    root.currentTab = root.normalizedTab(root.currentTab, 2)
                }
            }

            Rectangle {
                id: tabContainer
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 12
                implicitHeight: 40
                radius: 9
                color: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.92 : 0.98)
                border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.90 : 0.78)
                border.width: 1

                Rectangle {
                    id: tabHighlight
                    x: root.activeTabButton ? root.activeTabButton.x + tabRow.x : 0
                    y: root.activeTabButton ? root.activeTabButton.y + tabRow.y : 0
                    width: root.activeTabButton ? root.activeTabButton.width : 0
                    height: root.activeTabButton ? root.activeTabButton.height : 0
                    radius: 7
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

                    DialogTabButton {
                        id: tabBtnGeneral
                        text: "General"
                        active: root.currentTab === 0
                        onClicked: root.currentTab = 0
                    }

                    DialogTabButton {
                        id: tabBtnDetails
                        text: "Details"
                        visible: root.hasDetailsTab
                        active: root.currentTab === 1
                        onClicked: root.currentTab = 1
                    }

                    DialogTabButton {
                        id: tabBtnAccess
                        text: root.multiMode ? "Selection" : "Access"
                        active: root.currentTab === 2
                        onClicked: root.currentTab = 2
                    }

                    DialogTabButton {
                        id: tabBtnAccessOwnership
                        text: "Permission & Ownership"
                        visible: root.hasAccessOwnershipTab
                        active: root.currentTab === 3
                        onClicked: root.currentTab = 3
                    }

                    DialogTabButton {
                        id: tabBtnHashes
                        text: "Hashes"
                        visible: root.hasHashesTab
                        active: root.currentTab === 4
                        onClicked: root.currentTab = 4
                    }
                }
            }

            Item {
                id: tabStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                implicitHeight: Math.min(root.maxTabStackHeight,
                                         Math.max(root.minTabStackHeight, root.activeTabImplicitHeight()))

                PropertiesGeneralPage {
                    id: generalPage
                    currentIndex: root.currentStackIndex
                    rows: root.propertyGroupRows(root.multiMode ? "selection.summary" : "general")
                    sectionTitle: root.propertyGroupTitle(root.multiMode ? "selection.summary" : "general", "OVERVIEW")
                }

                PropertiesDetailsPage {
                    id: detailsPage
                    visible: root.hasDetailsTab
                    currentIndex: root.currentStackIndex
                    rows: root.propertyGroupRows("details")
                    sectionTitle: root.propertyGroupTitle("details", "FILE DETAILS")
                }

                PropertiesAccessPage {
                    id: accessPage
                    currentIndex: root.currentStackIndex
                    multiMode: root.multiMode
                    controller: propertiesController
                    capabilityRows: root.propertyGroupRows("access.capabilities")
                    attributeRows: root.propertyGroupRows("access.attributes")
                    capabilitiesTitle: root.propertyGroupTitle("access.capabilities", "CAPABILITIES")
                    attributesTitle: root.propertyGroupTitle("access.attributes", "ATTRIBUTES")
                    fileNameForPath: root.fileNameForPath
                    parentPathForPath: root.parentPathForPath
                    displayPath: root.displayPath
                    capabilityDescription: root.capabilityDescription
                    tabContentY: root.tabContentY
                    useNativeIcons: root.useNativeIcons
                }

                PropertiesOwnershipPage {
                    id: ownershipPage
                    pageVisible: root.hasAccessOwnershipTab
                    visible: pageVisible
                    currentIndex: root.currentStackIndex
                    controller: propertiesController
                    rows: root.propertyGroupRows("accessOwnership.unix")
                    sectionTitle: root.propertyGroupTitle("accessOwnership.unix", "ACCESS & OWNERSHIP")
                    adminEditMode: root.accessOwnershipAdminEditMode
                    pendingMode: root.pendingUnixMode
                    modeDirty: root.unixModeDirty
                    recursively: root.applyUnixModeRecursively
                    pendingOwner: root.pendingUnixOwner
                    pendingGroup: root.pendingUnixGroup
                    ownershipDirty: root.unixOwnershipDirty
                    modeEnabled: root.unixModeEnabled
                    setModeBit: root.setUnixModeBit
                    modeOctal: root.unixModeOctal
                    resetMode: root.resetUnixMode
                    applyMode: root.applyUnixMode
                    setRecursive: function(value) { root.applyUnixModeRecursively = value }
                    ownerEdited: function(value) {
                        root.pendingUnixOwner = value
                        root.unixOwnershipDirty = root.pendingUnixOwner !== propertiesController.unixOwnerName
                                                  || root.pendingUnixGroup !== propertiesController.unixGroupName
                    }
                    groupEdited: function(value) {
                        root.pendingUnixGroup = value
                        root.unixOwnershipDirty = root.pendingUnixOwner !== propertiesController.unixOwnerName
                                                  || root.pendingUnixGroup !== propertiesController.unixGroupName
                    }
                    resetOwnership: root.resetUnixOwnership
                    applyOwnership: root.applyUnixOwnership
                    tabContentY: root.tabContentY
                }

                PropertiesHashesPage {
                    id: hashesPage
                    pageVisible: root.hasHashesTab
                    currentIndex: root.currentStackIndex
                    calculator: propertiesController.checksumCalculator
                    targetPath: propertiesController.path
                    allHashesText: root.hashResultsText()
                    tabContentY: root.tabContentY
                    copyText: function(text) {
                        if (typeof workspaceController !== "undefined" && workspaceController) {
                            workspaceController.copyTextToClipboard(text)
                        }
                    }
                }
            }
        }

        DialogFooter {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            DialogActionButton {
                text: "Done"
                highlighted: true
                onClicked: root.close()
            }
        }

        ToolTip {
            id: copyAllTooltip
            text: "All properties copied to clipboard"
            timeout: 2000
        }

        ToolTip {
            id: actionTooltip
            text: ""
            timeout: 1600
        }

        ToolTip {
            id: copyJsonTooltip
            text: "Properties JSON copied"
            timeout: 2000
        }
    }

    FileDialog {
        id: fileDialog
        title: "Export Properties"
        fileMode: FileDialog.SaveFile
        defaultSuffix: root.exportType
        nameFilters: root.exportType === "txt" ? ["Text files (*.txt)"] : ["JSON files (*.json)"]
        onAccepted: {
            let content = root.exportType === "txt"
                ? propertiesController.exportableText() 
                : propertiesController.exportableJson()
            if (propertiesController.saveToFile(selectedFile, content)) {
                exportSuccessTooltip.show(exportSuccessTooltip.text)
            } else {
                exportFailureTooltip.show(exportFailureTooltip.text)
            }
            if (root.suppressDialog) {
                propertiesController.visible = false
                root.suppressDialog = false
                root.exportDialogPending = false
            }
        }
        onRejected: {
            if (root.suppressDialog) {
                propertiesController.visible = false
                root.suppressDialog = false
                root.exportDialogPending = false
            }
        }
    }

    property string exportType: "txt"

    ToolTip {
        id: exportSuccessTooltip
        text: "Properties exported successfully"
        timeout: 2000
    }

    Popup {
        id: specialModeConfirmation

        parent: Overlay.overlay
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: 410
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: DialogShell {}

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Label {
                text: "Confirm elevated permission"
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Label {
                text: "Set user ID and set group ID can let an executable run with another identity. Apply only to a file you trust."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                color: Theme.textSecondary
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 8

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    text: "Cancel"
                    onClicked: specialModeConfirmation.close()
                }

                DialogActionButton {
                    text: "Apply"
                    highlighted: true
                    primaryColor: Theme.warning
                    onClicked: {
                        specialModeConfirmation.close()
                        root.applyConfirmedUnixMode(root.confirmedUnixMode)
                    }
                }
            }
        }
    }

    ToolTip {
        id: exportFailureTooltip
        text: "Properties export failed"
        timeout: 2000
    }

     onClosed: {
         propertiesController.visible = false
         propertiesController.checksumCalculator.abort()
     }
     onHasDetailsTabChanged: {
         if (!hasDetailsTab && currentTab === 1) {
             currentTab = root.normalizedTab(currentTab, 2)
         }
     }
    onHasHashesTabChanged: {
        if (!hasHashesTab && currentTab === 4) {
            currentTab = root.normalizedTab(currentTab, 0)
        }
    }
    onMultiModeChanged: {
        if (multiMode && (currentTab === 1 || currentTab === 3 || currentTab === 4)) {
            currentTab = root.normalizedTab(currentTab, 2)
        }
    }
    onHasAccessOwnershipTabChanged: {
        if (!hasAccessOwnershipTab && currentTab === 3 && !propertiesController.isCalculating) {
            currentTab = root.normalizedTab(currentTab, 2)
        }
    }
    Connections {
        target: propertiesController
        function onIsCalculatingChanged() {
            if (!propertiesController.isCalculating
                    && !root.hasAccessOwnershipTab
                    && root.currentTab === 3) {
                root.currentTab = root.normalizedTab(root.currentTab, 2)
            }
        }
    }
    onVisibleChanged: {
        if (visible) {
            root.applyUnixModeRecursively = false
            root.resetUnixMode()
            root.resetUnixOwnership()
            if (requestedTab >= 0) {
                currentTab = requestedTab === 3 && propertiesController.isCalculating
                           ? 3
                           : root.normalizedTab(requestedTab, 0)
                requestedTab = -1
            } else {
                currentTab = 0
            }
        } else if (propertiesController.visible) {
            root.applyUnixModeRecursively = false
            propertiesController.visible = false
        }
    }
}
