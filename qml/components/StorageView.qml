import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"

Item {
    id: root

    required property var controller
    property var panel: null
    property int currentDriveIndex: -1
    property int currentFolderIndex: -1
    property var driveIndexes: []
    property var folderIndexes: []

    readonly property int nameRole: Qt.UserRole + 1
    readonly property int pathRole: Qt.UserRole + 2
    readonly property int iconRole: Qt.UserRole + 3
    readonly property int isDriveRole: Qt.UserRole + 4
    readonly property int totalSpaceRole: Qt.UserRole + 5
    readonly property int freeSpaceRole: Qt.UserRole + 6
    readonly property int usagePercentRole: Qt.UserRole + 8
    readonly property int fileSystemRole: Qt.UserRole + 9
    readonly property int driveTypeRole: Qt.UserRole + 10
    readonly property int isReadyRole: Qt.UserRole + 11
    readonly property int isCriticalRole: Qt.UserRole + 12
    readonly property int canEjectRole: Qt.UserRole + 14
    readonly property int sourcePathRole: Qt.UserRole + 15

    function getDriveIndexes() {
        let indexes = []
        let m = workspaceController.placesModel
        for (let i = 0; i < m.rowCount(); i++) {
            if (m.data(m.index(i, 0), root.isDriveRole)) {
                indexes.push(i)
            }
        }
        return indexes
    }

    function getFolderIndexes() {
        let indexes = []
        let m = workspaceController.placesModel
        for (let i = 0; i < m.rowCount(); i++) {
            if (!m.data(m.index(i, 0), root.isDriveRole)) {
                indexes.push(i)
            }
        }
        return indexes
    }

    function modelValue(row, role, fallback) {
        let m = workspaceController.placesModel
        if (row < 0 || row >= m.rowCount()) return fallback
        let value = m.data(m.index(row, 0), role)
        return value === undefined || value === null ? fallback : value
    }

    function refreshIndexSnapshots() {
        root.driveIndexes = getDriveIndexes()
        root.folderIndexes = getFolderIndexes()
        if (root.currentDriveIndex >= 0 && root.driveIndexes.indexOf(root.currentDriveIndex) < 0) {
            root.currentDriveIndex = -1
        }
        if (root.currentFolderIndex >= 0 && root.folderIndexes.indexOf(root.currentFolderIndex) < 0) {
            root.currentFolderIndex = -1
        }
    }

    function clearUnmountedIsoState(rootPath) {
        if (!rootPath) return
        if (driveContextMenu.drivePath === rootPath) {
            driveContextMenu.close()
            driveContextMenu.reset()
        }
        if (quickLookController.path
                && quickLookController.path.toLowerCase().indexOf(rootPath.toLowerCase()) === 0) {
            quickLookController.preview("devices://")
        }
        root.currentDriveIndex = -1
        root.refreshIndexSnapshots()
    }

    Component.onCompleted: refreshIndexSnapshots()

    Connections {
        target: workspaceController.placesModel
        function onModelReset() { root.refreshIndexSnapshots() }
        function onRowsInserted() { root.refreshIndexSnapshots() }
        function onRowsRemoved() { root.refreshIndexSnapshots() }
        function onDataChanged() { root.refreshIndexSnapshots() }
    }

    Connections {
        target: workspaceController.isoMountManager
        function onUnmountStarted(rootPath) {
            root.clearUnmountedIsoState(rootPath)
        }
        function onUnmountFinished(rootPath, success, error) {
            root.refreshIndexSnapshots()
            if (success) {
                root.clearUnmountedIsoState(rootPath)
            }
        }
    }

    // ── Helper functions ──────────────────────────────────────────────────────

    function driveIconSource(driveType) {
        // All icons are mapped to available assets
        switch (String(driveType)) {
        case "usb":     return "qrc:/qt/qml/FM/qml/assets/icons/hard-drive.svg"
        case "optical": return "qrc:/qt/qml/FM/qml/assets/icons/hard-drive.svg"
        case "network": return "qrc:/qt/qml/FM/qml/assets/icons/hard-drive.svg"
        default:        return "qrc:/qt/qml/FM/qml/assets/icons/hard-drive.svg"
        }
    }

    function driveIconColor(driveType) {
        switch (String(driveType)) {
        case "usb":     return "#22c55e"
        case "optical": return "#f59e0b"
        case "network": return "#8b5cf6"
        case "iso":     return "#14b8a6"
        case "ssd":     return "#06b6d4"
        default:        return "#3b82f6"
        }
    }

    function driveTypeLabel(driveType) {
        switch (String(driveType)) {
        case "usb":     return "USB"
        case "optical": return "Optical"
        case "network": return "Network"
        case "iso":     return "ISO"
        case "ssd":     return "SSD"
        default:        return "HDD"
        }
    }

    function progressColor(percent, isCritical) {
        if (isCritical || percent > 0.90) return "#ef4444"
        if (percent > 0.75)              return "#f59e0b"
        return Theme.accent
    }

    function formatBytes(bytes) {
        if (bytes <= 0) return "—"
        var tb = 1024 * 1024 * 1024 * 1024
        var gb = 1024 * 1024 * 1024
        var mb = 1024 * 1024
        if (bytes >= tb) return (bytes / tb).toFixed(2) + " TB"
        if (bytes >= gb) return (bytes / gb).toFixed(1) + " GB"
        if (bytes >= mb) return Math.round(bytes / mb) + " MB"
        return Math.round(bytes / 1024) + " KB"
    }

    function folderIconSource(iconName) {
        if (!iconName || iconName === "drive") return ""
        return "qrc:/qt/qml/FM/qml/assets/icons/" + iconName + ".svg"
    }

    function folderIconColor(iconName) {
        switch (iconName) {
        case "home":     return "#3b82f6" // blue
        case "desktop":  return "#6366f1" // indigo
        case "download": return "#10b981" // emerald green
        case "document": return "#06b6d4" // cyan
        case "image":    return "#d946ef" // fuchsia
        case "music":    return "#f59e0b" // amber
        case "video":    return "#ef4444" // rose/red
        default:         return Theme.accent
        }
    }

    // ── Summary stats ──────────────────────────────────────────────────────────

    readonly property real totalSpaceSum: {
        var sum = 0
        var m = workspaceController.placesModel
        for (var i = 0; i < m.rowCount(); i++) {
            if (m.data(m.index(i, 0), root.isDriveRole)) {
                sum += m.data(m.index(i, 0), root.totalSpaceRole)
            }
        }
        return sum
    }

    readonly property real freeSpaceSum: {
        var sum = 0
        var m = workspaceController.placesModel
        for (var i = 0; i < m.rowCount(); i++) {
            if (m.data(m.index(i, 0), root.isDriveRole)) {
                sum += m.data(m.index(i, 0), root.freeSpaceRole)
            }
        }
        return sum
    }

    // Dynamic layout spacing to fill larger window heights
    readonly property real baseContentHeight: 356 + flowLayout.implicitHeight + quickAccessFlow.implicitHeight
    readonly property real extraHeight: Math.max(0, root.height - baseContentHeight)
    readonly property real gapAmount: Math.min(120, extraHeight / 3)

    // ── Premium Ambient Background ────────────────────────────────────────────

    Item {
        anchors.fill: parent
        z: -1

        // Soft linear background gradient
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: themeController.isDark
                        ? Theme.withAlpha(Theme.accent, 0.10)
                        : Theme.withAlpha(Theme.secondaryAccent, 0.08)
                }
                GradientStop {
                    position: 1.0
                    color: themeController.isDark
                        ? Theme.withAlpha(Theme.panelSurface, 0.94)
                        : Theme.withAlpha(Theme.panelSurface, 0.98)
                }
            }
        }

        // Ambient glow blobs
        Rectangle {
            width: parent.width * 0.5
            height: width
            radius: width / 2
            x: -parent.width * 0.1
            y: -parent.height * 0.1
            color: Theme.accent
            opacity: themeController.isDark ? 0.07 : 0.04
            layer.enabled: true
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: 150
            }
        }

        Rectangle {
            width: parent.width * 0.45
            height: width
            radius: width / 2
            x: parent.width * 0.65
            y: parent.height * 0.5
            color: "#8b5cf6" // Purple
            opacity: themeController.isDark ? 0.05 : 0.03
            layer.enabled: true
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: 130
            }
        }
    }

    // ── Content Area ──────────────────────────────────────────────────────────

    Flickable {
        id: mainFlickable
        anchors.fill: parent
        contentHeight: mainLayout.implicitHeight + 32
        clip: true

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        MouseArea {
            anchors.fill: parent
            z: -1
            onPressed: {
                if (root.panel) root.panel.activated()
                root.forceActiveFocus()
            }
        }

        ColumnLayout {
            id: mainLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0

            // ── Section Title Header ──────────────────────────────────────────
            Item {
                Layout.fillWidth: true
                implicitHeight: 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 10

                    Image {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        source: "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
                        sourceSize: Qt.size(20, 20)
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            colorization: 1.0
                            colorizationColor: "#6366f1"
                        }
                    }

                    Label {
                        text: "System Information"
                        font.pixelSize: 16
                        font.bold: true
                        color: Theme.textPrimary
                    }

                    Item { Layout.fillWidth: true }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.panelBorder
                    opacity: 0.35
                }
            }

            // ── Premium Dashboard Card ────────────────────────────────────────
            SurfaceCard {
                id: dashboardCard
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 16
                Layout.bottomMargin: 20 + root.gapAmount
                height: 132
                cornerRadius: Theme.radiusLg
                surfaceColor: themeController.isDark
                    ? Theme.withAlpha(Theme.panelSurface, 0.78)
                    : Theme.withAlpha(Theme.panelSurface, 0.92)
                strokeColor: Theme.panelBorder

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, themeController.isDark ? 0.20 : 0.06)
                    shadowBlur: 0.8
                    shadowVerticalOffset: 3
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 0

                    // Left Column (System Info)
                    ColumnLayout {
                        spacing: 4
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: 260

                        RowLayout {
                            spacing: 8
                            Label {
                                text: systemInfoProvider.computerName
                                font.pixelSize: 15
                                font.bold: true
                                color: Theme.textPrimary
                            }

                            InlineBadge {
                                text: systemInfoProvider.osName
                                fillColor: Theme.withAlpha(Theme.accent, 0.14)
                                strokeColor: "transparent"
                                textColor: Theme.accent
                                horizontalPadding: 12
                                badgeHeight: 18
                                fontSize: 9
                                fontWeight: Font.Bold
                            }
                        }

                        // CPU Model Name
                        Label {
                            text: systemInfoProvider.cpuName || "Detecting CPU..."
                            font.pixelSize: 11
                            font.bold: true
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 6
                            Label {
                                text: systemInfoProvider.cpuCores + " Cores (" + systemInfoProvider.cpuArchitecture + ")"
                                font.pixelSize: 10
                                color: Theme.textSecondary
                                opacity: 0.75
                            }
                            Rectangle {
                                width: 3
                                height: 3
                                radius: 1.5
                                color: Theme.textSecondary
                                opacity: 0.5
                            }
                            Label {
                                text: "Uptime: " + systemInfoProvider.uptime
                                font.pixelSize: 10
                                color: Theme.textSecondary
                                opacity: 0.75
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Center Column (Gauges)
                    RowLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 24

                        // RAM Gauge
                        ColumnLayout {
                            spacing: 4
                            Layout.alignment: Qt.AlignHCenter

                            Item {
                                width: 56
                                height: 56

                                Canvas {
                                    id: ramCanvas
                                    anchors.fill: parent
                                    property real val: systemInfoProvider.ramUsage
                                    onValChanged: requestPaint()
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.clearRect(0, 0, width, height);

                                        // Track
                                        ctx.beginPath();
                                        ctx.arc(width/2, height/2, width/2 - 4, 0, 2*Math.PI);
                                        ctx.lineWidth = 4;
                                        ctx.strokeStyle = themeController.isDark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(0,0,0,0.05);
                                        ctx.stroke();

                                        // Active
                                        ctx.beginPath();
                                        ctx.arc(width/2, height/2, width/2 - 4, -Math.PI/2, -Math.PI/2 + val * 2*Math.PI);
                                        ctx.lineWidth = 4;
                                        ctx.strokeStyle = "#8b5cf6"; // Purple
                                        ctx.lineCap = "round";
                                        ctx.stroke();
                                    }
                                }

                                Label {
                                    anchors.centerIn: parent
                                    text: Math.round(systemInfoProvider.ramUsage * 100) + "%"
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: Theme.textPrimary
                                }
                            }

                            Label {
                                text: systemInfoProvider.usedRamGB.toFixed(1) + " / " + systemInfoProvider.totalRamGB.toFixed(0) + " GB"
                                font.pixelSize: 9
                                font.bold: true
                                color: Theme.textSecondary
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }

                        // CPU Gauge
                        ColumnLayout {
                            spacing: 4
                            Layout.alignment: Qt.AlignHCenter

                            Item {
                                width: 56
                                height: 56

                                Canvas {
                                    id: cpuCanvas
                                    anchors.fill: parent
                                    property real val: systemInfoProvider.cpuUsage
                                    onValChanged: requestPaint()
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.clearRect(0, 0, width, height);

                                        // Track
                                        ctx.beginPath();
                                        ctx.arc(width/2, height/2, width/2 - 4, 0, 2*Math.PI);
                                        ctx.lineWidth = 4;
                                        ctx.strokeStyle = themeController.isDark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(0,0,0,0.05);
                                        ctx.stroke();

                                        // Active
                                        ctx.beginPath();
                                        ctx.arc(width/2, height/2, width/2 - 4, -Math.PI/2, -Math.PI/2 + val * 2*Math.PI);
                                        ctx.lineWidth = 4;
                                        ctx.strokeStyle = "#0ea5e9"; // Blue/cyan
                                        ctx.lineCap = "round";
                                        ctx.stroke();
                                    }
                                }

                                Label {
                                    anchors.centerIn: parent
                                    text: Math.round(systemInfoProvider.cpuUsage * 100) + "%"
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: Theme.textPrimary
                                }
                            }

                            Label {
                                text: "CPU Load"
                                font.pixelSize: 9
                                font.bold: true
                                color: Theme.textSecondary
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Right Column (Storage Overview)
                    ColumnLayout {
                        spacing: 4
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: 180

                        Label {
                            text: "Unified Drive Usage"
                            font.pixelSize: 11
                            font.bold: true
                            color: Theme.textPrimary
                        }

                        LinearProgress {
                            Layout.fillWidth: true
                            readonly property real usage: root.totalSpaceSum > 0 ? (root.totalSpaceSum - root.freeSpaceSum) / root.totalSpaceSum : 0.0
                            value: usage
                            trackColor: themeController.isDark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(0,0,0,0.05)
                            fillColor: usage > 0.90 ? "#ef4444" : (usage > 0.75 ? "#f59e0b" : Theme.accent)
                        }

                        Label {
                            text: root.formatBytes(root.totalSpaceSum - root.freeSpaceSum) + " used of " + root.formatBytes(root.totalSpaceSum)
                            font.pixelSize: 9
                            color: Theme.textSecondary
                            opacity: 0.8
                        }
                    }
                }
            }

            // ── Drives Section Header ─────────────────────────────────────────
            Item {
                Layout.fillWidth: true
                implicitHeight: 32

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 8

                    Rectangle {
                        width: 4
                        height: 14
                        radius: 2
                        color: Theme.accent
                    }

                    Label {
                        text: "Devices and Drives"
                        font.pixelSize: 13
                        font.bold: true
                        color: Theme.textPrimary
                    }
                }
            }

            // ── Drives Flow Layout ────────────────────────────────────────────
            Flow {
                id: flowLayout
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                Layout.bottomMargin: 16 + root.gapAmount
                spacing: 12

                readonly property int minCardW: 280
                readonly property int cols: Math.max(1, Math.floor((width + spacing) / (minCardW + spacing)))
                readonly property real cardW: Math.floor((width - (cols - 1) * spacing) / cols)

                Repeater {
                    id: drivesRepeater
                    model: root.driveIndexes
                    delegate: Item {
                        id: cardWrapper
                        readonly property int sourceIndex: modelData
                        readonly property string drivePath: root.modelValue(sourceIndex, root.pathRole, "")
                        readonly property string driveType: root.modelValue(sourceIndex, root.driveTypeRole, "")
                        readonly property bool isReady: root.modelValue(sourceIndex, root.isReadyRole, false)
                        readonly property bool isCritical: root.modelValue(sourceIndex, root.isCriticalRole, false)
                        readonly property bool canEject: root.modelValue(sourceIndex, root.canEjectRole, false)
                        readonly property real usagePercent: root.modelValue(sourceIndex, root.usagePercentRole, 0)
                        readonly property real freeSpace: root.modelValue(sourceIndex, root.freeSpaceRole, 0)
                        readonly property real totalSpace: root.modelValue(sourceIndex, root.totalSpaceRole, 0)
                        readonly property string driveName: root.modelValue(sourceIndex, root.nameRole, "")
                        readonly property string fileSystem: root.modelValue(sourceIndex, root.fileSystemRole, "")
                        width: flowLayout.cardW
                        height: 108
                        visible: true

                        Rectangle {
                            id: card
                            x: 0
                            y: cardMouse.containsMouse ? -2 : 0
                            width: parent.width
                            height: parent.height
                            radius: Theme.radiusMd
                            scale: cardMouse.containsMouse ? 1.02 : (cardWrapper.isSelected ? 1.01 : 1.0)

                            color: {
                                if (themeController.isDark) {
                                    if (cardMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.86)
                                    return Theme.withAlpha(Theme.panelSurface, 0.62)
                                } else {
                                    if (cardMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.94)
                                    return Theme.withAlpha(Theme.panelSurface, 0.74)
                                }
                            }

                            border.color: {
                                if (cardWrapper.isSelected) {
                                    return Theme.accent
                                }
                                if (cardMouse.containsMouse) {
                                    return themeController.isDark
                                        ? Theme.withAlpha(Theme.accent, 0.46)
                                        : Theme.withAlpha(Theme.accent, 0.36)
                                }
                                return Theme.panelBorder
                            }
                            border.width: cardWrapper.isSelected ? 1.5 : 1

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: themeController.isDark
                                    ? Qt.rgba(0, 0, 0, cardMouse.containsMouse ? 0.34 : (cardWrapper.isSelected ? 0.30 : 0.12))
                                    : Qt.rgba(0, 0, 0, cardMouse.containsMouse ? 0.10 : (cardWrapper.isSelected ? 0.08 : 0.03))
                                shadowBlur: cardMouse.containsMouse ? 0.8 : (cardWrapper.isSelected ? 0.7 : 0.4)
                                shadowVerticalOffset: cardMouse.containsMouse ? 5 : (cardWrapper.isSelected ? 3 : 2)
                                shadowHorizontalOffset: 0
                            }

                            Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                            Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
                            Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                            Behavior on y { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 14

                                // Drive icon column
                                Item {
                                    Layout.preferredWidth: 48
                                    Layout.alignment: Qt.AlignVCenter

                                    IconTile {
                                        anchors.centerIn: parent
                                        tileSize: 44
                                        iconSize: 24
                                        cornerRadius: Theme.radiusMd
                                        source: root.driveIconSource(cardWrapper.driveType)
                                        iconColor: root.driveIconColor(cardWrapper.driveType)
                                        tileColor: Theme.withAlpha(
                                            Qt.color(root.driveIconColor(cardWrapper.driveType)),
                                            (themeController.isDark ? 0.18 : 0.12) + (cardMouse.containsMouse ? 0.08 : 0))

                                        Behavior on tileColor { ColorAnimation { duration: Theme.motionFast } }
                                    }
                                }

                                // Info column
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 5

                                    // Drive name + FS badge row
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Label {
                                            text: cardWrapper.driveName || cardWrapper.drivePath
                                            font.pixelSize: 13
                                            font.bold: true
                                            color: Theme.textPrimary
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        // FS badge
                                        InlineBadge {
                                            visible: cardWrapper.fileSystem && cardWrapper.fileSystem.length > 0
                                            text: cardWrapper.fileSystem || ""
                                            fillColor: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.18 : 0.12)
                                            strokeColor: "transparent"
                                            textColor: Theme.accent
                                            horizontalPadding: 8
                                            badgeHeight: 17
                                            fontSize: 9
                                            fontWeight: Font.Bold
                                            letterSpacing: 0.5
                                        }
                                    }

                                    // Free space text
                                    Label {
                                        text: cardWrapper.isReady
                                            ? (root.formatBytes(cardWrapper.freeSpace) + " free of " + root.formatBytes(cardWrapper.totalSpace))
                                            : "Not ready"
                                        font.pixelSize: 11
                                        color: cardWrapper.isCritical ? "#ef4444" : Theme.textSecondary
                                        opacity: 0.88
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // Progress bar
                                    LinearProgress {
                                        Layout.fillWidth: true
                                        value: cardWrapper.isReady ? cardWrapper.usagePercent : 0
                                        trackColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.55)
                                        fillColor: root.progressColor(cardWrapper.usagePercent, cardWrapper.isCritical)
                                        preserveMinimumFill: true
                                    }

                                    // Drive type tag + percent row
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Label {
                                            text: root.driveTypeLabel(cardWrapper.driveType)
                                            font.pixelSize: 10
                                            font.bold: true
                                            font.letterSpacing: 0.8
                                            color: root.driveIconColor(cardWrapper.driveType)
                                            opacity: 0.82
                                        }

                                        Item { Layout.fillWidth: true }

                                        // Warning icon for critical
                                        Label {
                                            text: "⚠"
                                            font.pixelSize: 11
                                            color: "#ef4444"
                                            visible: cardWrapper.isCritical
                                        }

                                        Label {
                                            text: cardWrapper.isReady
                                                ? (Math.round(cardWrapper.usagePercent * 100) + "% used")
                                                : "—"
                                            font.pixelSize: 10
                                            color: cardWrapper.isCritical ? "#ef4444" : Theme.textSecondary
                                            opacity: 0.75
                                        }
                                    }
                                }
                            }

                            // Mouse interaction
                            MouseArea {
                                id: cardMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                cursorShape: Qt.PointingHandCursor

                                onClicked: function(mouse) {
                                    if (root.panel) root.panel.activated()
                                    root.forceActiveFocus()
                                    root.currentDriveIndex = cardWrapper.sourceIndex
                                    root.currentFolderIndex = -1
                                    if (mouse.button === Qt.RightButton) {
                                        driveContextMenu.driveIndex = cardWrapper.sourceIndex
                                        driveContextMenu.drivePath  = cardWrapper.drivePath
                                        driveContextMenu.driveType  = cardWrapper.driveType
                                        driveContextMenu.canEject = cardWrapper.canEject
                                        driveContextMenu.managedIsoMount = workspaceController.isManagedIsoMountRoot(cardWrapper.drivePath)
                                        driveContextMenu.popup()
                                    } else {
                                        quickLookController.preview(cardWrapper.drivePath)
                                    }
                                }

                                onDoubleClicked: function(mouse) {
                                    if (!cardWrapper.isReady) return
                                    root.controller.openPath(cardWrapper.drivePath)
                                }
                            }
                        }

                        // Card appear animation
                        opacity: 0
                        Component.onCompleted: {
                            appearAnim.start()
                        }

                        NumberAnimation {
                            id: appearAnim
                            target: cardWrapper
                            property: "opacity"
                            from: 0; to: 1
                            duration: 250 + (index % 6) * 40
                            easing.type: Easing.OutCubic
                        }

                        property bool isSelected: root.currentDriveIndex === sourceIndex
                    } // end delegate
                } // end Repeater
            } // end Flow

            // ── Quick Access Section Header ───────────────────────────────────
            Item {
                Layout.fillWidth: true
                implicitHeight: 32

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 8

                    Rectangle {
                        width: 4
                        height: 14
                        radius: Theme.radiusSm
                        color: Theme.withAlpha(Theme.accent, 0.92)
                    }

                    Label {
                        text: "Quick Access"
                        font.pixelSize: 13
                        font.bold: true
                        color: Theme.textPrimary
                    }
                }
            }

            // ── Quick Access Flow Layout ──────────────────────────────────────
            Flow {
                id: quickAccessFlow
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                Layout.bottomMargin: 16 + root.gapAmount
                spacing: 12

                readonly property int minCardW: 180
                readonly property int cols: Math.max(1, Math.floor((width + spacing) / (minCardW + spacing)))
                readonly property real cardW: Math.floor((width - (cols - 1) * spacing) / cols)

                Repeater {
                    id: foldersRepeater
                    model: root.folderIndexes
                    delegate: Item {
                        id: folderCardWrapper
                        readonly property int sourceIndex: modelData
                        readonly property string folderPath: root.modelValue(sourceIndex, root.pathRole, "")
                        readonly property string folderName: root.modelValue(sourceIndex, root.nameRole, "")
                        readonly property string folderIcon: root.modelValue(sourceIndex, root.iconRole, "")
                        width: quickAccessFlow.cardW
                        height: 68
                        visible: true
                        property bool isSelected: root.currentFolderIndex === sourceIndex

                        Rectangle {
                            id: folderCard
                            x: 0
                            y: folderMouse.containsMouse ? -2 : (folderCardWrapper.isSelected ? -1 : 0)
                            width: parent.width
                            height: parent.height
                            radius: Theme.radiusSm
                            scale: folderMouse.containsMouse ? 1.02 : (folderCardWrapper.isSelected ? 1.01 : 1.0)

                            color: {
                                if (folderCardWrapper.isSelected) {
                                    return themeController.isDark
                                        ? Theme.withAlpha(Theme.panelSurface, 0.90)
                                        : Theme.withAlpha(Theme.panelSurface, 0.97)
                                }
                                if (themeController.isDark) {
                                    if (folderMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.84)
                                    return Theme.withAlpha(Theme.panelSurface, 0.62)
                                } else {
                                    if (folderMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.92)
                                    return Theme.withAlpha(Theme.panelSurface, 0.74)
                                }
                            }

                            border.color: folderCardWrapper.isSelected
                                ? Theme.accent
                                : (folderMouse.containsMouse
                                    ? (themeController.isDark ? Theme.withAlpha(Theme.accent, 0.46) : Theme.withAlpha(Theme.accent, 0.36))
                                    : Theme.panelBorder)
                            border.width: folderCardWrapper.isSelected ? 1.5 : 1

                            Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                            Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
                            Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                            Behavior on y { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: themeController.isDark
                                    ? Qt.rgba(0, 0, 0, (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 0.32 : 0.10)
                                    : Qt.rgba(0, 0, 0, (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 0.08 : 0.02)
                                shadowBlur: (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 0.6 : 0.3
                                shadowVerticalOffset: (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 4 : 2
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10

                                IconTile {
                                    tileSize: 32
                                    iconSize: 16
                                    cornerRadius: Theme.radiusSm
                                    source: root.folderIconSource(folderCardWrapper.folderIcon)
                                    iconColor: root.folderIconColor(folderCardWrapper.folderIcon)
                                    tileColor: Theme.withAlpha(
                                        Qt.color(root.folderIconColor(folderCardWrapper.folderIcon)),
                                        (themeController.isDark ? 0.15 : 0.10) + ((folderMouse.containsMouse || folderCardWrapper.isSelected) ? 0.10 : 0))

                                    Behavior on tileColor { ColorAnimation { duration: Theme.motionFast } }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Label {
                                        text: folderCardWrapper.folderName
                                        font.pixelSize: 12
                                        font.bold: true
                                        color: Theme.textPrimary
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: "System Folder"
                                        font.pixelSize: 10
                                        color: Theme.textSecondary
                                        opacity: 0.6
                                    }
                                }
                            }

                            MouseArea {
                                id: folderMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton | Qt.RightButton

                                onClicked: function(mouse) {
                                    if (root.panel) root.panel.activated()
                                    root.forceActiveFocus()
                                    if (mouse.button === Qt.RightButton) return
                                    root.currentDriveIndex = -1
                                    root.currentFolderIndex = folderCardWrapper.sourceIndex
                                    quickLookController.preview(folderCardWrapper.folderPath)
                                }

                                onDoubleClicked: function(mouse) {
                                    root.controller.openPath(folderCardWrapper.folderPath)
                                }
                            }
                        }

                        // Staggered fade-in/slide-up animation
                        opacity: 0
                        y: 10
                        Component.onCompleted: {
                            folderAppearAnim.start()
                        }

                        ParallelAnimation {
                            id: folderAppearAnim
                            NumberAnimation {
                                target: folderCardWrapper
                                property: "opacity"
                                from: 0; to: 1
                                duration: 300 + (index % 6) * 40
                                easing.type: Easing.OutCubic
                            }
                            NumberAnimation {
                                target: folderCardWrapper
                                property: "y"
                                from: 10; to: 0
                                duration: 350 + (index % 6) * 40
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Drive context menu ────────────────────────────────────────────────────

    DriveContextMenu {
        id: driveContextMenu

        onOpenRequested: function(path) {
            root.controller.openPath(path)
        }

        onEjectRequested: function(path, managedIsoMount) {
            if (managedIsoMount) {
                workspaceController.unmountIsoRoot(path)
            } else {
                root.controller.ejectDrive(path)
            }
        }

        onPropertiesRequested: function(path) {
            propertiesController.load(path)
        }
    }

    // ── Keyboard navigation ───────────────────────────────────────────────────

    Keys.onPressed: function(event) {
        if (root.Window.window && root.Window.window.anyOverlayOpen) {
            event.accepted = true
            return
        }

        let drives = getDriveIndexes()
        let folders = getFolderIndexes()

        if (drives.length === 0 && folders.length === 0) return

        let isDriveSelected = (root.currentDriveIndex >= 0)
        let isFolderSelected = (root.currentFolderIndex >= 0)

        // Initial selection if none
        if (!isDriveSelected && !isFolderSelected) {
            if (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                if (drives.length > 0) {
                    root.currentDriveIndex = drives[0]
                    quickLookController.preview(workspaceController.placesModel.data(workspaceController.placesModel.index(root.currentDriveIndex, 0), Qt.UserRole + 2))
                } else if (folders.length > 0) {
                    root.currentFolderIndex = folders[0]
                    quickLookController.preview(workspaceController.placesModel.data(workspaceController.placesModel.index(root.currentFolderIndex, 0), Qt.UserRole + 2))
                }
                event.accepted = true
                return
            }
        }

        let m = workspaceController.placesModel

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (isDriveSelected) {
                let path = m.data(m.index(root.currentDriveIndex, 0), Qt.UserRole + 2)
                if (path) root.controller.openPath(path)
            } else if (isFolderSelected) {
                let folderPath = m.data(m.index(root.currentFolderIndex, 0), Qt.UserRole + 2)
                if (folderPath) root.controller.openPath(folderPath)
            }
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Right) {
            if (isDriveSelected) {
                let idx = drives.indexOf(root.currentDriveIndex)
                if (idx >= 0 && idx < drives.length - 1) {
                    root.currentDriveIndex = drives[idx + 1]
                    quickLookController.preview(m.data(m.index(root.currentDriveIndex, 0), Qt.UserRole + 2))
                }
            } else if (isFolderSelected) {
                let idx = folders.indexOf(root.currentFolderIndex)
                if (idx >= 0 && idx < folders.length - 1) {
                    root.currentFolderIndex = folders[idx + 1]
                    quickLookController.preview(m.data(m.index(root.currentFolderIndex, 0), Qt.UserRole + 2))
                }
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            if (isDriveSelected) {
                let idx = drives.indexOf(root.currentDriveIndex)
                if (idx > 0) {
                    root.currentDriveIndex = drives[idx - 1]
                    quickLookController.preview(m.data(m.index(root.currentDriveIndex, 0), Qt.UserRole + 2))
                }
            } else if (isFolderSelected) {
                let idx = folders.indexOf(root.currentFolderIndex)
                if (idx > 0) {
                    root.currentFolderIndex = folders[idx - 1]
                    quickLookController.preview(m.data(m.index(root.currentFolderIndex, 0), Qt.UserRole + 2))
                }
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            if (isDriveSelected) {
                let idx = drives.indexOf(root.currentDriveIndex)
                let cols = flowLayout.cols
                if (idx >= 0 && idx + cols < drives.length) {
                    root.currentDriveIndex = drives[idx + cols]
                    quickLookController.preview(m.data(m.index(root.currentDriveIndex, 0), Qt.UserRole + 2))
                } else if (folders.length > 0) {
                    // Jump to folders
                    root.currentDriveIndex = -1
                    root.currentFolderIndex = folders[0]
                    quickLookController.preview(m.data(m.index(root.currentFolderIndex, 0), Qt.UserRole + 2))
                }
            } else if (isFolderSelected) {
                let idx = folders.indexOf(root.currentFolderIndex)
                let cols = quickAccessFlow.cols
                if (idx >= 0 && idx + cols < folders.length) {
                    root.currentFolderIndex = folders[idx + cols]
                    quickLookController.preview(m.data(m.index(root.currentFolderIndex, 0), Qt.UserRole + 2))
                }
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            if (isDriveSelected) {
                let idx = drives.indexOf(root.currentDriveIndex)
                let cols = flowLayout.cols
                if (idx - cols >= 0) {
                    root.currentDriveIndex = drives[idx - cols]
                    quickLookController.preview(m.data(m.index(root.currentDriveIndex, 0), Qt.UserRole + 2))
                }
            } else if (isFolderSelected) {
                let idx = folders.indexOf(root.currentFolderIndex)
                let cols = quickAccessFlow.cols
                if (idx - cols >= 0) {
                    root.currentFolderIndex = folders[idx - cols]
                    quickLookController.preview(m.data(m.index(root.currentFolderIndex, 0), Qt.UserRole + 2))
                } else if (drives.length > 0) {
                    // Jump to drives (bottom row)
                    root.currentFolderIndex = -1
                    root.currentDriveIndex = drives[drives.length - 1]
                    quickLookController.preview(m.data(m.index(root.currentDriveIndex, 0), Qt.UserRole + 2))
                }
            }
            event.accepted = true
        }
    }

    function ensureVisible(item) {
        if (!item) return
        var itemY = item.mapToItem(mainLayout, 0, 0).y
        var itemHeight = item.height
        
        var viewportHeight = mainFlickable.height
        var currentScrollY = mainFlickable.contentY
        
        if (itemY < currentScrollY) {
            mainFlickable.contentY = Math.max(0, itemY - 10)
        } else if (itemY + itemHeight > currentScrollY + viewportHeight) {
            mainFlickable.contentY = Math.min(mainFlickable.contentHeight - viewportHeight, itemY + itemHeight - viewportHeight + 10)
        }
    }

    onCurrentDriveIndexChanged: {
        if (currentDriveIndex >= 0 && drivesRepeater) {
            Qt.callLater(() => {
                var item = drivesRepeater.itemAt(root.driveIndexes.indexOf(currentDriveIndex))
                if (item) ensureVisible(item)
            })
        }
    }

    onCurrentFolderIndexChanged: {
        if (currentFolderIndex >= 0 && foldersRepeater) {
            Qt.callLater(() => {
                var item = foldersRepeater.itemAt(root.folderIndexes.indexOf(currentFolderIndex))
                if (item) ensureVisible(item)
            })
        }
    }
}
