import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "common"
import "../style"

Popup {
    id: root

    required property var panel

    width: 240
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // ── Visual container ─────────────────────────────────────────────────────
    background: Rectangle {
        radius: 12
        color: Theme.menuSurface
        border.color: Theme.menuBorder
        border.width: 1

        layer.enabled: true
        layer.effect: null  // shadow via drop shadow if available

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: Qt.rgba(1, 1, 1, themeController.isDark ? 0.06 : 0.5)
            border.width: 1
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140; easing.type: Easing.OutCubic }
        NumberAnimation { property: "y";       from: y - 8; to: y; duration: 140; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100; easing.type: Easing.InCubic }
    }

    // ── Content ───────────────────────────────────────────────────────────────
    Column {
        width: root.width
        spacing: 0

        // Header
        Rectangle {
            width: parent.width
            height: 36
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 10
                spacing: 0

                Text {
                    text: "Columns"
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    font.weight: 600
                    Layout.fillWidth: true
                }

                // Reset button
                Rectangle {
                    width: 54
                    height: 20
                    radius: 5
                    color: resetMa.containsMouse
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                        : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 80 } }

                    Text {
                        anchors.centerIn: parent
                        text: "Reset"
                        color: Theme.accent
                        font.pixelSize: 10
                        font.weight: 500
                    }

                    MouseArea {
                        id: resetMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.panel.resetColumnsToDefaults()
                    }
                }
            }

            // Separator under header
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.menuBorder
            }
        }

        // ── Scrollable column list ────────────────────────────────────────────
        ScrollView {
            width: parent.width
            height: Math.min(contentHeight, 380)
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Column {
                width: root.width
                spacing: 0

                // ── Group: FILE SYSTEM ────────────────────────────────────────
                GroupHeader { text: "FILE SYSTEM" }

                // Name (locked — cannot hide)
                ColumnRow {
                    label: "Name"
                    iconSource: "../assets/icons/folder.svg"
                    iconColor: Theme.actionIconColor("folder")
                    sortRole: 0
                    checked: true
                    locked: true
                    panel: root.panel
                }

                ColumnRow {
                    label: "Size"
                    iconSource: "../assets/icons/size.svg"
                    iconColor: Theme.actionIconColor("info")
                    sortRole: 1
                    checked: root.panel.colShowSize
                    onToggled: root.panel.colShowSize = !root.panel.colShowSize
                    panel: root.panel
                }

                ColumnRow {
                    label: "Type"
                    iconSource: "../assets/icons/document.svg"
                    iconColor: Theme.actionIconColor("document")
                    sortRole: 2
                    checked: root.panel.colShowType
                    onToggled: root.panel.colShowType = !root.panel.colShowType
                    panel: root.panel
                }

                ColumnRow {
                    label: "Date Modified"
                    iconSource: "../assets/icons/calendar-clock.svg"
                    iconColor: Theme.actionIconColor("refresh")
                    sortRole: 3
                    checked: root.panel.colShowDate
                    onToggled: root.panel.colShowDate = !root.panel.colShowDate
                    panel: root.panel
                }

                ColumnRow {
                    label: "Date Created"
                    iconSource: "../assets/icons/calendar-plus.svg"
                    iconColor: Theme.actionIconColor("create")
                    sortRole: 4
                    checked: root.panel.colShowDateCreated
                    onToggled: root.panel.colShowDateCreated = !root.panel.colShowDateCreated
                    panel: root.panel
                }

                ColumnRow {
                    label: "Extension"
                    iconSource: "../assets/icons/extension.svg"
                    iconColor: Theme.actionIconColor("rename")
                    sortRole: 5
                    checked: root.panel.colShowExtension
                    onToggled: root.panel.colShowExtension = !root.panel.colShowExtension
                    panel: root.panel
                }

                ColumnRow {
                    label: "Attributes"
                    iconSource: "../assets/icons/attributes.svg"
                    iconColor: Theme.actionIconColor("attributes")
                    sortRole: -1
                    checked: root.panel.colShowAttributes
                    onToggled: root.panel.colShowAttributes = !root.panel.colShowAttributes
                    panel: root.panel
                }

                // ── Separator ─────────────────────────────────────────────────
                SectionDivider {}

                // ── Group: MEDIA ──────────────────────────────────────────────
                GroupHeader { text: "MEDIA" }

                ColumnRow {
                    label: "Dimensions"
                    iconSource: "../assets/filetypes/image.svg"
                    iconColor: Theme.actionIconColor("image")
                    sortRole: -1
                    checked: root.panel.colShowResolution
                    onToggled: root.panel.colShowResolution = !root.panel.colShowResolution
                    panel: root.panel
                }

                ColumnRow {
                    label: "Duration"
                    iconSource: "../assets/icons/duration.svg"
                    iconColor: Theme.actionIconColor("media")
                    sortRole: -1
                    checked: root.panel.colShowDuration
                    onToggled: root.panel.colShowDuration = !root.panel.colShowDuration
                    panel: root.panel
                }

                ColumnRow {
                    label: "Artist"
                    iconSource: "../assets/icons/artist.svg"
                    iconColor: Theme.actionIconColor("text-file")
                    sortRole: -1
                    checked: root.panel.colShowArtist
                    onToggled: root.panel.colShowArtist = !root.panel.colShowArtist
                    panel: root.panel
                }

                ColumnRow {
                    label: "Album"
                    iconSource: "../assets/icons/album.svg"
                    iconColor: Theme.actionIconColor("success")
                    sortRole: -1
                    checked: root.panel.colShowAlbum
                    onToggled: root.panel.colShowAlbum = !root.panel.colShowAlbum
                    panel: root.panel
                }

                ColumnRow {
                    label: "Bitrate"
                    iconSource: "../assets/icons/bitrate.svg"
                    iconColor: Theme.actionIconColor("danger")
                    sortRole: -1
                    checked: root.panel.colShowBitrate
                    onToggled: root.panel.colShowBitrate = !root.panel.colShowBitrate
                    panel: root.panel
                }

                // ── Separator ─────────────────────────────────────────────────
                SectionDivider {}

                // ── Group: TABLE STYLE ────────────────────────────────────────
                GroupHeader { text: "TABLE STYLE" }

                ColumnRow {
                    label: "Mixed Sorting"
                    iconSource: "../assets/icons/sort-mixed.svg"
                    iconColor: Theme.actionIconColor("sort")
                    sortRole: -1
                    checked: root.panel.controller.directoryModel.mixFilesAndFolders
                    onToggled: root.panel.controller.directoryModel.mixFilesAndFolders = !root.panel.controller.directoryModel.mixFilesAndFolders
                    panel: root.panel
                }

                ColumnRow {
                    label: "Zebra Striping"
                    iconSource: "../assets/icons/zebra.svg"
                    iconColor: Theme.actionIconColor("utility")
                    sortRole: -1
                    checked: root.panel.showZebraStriping
                    onToggled: root.panel.showZebraStriping = !root.panel.showZebraStriping
                    panel: root.panel
                }

                ColumnRow {
                    label: "Gridlines"
                    iconSource: "../assets/icons/grid.svg"
                    iconColor: Theme.actionIconColor("grid")
                    sortRole: -1
                    checked: root.panel.showGridlines
                    onToggled: root.panel.showGridlines = !root.panel.showGridlines
                    panel: root.panel
                }

                // Bottom padding
                Item { width: 1; height: 6 }
            }
        }
    }

    // ── Internal sub-components ───────────────────────────────────────────────

    component GroupHeader : Item {
        property string text: ""
        width: root.width
        height: 22

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: parent.text
            color: Theme.textSecondary
            font.pixelSize: 9
            font.weight: 600
            font.letterSpacing: 0.8
            opacity: 0.65
        }
    }

    component SectionDivider : Rectangle {
        width: root.width
        height: 1
        color: Theme.menuBorder
        opacity: 0.7
    }

    component ColumnRow : Item {
        id: colRow
        property string label: ""
        property string iconSource: ""
        property color iconColor: Theme.accent
        property int sortRole: -1
        property bool checked: false
        property bool locked: false
        required property var panel
        signal toggled()

        width: root.width
        height: 28

        // Hover background
        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            radius: 5
            color: rowMa.containsMouse
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.09 : 0.06)
                : "transparent"
            Behavior on color { ColorAnimation { duration: 80 } }
        }

        MouseArea {
            id: rowMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: colRow.locked ? Qt.ArrowCursor : Qt.PointingHandCursor
            onClicked: if (!colRow.locked) colRow.toggled()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            // Small colored icon
            Item {
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
                Layout.alignment: Qt.AlignVCenter

                RecolorSvgIcon {
                    id: colIcon
                    anchors.fill: parent
                    sourcePath: colRow.iconSource
                    recolorColor: colRow.iconColor
                    sourceSize: Qt.size(32, 32)
                    smooth: true
                    visible: colRow.iconSource.length > 0
                }
            }

            // Column label
            Text {
                text: colRow.label
                color: colRow.checked ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: 12
                font.weight: colRow.checked ? 500 : 400
                Layout.fillWidth: true
                Behavior on color { ColorAnimation { duration: 100 } }
            }

            // "Sort" pill — only for sortable columns
            Rectangle {
                visible: colRow.sortRole >= 0 && colRow.checked
                width: 26
                height: 14
                radius: 3
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)

                Text {
                    anchors.centerIn: parent
                    text: "sort"
                    color: Theme.accent
                    font.pixelSize: 8
                    font.weight: 500
                }
            }

            // Locked indicator
            Text {
                visible: colRow.locked
                text: "●"
                color: Theme.textSecondary
                font.pixelSize: 8
                opacity: 0.4
                Layout.preferredWidth: implicitWidth
            }

            // Checkbox
            Rectangle {
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
                radius: 4
                color: colRow.checked
                    ? (colRow.locked ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35) : Theme.accent)
                    : "transparent"
                border.color: colRow.checked
                    ? (colRow.locked ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5) : Theme.accent)
                    : Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.4)
                border.width: 1.2

                Behavior on color { ColorAnimation { duration: 120 } }
                Behavior on border.color { ColorAnimation { duration: 120 } }

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    color: "white"
                    font.pixelSize: 9
                    font.bold: true
                    visible: colRow.checked
                    opacity: colRow.locked ? 0.55 : 1.0
                }
            }
        }
    }
}
