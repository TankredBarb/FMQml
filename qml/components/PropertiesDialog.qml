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

    readonly property bool multiMode: propertiesController.selectedCount > 1
    readonly property bool driveMode: !root.multiMode && propertiesController.isDrive
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
                : (propertiesController.path !== "" ? "image://icon/" + encodeURIComponent(propertiesController.path) : "qrc:/qt/qml/FM/qml/assets/icons/document.svg")
                )
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

        ScrollView {
            id: scrollView
            visible: !root.driveMode
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: contentColumn.implicitHeight
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            clip: true

            ColumnLayout {
                id: contentColumn
                x: 16
                width: scrollView.availableWidth - 32
                spacing: 12
                
                Item { height: 4; Layout.fillWidth: true } // Top padding spacer

                // Overview Card
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

                // File Details Card
                SectionCard {
                    title: "FILE DETAILS"
                    visible: !root.multiMode && propertiesController.extraProperties.length > 0

                    Repeater {
                        model: propertiesController.extraProperties
                        PropertyRow {
                            label: modelData.label
                            value: modelData.value
                            emphasizeValue: true
                        }
                    }
                }

                // Timestamps Card
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

                // Permissions Card
                SectionCard {
                    title: "PERMISSIONS"
                    visible: !root.multiMode

                    Repeater {
                        model: [
                            { name: "Read", icon: "eye" },
                            { name: "Write", icon: "move" },
                            { name: "Execute", icon: "terminal" }
                        ]

                        PropertyRow {
                            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/" + modelData.icon + ".svg"
                            label: modelData.name
                            value: ""
                        }
                    }
                }

                // Selected Items Card
                SectionCard {
                    title: "SELECTED ITEMS"
                    visible: root.multiMode

                    Repeater {
                        model: propertiesController.selectedPaths

                        PropertyRow {
                            iconSource: "image://icon/" + encodeURIComponent(modelData)
                            label: modelData.split(/[/\\]/).pop()
                            value: ""
                        }
                    }
                }

                Item { height: 4; Layout.fillWidth: true } // Bottom padding spacer
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
    onVisibleChanged: {
        if (!visible && propertiesController.visible) {
            propertiesController.visible = false
        }
    }
}
