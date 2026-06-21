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
    width: root.driveMode ? 560 : 420
    padding: 0
    height: Math.min(mainLayout.implicitHeight, parent ? parent.height * 0.95 : 640)
    visible: propertiesController.visible && !root.suppressDialog

    property bool suppressDialog: false
    property bool exportDialogPending: false
    property var appRoot: null

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

    function openExportMenuAtCursor() {
        exportMenu.popup()
    }

    function silentExport(type) {
        root.suppressDialog = true
        root.exportDialogPending = true
        root.exportType = (type && type.toLowerCase() === "txt") ? "txt" : "json"
        fileDialog.selectedFile = "file:///" + propertiesController.name.replace(/\\/g, "/") + "_properties." + root.exportType
        fileDialog.open()
    }

    function silentExportJson() {
        silentExport("json")
    }

    function activePanelController() {
        return root.appRoot && root.appRoot.activePanelController ? root.appRoot.activePanelController() : null
    }

    function activePanelCurrentPath() {
        const ctrl = root.activePanelController()
        return ctrl && ctrl.currentPath ? ctrl.currentPath : ""
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

    component AttributeToggleRow : Rectangle {
        id: row

        property string title: ""
        property string subtitle: ""
        property bool checked: false
        property bool toggleEnabled: true
        property color accentColor: Theme.accent
        signal toggled(bool checked)

        Layout.fillWidth: true
        implicitHeight: Math.max(54, rowLayout.implicitHeight + 12)
        radius: Theme.radiusSm
        color: rowMouse.containsMouse ? Theme.panelSurfaceSoft : Theme.panelSurface
        border.color: row.checked
                      ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.42 : 0.34)
                      : Theme.panelBorder
        border.width: 1
        opacity: row.toggleEnabled ? 1.0 : 0.55

        RowLayout {
            id: rowLayout
            anchors.fill: parent
            anchors.margins: 10
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: row.title
                    Layout.fillWidth: true
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    text: row.subtitle
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.textSecondary
                    visible: text.length > 0
                }
            }

            Switch {
                id: switchControl
                checked: row.checked
                enabled: row.toggleEnabled
                Layout.preferredWidth: 46
                Layout.preferredHeight: 26

                indicator: Rectangle {
                    implicitWidth: 42
                    implicitHeight: 22
                    x: switchControl.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: height / 2
                    color: switchControl.checked
                           ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.50 : 0.36)
                           : Theme.panelSurfaceSoft
                    border.color: switchControl.checked ? row.accentColor : Theme.panelBorder
                    border.width: 1

                    Rectangle {
                        x: switchControl.checked ? parent.width - width - 3 : 3
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        radius: 8
                        color: switchControl.checked ? row.accentColor : Theme.textSecondary

                        Behavior on x {
                            NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
                        }
                    }
                }

                contentItem: Item {}
            }
        }

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            enabled: row.toggleEnabled
            cursorShape: row.toggleEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: row.toggled(!row.checked)
        }
    }

    component AccessCapabilityRow : Rectangle {
        id: capabilityRow

        required property string label
        required property string value
        required property bool allowed
        property string accessState: allowed ? "allowed" : "denied"
        required property string description
        readonly property bool unknown: accessState === "unknown"
        readonly property color stateColor: unknown ? Theme.categoryInfo : (allowed ? Theme.success : Theme.warning)

        Layout.fillWidth: true
        implicitHeight: Math.max(58, capabilityLayout.implicitHeight + 16)
        radius: Theme.radiusSm
        color: capabilityMouse.containsMouse ? Theme.panelSurfaceSoft : Theme.panelSurface
        border.color: Theme.panelBorder
        border.width: 1

        MouseArea {
            id: capabilityMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }

        RowLayout {
            id: capabilityLayout
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 9
                Layout.preferredHeight: 34
                radius: 5
                color: capabilityRow.stateColor
                opacity: capabilityRow.allowed ? 0.82 : (capabilityRow.unknown ? 0.76 : 0.70)
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: capabilityRow.label
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Label {
                    text: capabilityRow.description
                    Layout.fillWidth: true
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                }
            }

            Label {
                text: capabilityRow.value
                color: capabilityRow.stateColor
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignRight
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    component ActionPill : Button {
        id: actionPill

        property color accentColor: Theme.accent
        property string iconSource: ""
        property int pillWidth: 64

        Layout.preferredHeight: 30
        Layout.minimumWidth: 54
        Layout.fillWidth: true
        Layout.preferredWidth: actionPill.pillWidth
        padding: 0
        hoverEnabled: true

        contentItem: RowLayout {
            id: actionContent
            spacing: 5

            Item { Layout.fillWidth: true }

            RecolorSvgIcon {
                Layout.preferredWidth: 13
                Layout.preferredHeight: 13
                visible: actionPill.iconSource.length > 0
                sourcePath: actionPill.iconSource
                recolorColor: actionPill.enabled ? actionPill.accentColor : Theme.textSecondary
                sourceSize.width: 13
                sourceSize.height: 13
                opacity: actionPill.enabled ? 0.95 : 0.45
            }

            Label {
                text: actionPill.text
                color: actionPill.enabled ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            Item { Layout.fillWidth: true }
        }

        background: Rectangle {
            implicitHeight: 30
            radius: Theme.radiusSm
            color: !actionPill.enabled
                   ? Theme.withAlpha(Theme.panelBorder, 0.45)
                   : (actionPill.pressed
                      ? Theme.surfaceActive
                      : (actionPill.hovered ? Theme.panelSurfaceSoft : Theme.panelSurface))
            border.color: Theme.withAlpha(actionPill.enabled ? actionPill.accentColor : Theme.panelBorder,
                                          actionPill.hovered ? 0.72 : (actionPill.enabled ? 0.46 : 0.55))
            border.width: 1

            Rectangle {
                visible: actionPill.enabled
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 3
                radius: 2
                color: actionPill.accentColor
                opacity: actionPill.hovered || actionPill.pressed ? 0.9 : 0.48
            }
        }
    }

    component ProgressRing : Item {
        id: progressRing

        property real value: 0
        property bool running: false
        property color accentColor: Theme.accent
        property real displayedValue: 0

        implicitWidth: 18
        implicitHeight: 18

        Behavior on displayedValue {
            NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
        }

        Component.onCompleted: displayedValue = Math.max(0, Math.min(1, value))
        onValueChanged: displayedValue = Math.max(0, Math.min(1, value))
        onDisplayedValueChanged: {
            if (!running) {
                ringCanvas.requestPaint()
            }
        }
        onRunningChanged: ringCanvas.requestPaint()
        onAccentColorChanged: ringCanvas.requestPaint()
        onWidthChanged: ringCanvas.requestPaint()
        onHeightChanged: ringCanvas.requestPaint()

        RotationAnimator on rotation {
            from: 0
            to: 360
            duration: 1100
            loops: Animation.Infinite
            running: progressRing.running
        }

        Canvas {
            id: ringCanvas
            anchors.fill: parent
            antialiasing: true

            onPaint: {
                const ctx = getContext("2d")
                ctx.setTransform(1, 0, 0, 1, 0, 0)
                ctx.clearRect(0, 0, width, height)

                const size = Math.min(width, height)
                const center = size / 2
                const lineWidth = 2.4
                const radius = (size - lineWidth) / 2
                const start = -Math.PI / 2
                const progress = progressRing.running
                                 ? 0.34
                                 : Math.max(0, Math.min(1, progressRing.displayedValue))

                ctx.lineCap = "round"
                ctx.lineWidth = lineWidth
                ctx.strokeStyle = Theme.withAlpha(Theme.panelBorder, 0.76)
                ctx.beginPath()
                ctx.arc(center, center, radius, 0, Math.PI * 2)
                ctx.stroke()

                ctx.strokeStyle = progressRing.accentColor
                ctx.beginPath()
                ctx.arc(center, center, radius, start, start + Math.PI * 2 * progress)
                ctx.stroke()
            }
        }
    }

    readonly property bool multiMode: propertiesController.selectedCount > 1
    readonly property bool driveMode: !root.multiMode && propertiesController.isDrive
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useHighQualitySystemIcons : true
    readonly property bool hasDetailsTab: !root.multiMode && propertiesController.extraProperties.length > 0
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
        if (root.currentTab === 3) return tabBtnHashes
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
            return root.hasHashesTab
        }
        return tab === 0 || tab === 2
    }

    function activeTabImplicitHeight() {
        if (root.currentStackIndex === 0) {
            return generalLayout.implicitHeight
        }
        if (root.currentStackIndex === 1 && root.hasDetailsTab) {
            return detailsLayout.implicitHeight
        }
        if (root.currentStackIndex === 2) {
            return accessLayout.implicitHeight
        }
        if (root.currentStackIndex === 3 && root.hasHashesTab) {
            return hashesLayout.implicitHeight
        }
        return generalLayout.implicitHeight
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

    component DriveMetricCard : Rectangle {
        required property string label
        required property string value
        property string subtext: ""
        property color accentColor: Theme.accent

        Layout.fillWidth: true
        Layout.preferredHeight: 76
        radius: Theme.radiusLg
        color: Theme.withAlpha(accentColor, themeController.isDark ? 0.13 : 0.08)
        border.color: Theme.withAlpha(accentColor, themeController.isDark ? 0.28 : 0.18)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 3

            Label {
                text: label
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMicro
                font.bold: true
                font.letterSpacing: 0
                color: Theme.withAlpha(accentColor, 0.95)
            }

            Label {
                text: value
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.scaledSize(19)
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            Label {
                visible: subtext.length > 0
                text: subtext
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textSecondary
                elide: Text.ElideRight
            }
        }
    }

    component DriveInfoRow : RowLayout {
        required property string label
        required property string value
        property color valueColor: Theme.textPrimary

        Layout.fillWidth: true
        spacing: 12

        Label {
            text: label
            Layout.preferredWidth: 112
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            color: Theme.textSecondary
            elide: Text.ElideRight
        }

        Label {
            text: value
            Layout.fillWidth: true
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.Medium
            color: valueColor
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideMiddle
        }
    }

    component DialogTabButton : Button {
        id: tabBtn

        property bool active: false

        Layout.fillWidth: true
        implicitHeight: 30
        leftPadding: 12
        rightPadding: 12
        topPadding: 0
        bottomPadding: 0

        background: Rectangle {
            radius: 7
            color: !tabBtn.active && tabBtn.hovered
                   ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.05 : 0.035)
                   : "transparent"
            border.color: "transparent"
            border.width: 1
        }

        contentItem: Label {
            text: tabBtn.text
            color: tabBtn.active ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            font.weight: tabBtn.active ? Font.DemiBold : Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }
    }

    component SelectedPathRow : Rectangle {
        id: selectedRow

        required property string filePath
        required property string fileName
        required property string parentPath
        required property bool isDirectory
        required property string suffix
        property bool useNativeIcons: true

        width: ListView.view ? ListView.view.width : 0
        height: 42
        radius: Theme.radiusSm
        color: rowMouse.containsMouse ? Theme.panelSurfaceSoft : Theme.panelSurface
        border.color: Theme.panelBorder
        border.width: 1

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 9

            FileIconCell {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                iconSize: 18
                path: selectedRow.filePath
                suffix: selectedRow.suffix
                isDirectory: selectedRow.isDirectory
                useNativeIcons: selectedRow.useNativeIcons
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    clip: true

                    Label {
                        text: {
                            if (selectedRow.isDirectory || !selectedRow.suffix) return selectedRow.fileName
                            const extLen = selectedRow.suffix.length
                            if (extLen > 0 && selectedRow.fileName.endsWith("." + selectedRow.suffix)) {
                                return selectedRow.fileName.substring(0, selectedRow.fileName.length - extLen - 1)
                            }
                            return selectedRow.fileName
                        }
                        color: selectedRow.isDirectory ? TextColors.folderNameText : TextColors.fileNameText
                        elide: Text.ElideRight
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                    }

                    Label {
                        visible: !selectedRow.isDirectory && !!selectedRow.suffix && selectedRow.fileName.endsWith("." + selectedRow.suffix)
                        text: "." + selectedRow.suffix
                        color: TextColors.fileExtensionText
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: false
                    }
                }

                Label {
                    text: selectedRow.parentPath
                    Layout.fillWidth: true
                    color: TextColors.filePathText
                    font.pixelSize: Theme.fontSizeMicro
                    elide: Text.ElideMiddle
                }
            }
        }
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
            Layout.preferredHeight: 56
            radius: Theme.radiusSm
            color: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.72 : 0.86)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.90)
            border.width: 1
            clip: true

            ColumnLayout {
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

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
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
                        id: tabBtnHashes
                        text: "Hashes"
                        visible: root.hasHashesTab
                        active: root.currentTab === 3
                        onClicked: root.currentTab = 3
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

                ScrollView {
                    id: generalScrollView
                    anchors.fill: parent
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    clip: true
                    enabled: root.currentStackIndex === 0

                    opacity: root.currentStackIndex === 0 ? 1.0 : 0.0
                    z: root.currentStackIndex === 0 ? 1 : 0
                    transform: Translate {
                        x: root.currentStackIndex === 0 ? 0 : (0 < root.currentStackIndex ? -400 : 400)
                        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    }
                    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.InOutQuad } }

                    ColumnLayout {
                        id: generalLayout
                        x: 16
                        y: root.tabContentY(generalScrollView, generalLayout)
                        width: generalScrollView.availableWidth - 32
                        spacing: 12

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }

                        SectionCard {
                            title: root.propertyGroupTitle(root.multiMode ? "selection.summary" : "general", "OVERVIEW")
                            visible: root.propertyGroupRows(root.multiMode ? "selection.summary" : "general").length > 0

                            Repeater {
                                model: root.propertyGroupRows(root.multiMode ? "selection.summary" : "general")

                                PropertyRow {
                                    required property var modelData
                                    label: modelData && modelData.label ? modelData.label : ""
                                    value: modelData && modelData.value ? modelData.value : ""
                                    isLink: modelData && (modelData.key === "general.location" || modelData.key === "general.fullPath" || modelData.key === "selection.location")
                                    valueMaximumLineCount: isLink ? 4 : 2
                                    emphasizeValue: modelData && modelData.emphasize ? true : false
                                    showBusy: modelData && modelData.busy ? true : false
                                    accentColor: Theme.accent
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
                    }
                }

                ScrollView {
                    id: detailsScrollView
                    visible: root.hasDetailsTab
                    anchors.fill: parent
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    clip: true
                    enabled: root.currentStackIndex === 1

                    opacity: root.currentStackIndex === 1 ? 1.0 : 0.0
                    z: root.currentStackIndex === 1 ? 1 : 0
                    transform: Translate {
                        x: root.currentStackIndex === 1 ? 0 : (1 < root.currentStackIndex ? -400 : 400)
                        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    }
                    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.InOutQuad } }

                    ColumnLayout {
                        id: detailsLayout
                        x: 16
                        y: root.tabContentY(detailsScrollView, detailsLayout)
                        width: detailsScrollView.availableWidth - 32
                        spacing: 12

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }

                        SectionCard {
                            title: root.propertyGroupTitle("details", "FILE DETAILS")
                            visible: root.propertyGroupRows("details").length > 0

                            Repeater {
                                model: root.propertyGroupRows("details")
                                PropertyRow {
                                    required property var modelData
                                    label: modelData && modelData.label ? modelData.label : ""
                                    value: modelData && modelData.value ? modelData.value : ""
                                    emphasizeValue: modelData && modelData.emphasize ? true : false
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
                    }
                }

                ScrollView {
                    id: accessScrollView
                    anchors.fill: parent
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    clip: true
                    enabled: root.currentStackIndex === 2

                    opacity: root.currentStackIndex === 2 ? 1.0 : 0.0
                    z: root.currentStackIndex === 2 ? 1 : 0
                    transform: Translate {
                        x: root.currentStackIndex === 2 ? 0 : (2 < root.currentStackIndex ? -400 : 400)
                        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    }
                    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.InOutQuad } }

                    ColumnLayout {
                        id: accessLayout
                        x: 16
                        y: root.tabContentY(accessScrollView, accessLayout)
                        width: accessScrollView.availableWidth - 32
                        spacing: 12

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }

                        SectionCard {
                            title: "SELECTED ITEMS"
                            visible: root.multiMode

                            ListView {
                                id: selectedPathsList
                                visible: root.multiMode
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(320, Math.max(112, propertiesController.selectedCount * 44))
                                clip: true
                                model: propertiesController.selectedPaths
                                spacing: 4
                                boundsBehavior: Flickable.StopAtBounds
                                cacheBuffer: Math.max(0, height * 2)
                                reuseItems: true
                                delegate: SelectedPathRow {
                                    required property string modelData
                                    readonly property string pathValue: modelData
                                    filePath: modelData
                                    fileName: root.fileNameForPath(pathValue)
                                    parentPath: root.displayPath(root.parentPathForPath(pathValue))
                                    isDirectory: propertiesController.isPathDir(pathValue)
                                    suffix: propertiesController.getPathSuffix(pathValue).toLowerCase()
                                    useNativeIcons: root.useNativeIcons
                                }

                                ScrollBar.vertical: ScrollBar {
                                    policy: selectedPathsList.contentHeight > selectedPathsList.height
                                            ? ScrollBar.AlwaysOn
                                            : ScrollBar.AsNeeded
                                    width: 10
                                }
                            }
                        }

                        SectionCard {
                            title: root.propertyGroupTitle("access.capabilities", "CAPABILITIES")
                            visible: !root.multiMode && root.propertyGroupRows("access.capabilities").length > 0

                            Repeater {
                                model: root.propertyGroupRows("access.capabilities")

                                AccessCapabilityRow {
                                    required property var modelData
                                    label: (modelData && modelData.label) ? modelData.label : ""
                                    value: (modelData && modelData.value ? modelData.value : "")
                                    allowed: modelData && modelData.allowed ? true : false
                                    accessState: (modelData && modelData.state) ? modelData.state : (allowed ? "allowed" : "denied")
                                    description: root.capabilityDescription((modelData && modelData.label) ? modelData.label : "",
                                                                            modelData && modelData.allowed ? true : false,
                                                                            (modelData && modelData.state) ? modelData.state : "")
                                }
                            }
                        }

                        SectionCard {
                            title: root.propertyGroupTitle("access.unix", "OWNERSHIP / UNIX MODE")
                            visible: !root.multiMode && root.propertyGroupRows("access.unix").length > 0

                            Repeater {
                                model: root.propertyGroupRows("access.unix")

                                PropertyRow {
                                    required property var modelData
                                    label: modelData && modelData.label ? modelData.label : ""
                                    value: modelData && modelData.value ? modelData.value : ""
                                    valueMaximumLineCount: 2
                                }
                            }
                        }

                        SectionCard {
                            title: root.propertyGroupTitle("access.attributes", "ATTRIBUTES")
                            visible: !root.multiMode && root.propertyGroupRows("access.attributes").length > 0

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                AttributeToggleRow {
                                    visible: propertiesController.canEditAttributes
                                    title: "Hidden"
                                    subtitle: "Hide this item from normal file listings."
                                    checked: propertiesController.hiddenAttribute
                                    accentColor: Theme.warning
                                    onToggled: (checked) => propertiesController.setHiddenAttribute(checked)
                                }

                                AttributeToggleRow {
                                    visible: propertiesController.canEditAttributes
                                    title: "Read-only"
                                    subtitle: "Mark this item as read-only at the filesystem attribute level."
                                    checked: propertiesController.readOnlyAttribute
                                    accentColor: Theme.accent
                                    onToggled: (checked) => propertiesController.setReadOnlyAttribute(checked)
                                }

                                Repeater {
                                    model: root.propertyGroupRows("access.attributes")

                                    PropertyRow {
                                        required property var modelData
                                        visible: !(modelData && modelData.editable)
                                        label: modelData && modelData.label ? modelData.label : ""
                                        value: modelData && modelData.value ? modelData.value : ""
                                        valueColor: modelData && modelData.enabled ? Theme.warning : Theme.textSecondary
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
                    }
                }

                ScrollView {
                    id: hashesScrollView
                    visible: root.hasHashesTab
                    anchors.fill: parent
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    clip: true
                    enabled: root.currentStackIndex === 3

                    opacity: root.currentStackIndex === 3 ? 1.0 : 0.0
                    z: root.currentStackIndex === 3 ? 1 : 0
                    transform: Translate {
                        x: root.currentStackIndex === 3 ? 0 : (3 < root.currentStackIndex ? -400 : 400)
                        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    }
                    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.InOutQuad } }

                    ColumnLayout {
                        id: hashesLayout
                        x: 16
                        y: root.tabContentY(hashesScrollView, hashesLayout)
                        width: hashesScrollView.availableWidth - 32
                        spacing: 12

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }

                        SectionCard {
                            title: "FILE CHECKSUMS"

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    ProgressRing {
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                        visible: propertiesController.checksumCalculator.busy
                                        running: propertiesController.checksumCalculator.busy
                                        value: propertiesController.checksumCalculator.progress
                                        accentColor: Theme.accent
                                    }

                                    Label {
                                        text: propertiesController.checksumCalculator.busy
                                              ? "Calculating " + Math.floor(propertiesController.checksumCalculator.progress * 100) + "%"
                                              : "Calculate hashes for this file and copy deterministic output with file context."
                                        Layout.fillWidth: true
                                        color: propertiesController.checksumCalculator.busy ? Theme.textPrimary : Theme.textSecondary
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.weight: propertiesController.checksumCalculator.busy ? Font.Medium : Font.Normal
                                        elide: Text.ElideRight
                                    }

                                    Button {
                                        id: copyAllHashesButton
                                        text: "Copy All"
                                        enabled: root.hasAnyHashResult()
                                        visible: !propertiesController.checksumCalculator.busy

                                        contentItem: Label {
                                            text: copyAllHashesButton.text
                                            font.pixelSize: Theme.fontSizeCaption
                                            font.weight: Font.Medium
                                            color: copyAllHashesButton.enabled ? Theme.textPrimary : Theme.textSecondary
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        background: Rectangle {
                                            implicitWidth: 82
                                            implicitHeight: 30
                                            radius: Theme.radiusSm
                                            color: copyAllHashesButton.enabled
                                                   ? (copyAllHashesButton.hovered ? Theme.panelSurfaceSoft : Theme.panelSurface)
                                                   : Theme.panelBorder
                                            border.color: Theme.panelBorder
                                            border.width: 1
                                        }

                                        onClicked: workspaceController.copyTextToClipboard(root.hashResultsText())
                                    }

                                    Button {
                                        id: cancelHashesButton
                                        text: "Cancel"
                                        visible: propertiesController.checksumCalculator.busy

                                        contentItem: Label {
                                            text: cancelHashesButton.text
                                            font.pixelSize: Theme.fontSizeCaption
                                            font.weight: Font.Medium
                                            color: Theme.warning
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        background: Rectangle {
                                            implicitWidth: 74
                                            implicitHeight: 30
                                            radius: Theme.radiusSm
                                            color: cancelHashesButton.hovered ? Theme.panelSurfaceSoft : Theme.panelSurface
                                            border.color: Theme.withAlpha(Theme.warning, 0.45)
                                            border.width: 1
                                        }

                                        onClicked: propertiesController.checksumCalculator.abort()
                                    }
                                }

                                // MD5 Row
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Label {
                                        text: "MD5"
                                        font.pixelSize: Theme.fontSizeMicro; font.bold: true; color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        TextField {
                                            text: propertiesController.checksumCalculator.md5
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                            Layout.fillWidth: true
                                            color: Theme.textPrimary
                                            selectByMouse: true
                                            leftPadding: 10
                                            background: Rectangle {
                                                color: Theme.panelSurfaceSoft
                                                radius: Theme.radiusSm
                                                border.color: Theme.panelBorder; border.width: 1
                                            }
                                        }

                                        Button {
                                            id: md5CalculateButton
                                            text: "Calculate"
                                            visible: propertiesController.checksumCalculator.md5 === ""
                                            enabled: !propertiesController.checksumCalculator.busy

                                            contentItem: Label {
                                                text: md5CalculateButton.text
                                                font.pixelSize: Theme.fontSizeCaption; font.weight: Font.Medium
                                                color: md5CalculateButton.enabled ? Theme.readableOn(Theme.accent, Theme.accentText) : Theme.textSecondary
                                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                implicitWidth: 80; implicitHeight: 32
                                                radius: Theme.radiusSm
                                                color: md5CalculateButton.enabled ? Theme.accent : Theme.panelBorder
                                            }

                                            onClicked: propertiesController.checksumCalculator.calculate(propertiesController.path, "md5")
                                        }

                                        Button {
                                            id: md5CopyButton
                                            visible: propertiesController.checksumCalculator.md5 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            flat: true
                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: md5CopyButton.pressed ? Theme.surfaceActive : (md5CopyButton.hovered ? Theme.panelSurfaceSoft : "transparent")
                                            }
                                            contentItem: RecolorSvgIcon {
                                                sourcePath: "qrc:/qt/qml/FM/qml/assets/icons/clipboard-copy.svg"
                                                recolorColor: Theme.textSecondary
                                                anchors.centerIn: parent
                                                width: 14; height: 14
                                            }
                                            onClicked: workspaceController.copyTextToClipboard(propertiesController.checksumCalculator.md5)
                                        }
                                    }
                                }

                                // SHA-1 Row
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Label {
                                        text: "SHA-1"
                                        font.pixelSize: Theme.fontSizeMicro; font.bold: true; color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        TextField {
                                            text: propertiesController.checksumCalculator.sha1
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                            Layout.fillWidth: true
                                            color: Theme.textPrimary
                                            selectByMouse: true
                                            leftPadding: 10
                                            background: Rectangle {
                                                color: Theme.panelSurfaceSoft
                                                radius: Theme.radiusSm
                                                border.color: Theme.panelBorder; border.width: 1
                                            }
                                        }

                                        Button {
                                            id: sha1CalculateButton
                                            text: "Calculate"
                                            visible: propertiesController.checksumCalculator.sha1 === ""
                                            enabled: !propertiesController.checksumCalculator.busy

                                            contentItem: Label {
                                                text: sha1CalculateButton.text
                                                font.pixelSize: Theme.fontSizeCaption; font.weight: Font.Medium
                                                color: sha1CalculateButton.enabled ? Theme.readableOn(Theme.accent, Theme.accentText) : Theme.textSecondary
                                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                implicitWidth: 80; implicitHeight: 32
                                                radius: Theme.radiusSm
                                                color: sha1CalculateButton.enabled ? Theme.accent : Theme.panelBorder
                                            }

                                            onClicked: propertiesController.checksumCalculator.calculate(propertiesController.path, "sha1")
                                        }

                                        Button {
                                            id: sha1CopyButton
                                            visible: propertiesController.checksumCalculator.sha1 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            flat: true
                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: sha1CopyButton.pressed ? Theme.surfaceActive : (sha1CopyButton.hovered ? Theme.panelSurfaceSoft : "transparent")
                                            }
                                            contentItem: RecolorSvgIcon {
                                                sourcePath: "qrc:/qt/qml/FM/qml/assets/icons/clipboard-copy.svg"
                                                recolorColor: Theme.textSecondary
                                                anchors.centerIn: parent
                                                width: 14; height: 14
                                            }
                                            onClicked: workspaceController.copyTextToClipboard(propertiesController.checksumCalculator.sha1)
                                        }
                                    }
                                }

                                // SHA-256 Row
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Label {
                                        text: "SHA-256"
                                        font.pixelSize: Theme.fontSizeMicro; font.bold: true; color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        TextField {
                                            text: propertiesController.checksumCalculator.sha256
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                            Layout.fillWidth: true
                                            color: Theme.textPrimary
                                            selectByMouse: true
                                            leftPadding: 10
                                            background: Rectangle {
                                                color: Theme.panelSurfaceSoft
                                                radius: Theme.radiusSm
                                                border.color: Theme.panelBorder; border.width: 1
                                            }
                                        }

                                        Button {
                                            id: sha256CalculateButton
                                            text: "Calculate"
                                            visible: propertiesController.checksumCalculator.sha256 === ""
                                            enabled: !propertiesController.checksumCalculator.busy

                                            contentItem: Label {
                                                text: sha256CalculateButton.text
                                                font.pixelSize: Theme.fontSizeCaption; font.weight: Font.Medium
                                                color: sha256CalculateButton.enabled ? Theme.readableOn(Theme.accent, Theme.accentText) : Theme.textSecondary
                                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                implicitWidth: 80; implicitHeight: 32
                                                radius: Theme.radiusSm
                                                color: sha256CalculateButton.enabled ? Theme.accent : Theme.panelBorder
                                            }

                                            onClicked: propertiesController.checksumCalculator.calculate(propertiesController.path, "sha256")
                                        }

                                        Button {
                                            id: sha256CopyButton
                                            visible: propertiesController.checksumCalculator.sha256 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            flat: true
                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: sha256CopyButton.pressed ? Theme.surfaceActive : (sha256CopyButton.hovered ? Theme.panelSurfaceSoft : "transparent")
                                            }
                                            contentItem: RecolorSvgIcon {
                                                sourcePath: "qrc:/qt/qml/FM/qml/assets/icons/clipboard-copy.svg"
                                                recolorColor: Theme.textSecondary
                                                anchors.centerIn: parent
                                                width: 14; height: 14
                                            }
                                            onClicked: workspaceController.copyTextToClipboard(propertiesController.checksumCalculator.sha256)
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
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
         if (!hasHashesTab && currentTab === 3) {
             currentTab = root.normalizedTab(currentTab, 0)
         }
     }
     onMultiModeChanged: {
         if (multiMode && (currentTab === 1 || currentTab === 3)) {
             currentTab = root.normalizedTab(currentTab, 2)
         }
     }
    onVisibleChanged: {
        if (visible) {
            if (requestedTab >= 0) {
                currentTab = root.normalizedTab(requestedTab, 0)
                requestedTab = -1
            } else {
                currentTab = 0
            }
        } else if (propertiesController.visible) {
            propertiesController.visible = false
        }
    }
}
