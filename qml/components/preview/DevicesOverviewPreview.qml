import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"

Item {
    id: root

    property bool compact: false
    property var placesModel: typeof workspaceController !== "undefined" && workspaceController
                              ? workspaceController.placesModel : null
    property var driveRows: []

    readonly property color accent: Theme.actionIconColor("drive")
    readonly property int mountedCount: driveRows.length
    implicitHeight: Math.max(224, 150 + mountedCount * (compact ? 38 : 46))
    readonly property real totalBytes: driveRows.reduce(function(total, drive) {
        return total + Number(drive.totalBytes || 0)
    }, 0)
    readonly property real freeBytes: driveRows.reduce(function(total, drive) {
        return total + Number(drive.freeBytes || 0)
    }, 0)

    function formatBytes(value) {
        const bytes = Number(value || 0)
        if (bytes <= 0) return ""
        const units = ["B", "KB", "MB", "GB", "TB", "PB"]
        let amount = bytes
        let unit = 0
        while (amount >= 1024 && unit < units.length - 1) {
            amount /= 1024
            ++unit
        }
        return (amount >= 100 || unit === 0 ? Math.round(amount) : amount.toFixed(1)) + " " + units[unit]
    }

    function rebuildDrives() {
        if (!placesModel) {
            driveRows = []
            return
        }

        const rows = []
        for (let row = 0; row < placesModel.rowCount(); ++row) {
            const index = placesModel.index(row, 0)
            if (!placesModel.data(index, Qt.UserRole + 4)
                    || placesModel.data(index, Qt.UserRole + 23)) {
                continue
            }
            const total = Number(placesModel.data(index, Qt.UserRole + 5) || 0)
            const free = Number(placesModel.data(index, Qt.UserRole + 6) || 0)
            rows.push({
                name: String(placesModel.data(index, Qt.UserRole + 1) || "Drive"),
                mountPoint: String(placesModel.data(index, Qt.UserRole + 2) || ""),
                fileSystem: String(placesModel.data(index, Qt.UserRole + 9) || ""),
                totalBytes: total,
                freeBytes: free,
                usage: total > 0 ? Math.max(0, Math.min(1, (total - free) / total)) : 0,
                critical: !!placesModel.data(index, Qt.UserRole + 12)
            })
        }
        driveRows = rows
    }

    Component.onCompleted: rebuildDrives()
    onPlacesModelChanged: rebuildDrives()

    Connections {
        target: root.placesModel
        function onModelReset() { root.rebuildDrives() }
        function onDataChanged() { root.rebuildDrives() }
        function onRowsInserted() { root.rebuildDrives() }
        function onRowsRemoved() { root.rebuildDrives() }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.compact ? 8 : 18
        radius: Theme.panelRadius
        color: Theme.withAlpha(root.accent, themeController.isDark ? 0.12 : 0.08)
        border.color: Theme.withAlpha(root.accent, themeController.isDark ? 0.32 : 0.22)
        border.width: 1
        clip: true

        Rectangle {
            width: parent.width * 0.75
            height: width
            radius: width / 2
            x: parent.width * 0.55
            y: -height * 0.46
            color: Theme.withAlpha(root.accent, themeController.isDark ? 0.15 : 0.10)
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.compact ? 12 : 22
            spacing: root.compact ? 7 : 12

            RowLayout {
                Layout.fillWidth: true
                spacing: root.compact ? 10 : 14

                IconTile {
                    Layout.preferredWidth: root.compact ? 44 : 62
                    Layout.preferredHeight: width
                    tileSize: width
                    iconSize: root.compact ? 23 : 31
                    cornerRadius: Theme.radiusLg
                    source: "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
                    iconColor: root.accent
                    tileColor: Theme.withAlpha(root.accent, themeController.isDark ? 0.20 : 0.14)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        Layout.fillWidth: true
                        text: "Devices and Drives"
                        font.pixelSize: root.compact ? Theme.fontSizeBody : Theme.scaledSize(23)
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.mountedCount === 0 ? "No mounted volumes"
                              : root.mountedCount + (root.mountedCount === 1 ? " mounted volume" : " mounted volumes")
                        font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeCaption
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                }

                InlineBadge {
                    text: root.mountedCount + " online"
                    fillColor: Theme.withAlpha(root.accent, themeController.isDark ? 0.18 : 0.12)
                    strokeColor: "transparent"
                    textColor: root.accent
                    horizontalPadding: 8
                    badgeHeight: 19
                    fontSize: 9
                    fontWeight: Font.Bold
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.totalBytes > 0

                Label {
                    text: root.formatBytes(root.freeBytes) + " free"
                    font.pixelSize: root.compact ? Theme.fontSizeCaption : Theme.fontSizeBody
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: "of " + root.formatBytes(root.totalBytes)
                    font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeCaption
                    color: Theme.textSecondary
                }
            }

            Repeater {
                model: root.driveRows

                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: modelData.name
                            font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeCaption
                            font.weight: Font.DemiBold
                            color: modelData.critical ? Theme.danger : Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        Label {
                            text: modelData.fileSystem.toUpperCase()
                            visible: text.length > 0
                            font.pixelSize: Theme.fontSizeMicro
                            color: root.accent
                        }
                        Label {
                            text: root.formatBytes(modelData.freeBytes)
                            font.pixelSize: Theme.fontSizeMicro
                            color: Theme.textSecondary
                        }
                    }

                    LinearProgress {
                        Layout.fillWidth: true
                        value: modelData.usage
                        trackColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.52 : 0.58)
                        fillColor: modelData.critical ? Theme.danger : root.accent
                        preserveMinimumFill: true
                    }
                }
            }

        }
    }
}
