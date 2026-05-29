import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "dialogs"

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

    function copyAll() {
        if (typeof workspaceController !== "undefined" && workspaceController) {
            workspaceController.copyTextToClipboard(propertiesController.exportableText())
            copyAllTooltip.show(copyAllTooltip.text)
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
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    text: row.subtitle
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    font.pixelSize: 11
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
    readonly property real drivePercent: Math.max(0, Math.min(1, propertiesController.driveUsagePercent))
    readonly property color driveAccent: {
        switch (propertiesController.driveType) {
        case "usb": return "#22c55e"
        case "network": return "#0ea5e9"
        case "optical": return "#f59e0b"
        case "nvme": return "#14b8a6"
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
            return "qrc:/qt/qml/FM/qml/assets/filetypes/document.svg"
        }

        var isDir = propertiesController.isPathDir(filePath)
        if (isDir) {
            return "qrc:/qt/qml/FM/qml/assets/filetypes/folder.svg"
        }

        var suffix = propertiesController.getPathSuffix(filePath).toLowerCase()
        if (["jpg", "jpeg", "png", "gif", "bmp", "webp", "ico", "svg", "svgz", "avif", "heic", "tif", "tiff"].indexOf(suffix) >= 0) {
            return "qrc:/qt/qml/FM/qml/assets/filetypes/image.svg"
        }
        if (["mp3", "flac", "ogg", "m4a", "m4b", "wav", "wma", "aac", "opus"].indexOf(suffix) >= 0) {
            return "qrc:/qt/qml/FM/qml/assets/filetypes/music.svg"
        }
        if (["mp4", "avi", "mkv", "mov", "wmv", "webm", "flv", "m4v"].indexOf(suffix) >= 0) {
            return "qrc:/qt/qml/FM/qml/assets/filetypes/video.svg"
        }
        if (["zip", "rar", "7z", "tar", "gz", "bz2", "xz", "cab", "iso"].indexOf(suffix) >= 0) {
            return "qrc:/qt/qml/FM/qml/assets/filetypes/archive.svg"
        }
        if (["exe", "bat", "cmd", "ps1", "com", "msi", "dll", "sys"].indexOf(suffix) >= 0) {
            return "qrc:/qt/qml/FM/qml/assets/filetypes/executable.svg"
        }
        return "qrc:/qt/qml/FM/qml/assets/filetypes/document.svg"
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
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.1
                color: Theme.withAlpha(accentColor, 0.95)
            }

            Label {
                text: value
                Layout.fillWidth: true
                font.pixelSize: 19
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            Label {
                visible: subtext.length > 0
                text: subtext
                Layout.fillWidth: true
                font.pixelSize: 11
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
            font.pixelSize: 12
            color: Theme.textSecondary
            elide: Text.ElideRight
        }

        Label {
            text: value
            Layout.fillWidth: true
            font.pixelSize: 13
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
            font.pixelSize: 11
            font.weight: tabBtn.active ? Font.DemiBold : Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            Behavior on color {
                ColorAnimation { duration: 150 }
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
                   ? "image://icon/" + encodeURIComponent(propertiesController.path + "?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
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
            height: 1
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

                Item { height: 4; Layout.fillWidth: true }

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

                Item { height: 4; Layout.fillWidth: true }
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
                    root.currentTab = root.accessTabIndex
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

                implicitHeight: Math.max(
                    generalLayout.implicitHeight,
                    root.hasDetailsTab ? detailsLayout.implicitHeight : 0,
                    accessLayout.implicitHeight
                )

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
                        width: generalScrollView.availableWidth - 32
                        spacing: 12

                        Item { height: 4; Layout.fillWidth: true }

                        SectionCard {
                            title: "OVERVIEW"

                            PropertyRow {
                                label: root.multiMode ? "Parent" : "Location"
                                value: propertiesController.path
                                isLink: !root.multiMode
                            }

                            PropertyRow {
                                visible: root.multiMode
                                label: "Selection"
                                value: propertiesController.selectedCount + " items"
                            }

                            PropertyRow {
                                visible: root.multiMode && propertiesController.typeText.length > 0
                                label: "Type"
                                value: propertiesController.typeText
                            }

                            PropertyRow {
                                label: "Total Size"
                                value: propertiesController.sizeText
                                emphasizeValue: true
                                showBusy: propertiesController.isCalculating
                            }

                            PropertyRow {
                                visible: !root.multiMode && !!propertiesController.isDirectory
                                label: "Contents"
                                value: propertiesController.fileCount + " files, " + propertiesController.folderCount + " folders"
                            }
                        }

                        SectionCard {
                            title: "TIMESTAMPS"

                            PropertyRow {
                                label: root.multiMode ? "Oldest created" : "Created"
                                value: propertiesController.created
                            }

                            PropertyRow {
                                label: root.multiMode ? "Latest modified" : "Modified"
                                value: propertiesController.modified
                            }

                            PropertyRow {
                                label: root.multiMode ? "Latest accessed" : "Accessed"
                                value: propertiesController.accessed
                            }
                        }

                        Item { height: 4; Layout.fillWidth: true }
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
                        width: detailsScrollView.availableWidth - 32
                        spacing: 12

                        Item { height: 4; Layout.fillWidth: true }

                        SectionCard {
                            title: "FILE DETAILS"
                            visible: propertiesController.extraProperties.length > 0

                            Repeater {
                                model: propertiesController.extraProperties
                                PropertyRow {
                                    label: modelData.label
                                    value: modelData.value
                                    emphasizeValue: true
                                }
                            }
                        }

                        Item { height: 4; Layout.fillWidth: true }
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
                        width: accessScrollView.availableWidth - 32
                        spacing: 12

                        Item { height: 4; Layout.fillWidth: true }

                        SectionCard {
                            title: root.multiMode ? "SELECTED ITEMS" : "ACCESS"
                            visible: root.multiMode || propertiesController.accessProperties.length > 0

                            Repeater {
                                model: root.multiMode
                                       ? propertiesController.selectedPaths
                                       : propertiesController.accessProperties

                                PropertyRow {
                                    colorizeIcon: false
                                    iconSource: {
                                        if (root.multiMode) {
                                            if (typeof modelData !== "string") return ""
                                            if (!root.useNativeIcons) {
                                                return root.getFiletypeIcon(modelData)
                                            }
                                            return "image://icon/" + encodeURIComponent(modelData + "?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
                                        } else {
                                            return ""
                                        }
                                    }
                                    label: {
                                        if (root.multiMode) {
                                            if (typeof modelData !== "string") return ""
                                            var idx1 = modelData.lastIndexOf('/')
                                            var idx2 = modelData.lastIndexOf('\\')
                                            var idx = idx1 > idx2 ? idx1 : idx2
                                            return modelData.substring(idx + 1)
                                        } else {
                                            return (modelData && modelData.label) ? modelData.label : ""
                                        }
                                    }
                                    value: root.multiMode ? "" : (modelData && modelData.value ? modelData.value : "")
                                    valueColor: root.multiMode
                                                ? Theme.textPrimary
                                                : (modelData && modelData.allowed ? Theme.success : Theme.textSecondary)
                                }
                            }
                        }

                        SectionCard {
                            title: "ATTRIBUTES"
                            visible: !root.multiMode && propertiesController.attributeProperties.length > 0

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
                                    model: propertiesController.attributeProperties

                                    PropertyRow {
                                        visible: !propertiesController.canEditAttributes
                                                 || (modelData.label !== "Hidden" && modelData.label !== "Read-only")
                                        label: modelData.label
                                        value: modelData.value
                                        valueColor: modelData.enabled ? Theme.warning : Theme.textSecondary
                                    }
                                }
                            }
                        }

                        Item { height: 4; Layout.fillWidth: true }
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
                        width: hashesScrollView.availableWidth - 32
                        spacing: 12

                        Item { height: 4; Layout.fillWidth: true }

                        SectionCard {
                            title: "FILE CHECKSUMS"

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                // Progress bar visible when calculating
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    visible: propertiesController.checksumCalculator.busy
                                    spacing: 6

                                    ProgressBar {
                                        id: hashProgress
                                        Layout.fillWidth: true
                                        value: propertiesController.checksumCalculator.progress

                                        background: Rectangle { implicitHeight: 6; color: Theme.panelSurfaceSoft; radius: Theme.radiusSm }
                                        contentItem: Item {
                                            Rectangle {
                                                width: hashProgress.visualPosition * parent.width
                                                height: parent.height
                                                radius: 3
                                                color: Theme.accent
                                            }
                                        }
                                    }

                                    Label {
                                        text: "Calculating... " + Math.floor(hashProgress.value * 100) + "%"
                                        font.pixelSize: 11
                                        color: Theme.textSecondary
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }

                                // MD5 Row
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Label {
                                        text: "MD5"
                                        font.pixelSize: 10; font.bold: true; color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        TextField {
                                            text: propertiesController.checksumCalculator.md5
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: 11
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
                                            text: "Calculate"
                                            visible: propertiesController.checksumCalculator.md5 === ""
                                            enabled: !propertiesController.checksumCalculator.busy

                                            contentItem: Label {
                                                text: parent.text
                                                font.pixelSize: 11; font.weight: Font.Medium
                                                color: parent.enabled ? "white" : Theme.textSecondary
                                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                implicitWidth: 80; implicitHeight: 32
                                                radius: Theme.radiusSm
                                                color: parent.enabled ? Theme.accent : Theme.panelBorder
                                            }

                                            onClicked: propertiesController.checksumCalculator.calculate(propertiesController.path, "md5")
                                        }

                                        Button {
                                            visible: propertiesController.checksumCalculator.md5 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            flat: true
                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.panelSurfaceSoft : "transparent")
                                            }
                                            contentItem: Image {
                                                source: "qrc:/qt/qml/FM/qml/assets/icons/copy.svg"
                                                anchors.centerIn: parent
                                                width: 14; height: 14
                                                layer.enabled: true
                                                layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.textSecondary }
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
                                        font.pixelSize: 10; font.bold: true; color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        TextField {
                                            text: propertiesController.checksumCalculator.sha1
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: 11
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
                                            text: "Calculate"
                                            visible: propertiesController.checksumCalculator.sha1 === ""
                                            enabled: !propertiesController.checksumCalculator.busy

                                            contentItem: Label {
                                                text: parent.text
                                                font.pixelSize: 11; font.weight: Font.Medium
                                                color: parent.enabled ? "white" : Theme.textSecondary
                                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                implicitWidth: 80; implicitHeight: 32
                                                radius: Theme.radiusSm
                                                color: parent.enabled ? Theme.accent : Theme.panelBorder
                                            }

                                            onClicked: propertiesController.checksumCalculator.calculate(propertiesController.path, "sha1")
                                        }

                                        Button {
                                            visible: propertiesController.checksumCalculator.sha1 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            flat: true
                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.panelSurfaceSoft : "transparent")
                                            }
                                            contentItem: Image {
                                                source: "qrc:/qt/qml/FM/qml/assets/icons/copy.svg"
                                                anchors.centerIn: parent
                                                width: 14; height: 14
                                                layer.enabled: true
                                                layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.textSecondary }
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
                                        font.pixelSize: 10; font.bold: true; color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        TextField {
                                            text: propertiesController.checksumCalculator.sha256
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: 11
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
                                            text: "Calculate"
                                            visible: propertiesController.checksumCalculator.sha256 === ""
                                            enabled: !propertiesController.checksumCalculator.busy

                                            contentItem: Label {
                                                text: parent.text
                                                font.pixelSize: 11; font.weight: Font.Medium
                                                color: parent.enabled ? "white" : Theme.textSecondary
                                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                implicitWidth: 80; implicitHeight: 32
                                                radius: Theme.radiusSm
                                                color: parent.enabled ? Theme.accent : Theme.panelBorder
                                            }

                                            onClicked: propertiesController.checksumCalculator.calculate(propertiesController.path, "sha256")
                                        }

                                        Button {
                                            visible: propertiesController.checksumCalculator.sha256 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            flat: true
                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.panelSurfaceSoft : "transparent")
                                            }
                                            contentItem: Image {
                                                source: "qrc:/qt/qml/FM/qml/assets/icons/copy.svg"
                                                anchors.centerIn: parent
                                                width: 14; height: 14
                                                layer.enabled: true
                                                layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.textSecondary }
                                            }
                                            onClicked: workspaceController.copyTextToClipboard(propertiesController.checksumCalculator.sha256)
                                        }
                                    }
                                }
                            }
                        }

                        Item { height: 4; Layout.fillWidth: true }
                    }
                }
            }
        }

        DialogFooter {
            Layout.fillWidth: true

            DialogActionButton {
                text: "Copy All"
                onClicked: root.copyAll()

                ToolTip {
                    id: copyAllTooltip
                    text: "All properties copied to clipboard"
                    timeout: 2000
                }
            }

            DialogActionButton {
                id: exportButton
                text: "Export..."
                onClicked: root.openExportMenu()

                ThemedContextMenu {
                    id: exportMenu
                    implicitWidth: 150
                    onClosed: {
                        if (root.suppressDialog && !root.exportDialogPending) {
                            propertiesController.visible = false
                            root.suppressDialog = false
                        }
                    }

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

            Item {
                Layout.fillWidth: true
            }

            DialogActionButton {
                text: "Done"
                highlighted: true
                onClicked: root.close()
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "Export Properties"
        fileMode: FileDialog.SaveFile
        defaultSuffix: exportType
        nameFilters: exportType === "txt" ? ["Text files (*.txt)"] : ["JSON files (*.json)"]
        onAccepted: {
            let content = exportType === "txt" 
                ? propertiesController.exportableText() 
                : propertiesController.exportableJson()
            if (propertiesController.saveToFile(selectedFile, content)) {
                exportSuccessTooltip.show(exportSuccessTooltip.text)
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

     onClosed: {
         propertiesController.visible = false
         propertiesController.checksumCalculator.abort()
     }
     onHasDetailsTabChanged: {
         if (!hasDetailsTab && currentTab === 1) {
             currentTab = 2
         }
     }
     onMultiModeChanged: {
         if (multiMode && (currentTab === 1 || currentTab === 3)) {
             currentTab = 2
         }
     }
    onVisibleChanged: {
        if (visible) {
            if (requestedTab >= 0) {
                currentTab = requestedTab
                requestedTab = -1
            } else {
                currentTab = 0
            }
        } else if (propertiesController.visible) {
            propertiesController.visible = false
        }
    }
}
