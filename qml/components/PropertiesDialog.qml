import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "dialogs"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: root.driveMode ? 560 : 420
    padding: 0
    height: Math.min(mainLayout.implicitHeight, parent ? parent.height * 0.95 : 640)
    visible: propertiesController.visible
    
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
    readonly property int accessTabIndex: root.multiMode ? 1 : (root.hasDetailsTab ? 2 : 1)
    readonly property int currentStackIndex: {
        if (root.multiMode) {
            return Math.min(root.currentTab, 1)
        }
        if (!root.hasDetailsTab && root.currentTab === 1) {
            return 2
        }
        return root.currentTab
    }
    property int currentTab: 0
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
            color: tabBtn.active
                   ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.16 : 0.10)
                   : (tabBtn.hovered ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.05 : 0.035) : "transparent")
            border.color: tabBtn.active
                          ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.34 : 0.22)
                          : "transparent"
            border.width: 1
        }

        contentItem: Label {
            text: tabBtn.text
            color: tabBtn.active ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: 11
            font.weight: tabBtn.active ? Font.DemiBold : Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
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
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 12
                implicitHeight: 40
                radius: 9
                color: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.92 : 0.98)
                border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.90 : 0.78)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4

                    DialogTabButton {
                        text: "General"
                        active: root.currentTab === 0
                        onClicked: root.currentTab = 0
                    }

                    DialogTabButton {
                        text: "Details"
                        visible: root.hasDetailsTab
                        active: root.currentTab === 1
                        onClicked: root.currentTab = 1
                    }

                    DialogTabButton {
                        text: root.multiMode ? "Selection" : "Access"
                        active: root.currentTab === root.accessTabIndex
                        onClicked: root.currentTab = root.accessTabIndex
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentStackIndex

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    clip: true

                    ColumnLayout {
                        x: 16
                        width: parent.width - 32
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
                                value: propertiesController.sizeText + (propertiesController.isCalculating ? " (calculating)" : "")
                                emphasizeValue: true
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
                    visible: root.hasDetailsTab
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    clip: true

                    ColumnLayout {
                        x: 16
                        width: parent.width - 32
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    clip: true

                    ColumnLayout {
                        x: 16
                        width: parent.width - 32
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
                                    iconSource: root.multiMode
                                                ? "image://icon/" + encodeURIComponent(modelData + "?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
                                                : ""
                                    label: root.multiMode ? modelData.split(/[/\\]/).pop() : modelData.label
                                    value: root.multiMode ? "" : modelData.value
                                    valueColor: root.multiMode
                                                ? Theme.textPrimary
                                                : (modelData.allowed ? Theme.success : Theme.textSecondary)
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
            }
        }

        DialogFooter {
            Layout.fillWidth: true

            DialogActionButton {
                text: "Done"
                highlighted: true
                onClicked: root.close()
            }
        }
    }

    onClosed: propertiesController.visible = false
    onHasDetailsTabChanged: {
        if (!hasDetailsTab && currentTab === 1) {
            currentTab = accessTabIndex
        }
    }
    onMultiModeChanged: {
        if (multiMode && currentTab > 1) {
            currentTab = 1
        }
    }
    onVisibleChanged: {
        if (!visible && propertiesController.visible) {
            propertiesController.visible = false
        }
    }
}
