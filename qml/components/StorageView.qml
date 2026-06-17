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
    property bool liveResizeActive: false
    property int currentDriveIndex: -1
    property int currentPortableIndex: -1
    property int currentFolderIndex: -1
    property string currentDrivePath: ""
    property string currentPortablePath: ""
    property string currentFolderPath: ""
    property var driveIndexes: []
    property var portableIndexes: []
    property var folderIndexes: []
    property int storageRevision: 0

    function refreshPositioners() {
        if (flowLayout && flowLayout.forceLayout) {
            flowLayout.forceLayout()
        }
        if (portableFlow && portableFlow.forceLayout) {
            portableFlow.forceLayout()
        }
        if (quickAccessFlow && quickAccessFlow.forceLayout) {
            quickAccessFlow.forceLayout()
        }
    }

    function schedulePositionerRefresh() {
        if (root.resizeOptimized) {
            return
        }
        relayoutTimer.restart()
    }

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
    readonly property int sectionRole: Qt.UserRole + 17
    readonly property int subtitleRole: Qt.UserRole + 18
    readonly property bool ultraLightMode: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.ultraLightMode
                                           : false
    readonly property bool resizeOptimized: root.liveResizeActive
    readonly property bool effectsReduced: root.resizeOptimized || root.ultraLightMode

    function getDriveIndexes() {
        let indexes = []
        let m = workspaceController.placesModel
        for (let i = 0; i < m.rowCount(); i++) {
            const section = String(m.data(m.index(i, 0), root.sectionRole) || "")
            if (section === "drive" || (section.length === 0 && m.data(m.index(i, 0), root.isDriveRole))) {
                indexes.push(i)
            }
        }
        return indexes
    }

    function getPortableIndexes() {
        let indexes = []
        let m = workspaceController.placesModel
        for (let i = 0; i < m.rowCount(); i++) {
            if (String(m.data(m.index(i, 0), root.sectionRole) || "") === "portable") {
                indexes.push(i)
            }
        }
        return indexes
    }

    function getFolderIndexes() {
        let indexes = []
        let m = workspaceController.placesModel
        for (let i = 0; i < m.rowCount(); i++) {
            const section = String(m.data(m.index(i, 0), root.sectionRole) || "")
            if (section === "place" || (section.length === 0 && !m.data(m.index(i, 0), root.isDriveRole))) {
                indexes.push(i)
            }
        }
        return indexes
    }

    function modelValue(row, role, fallback) {
        root.storageRevision
        let m = workspaceController.placesModel
        if (row < 0 || row >= m.rowCount()) return fallback
        let value = m.data(m.index(row, 0), role)
        return value === undefined || value === null ? fallback : value
    }

    function pathsEqual(lhs, rhs) {
        if (!lhs || !rhs) return false
        return String(lhs).toLowerCase() === String(rhs).toLowerCase()
    }

    function placeIndexForPath(path, requireDrive, requiredSection) {
        if (!path || path.length === 0) return -1
        let m = workspaceController.placesModel
        for (let i = 0; i < m.rowCount(); ++i) {
            if (requiredSection !== undefined) {
                const rowSection = String(m.data(m.index(i, 0), root.sectionRole) || "")
                if (rowSection !== requiredSection) {
                    continue
                }
            }
            if (requireDrive !== undefined && !!m.data(m.index(i, 0), root.isDriveRole) !== requireDrive) {
                continue
            }
            const rowPath = m.data(m.index(i, 0), root.pathRole) || ""
            if (root.pathsEqual(rowPath, path)) {
                return i
            }
        }
        return -1
    }

    function refreshIndexSnapshots() {
        const drivePath = root.currentDrivePath
        const portablePath = root.currentPortablePath
        const folderPath = root.currentFolderPath
        root.driveIndexes = getDriveIndexes()
        root.portableIndexes = getPortableIndexes()
        root.folderIndexes = getFolderIndexes()

        if (drivePath.length > 0) {
            const driveIndex = root.placeIndexForPath(drivePath, true, "drive")
            root.currentDriveIndex = driveIndex
            if (driveIndex < 0) {
                root.currentDrivePath = ""
            }
        } else if (root.currentDriveIndex >= 0 && root.driveIndexes.indexOf(root.currentDriveIndex) < 0) {
            root.currentDriveIndex = -1
        }

        if (portablePath.length > 0) {
            const portableIndex = root.placeIndexForPath(portablePath, false, "portable")
            root.currentPortableIndex = portableIndex
            if (portableIndex < 0) {
                root.currentPortablePath = ""
            }
        } else if (root.currentPortableIndex >= 0 && root.portableIndexes.indexOf(root.currentPortableIndex) < 0) {
            root.currentPortableIndex = -1
        }

        if (folderPath.length > 0) {
            const folderIndex = root.placeIndexForPath(folderPath, false, "place")
            root.currentFolderIndex = folderIndex
            if (folderIndex < 0) {
                root.currentFolderPath = ""
            }
        } else if (root.currentFolderIndex >= 0 && root.folderIndexes.indexOf(root.currentFolderIndex) < 0) {
            root.currentFolderIndex = -1
        }

        if (driveContextMenu.drivePath.length > 0
                && root.placeIndexForPath(driveContextMenu.drivePath, true) < 0) {
            driveContextMenu.close()
            driveContextMenu.reset()
        }
        root.schedulePositionerRefresh()
    }

    function refreshModelDerivedState() {
        root.storageRevision += 1
        root.refreshIndexSnapshots()
    }

    function clearRemovedDriveState(rootPath) {
        if (!rootPath) return
        if (root.pathsEqual(driveContextMenu.drivePath, rootPath)) {
            driveContextMenu.close()
            driveContextMenu.reset()
        }
        if (quickLookController.path
                && workspaceController.pathBelongsToVolumeRoot(quickLookController.path, rootPath)) {
            quickLookController.preview("devices://")
        }
        if (root.pathsEqual(root.currentDrivePath, rootPath)) {
            root.currentDriveIndex = -1
            root.currentDrivePath = ""
        }
        root.refreshIndexSnapshots()
    }

    function clearUnmountedIsoState(rootPath) {
        root.clearRemovedDriveState(rootPath)
    }

    Component.onCompleted: refreshIndexSnapshots()
    onVisibleChanged: {
        if (visible) {
            root.schedulePositionerRefresh()
        }
    }
    onWidthChanged: root.schedulePositionerRefresh()
    onHeightChanged: root.schedulePositionerRefresh()
    onResizeOptimizedChanged: {
        if (!root.resizeOptimized) {
            root.schedulePositionerRefresh()
        }
    }

    Timer {
        id: relayoutTimer
        interval: 0
        repeat: false
        onTriggered: {
            root.refreshPositioners()
            Qt.callLater(root.refreshPositioners)
        }
    }

    Connections {
        target: workspaceController.placesModel
        function onModelReset() { root.refreshModelDerivedState() }
        function onRowsInserted() { root.refreshModelDerivedState() }
        function onRowsRemoved() { root.refreshModelDerivedState() }
        function onDataChanged() { root.refreshModelDerivedState() }
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

    Connections {
        target: workspaceController.volumeMonitor
        function onVolumeRemoved(rootPath, displayName) {
            root.clearRemovedDriveState(rootPath)
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
        case "usb":     return Theme.actionIconColor("success")
        case "optical": return Theme.actionIconColor("warning")
        case "network": return Theme.actionIconColor("navigation")
        case "iso":     return Theme.actionIconColor("utility")
        case "ssd":     return Theme.actionIconColor("info")
        default:        return Theme.actionIconColor("drive")
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
        if (isCritical || percent > 0.90) return Theme.danger
        if (percent > 0.75)              return Theme.warning
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

    function displayPath(path) {
        if (typeof workspaceController !== "undefined" && workspaceController && workspaceController.displayPath) {
            return workspaceController.displayPath(String(path || ""))
        }
        return String(path || "")
    }

    function folderIconSource(iconName) {
        if (!iconName || iconName === "drive") return ""
        if (iconName === "gdrive") return "qrc:/qt/qml/FM/qml/assets/filetypes-next/gdrive.svg"
        return "qrc:/qt/qml/FM/qml/assets/icons/" + iconName + ".svg"
    }

    function folderIconColor(iconName) {
        switch (iconName) {
        case "home":     return Theme.actionIconColor("folder")
        case "desktop":  return Theme.actionIconColor("navigation")
        case "download": return Theme.actionIconColor("action")
        case "document": return Theme.actionIconColor("document")
        case "image":    return Theme.actionIconColor("image")
        case "music":    return Theme.actionIconColor("media")
        case "video":    return Theme.actionIconColor("media")
        default:         return Theme.actionIconColor("folder")
        }
    }

    // ── Summary stats ──────────────────────────────────────────────────────────

    function portableIconSource(driveType) {
        return String(driveType) === "camera"
            ? "qrc:/qt/qml/FM/qml/assets/icons/image.svg"
            : "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
    }

    function portableIconColor(driveType) {
        return String(driveType) === "camera"
            ? Theme.actionIconColor("image")
            : Theme.actionIconColor("media")
    }

    readonly property real totalSpaceSum: {
        root.storageRevision
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
        root.storageRevision
        var sum = 0
        var m = workspaceController.placesModel
        for (var i = 0; i < m.rowCount(); i++) {
            if (m.data(m.index(i, 0), root.isDriveRole)) {
                sum += m.data(m.index(i, 0), root.freeSpaceRole)
            }
        }
        return sum
    }
    readonly property bool driveSelected: currentDriveIndex >= 0
    readonly property bool portableSelected: currentPortableIndex >= 0
    readonly property bool folderSelected: currentFolderIndex >= 0
    readonly property int driveCount: driveIndexes.length
    readonly property int portableCount: portableIndexes.length
    readonly property int folderCount: folderIndexes.length
    readonly property string selectedDriveName: driveSelected ? modelValue(currentDriveIndex, nameRole, "") : ""
    readonly property string selectedDrivePath: driveSelected ? modelValue(currentDriveIndex, pathRole, "") : ""
    readonly property string selectedDriveFileSystem: driveSelected ? modelValue(currentDriveIndex, fileSystemRole, "") : ""
    readonly property string selectedDriveType: driveSelected ? modelValue(currentDriveIndex, driveTypeRole, "") : ""
    readonly property bool selectedDriveReady: driveSelected ? !!modelValue(currentDriveIndex, isReadyRole, false) : false
    readonly property bool selectedDriveCritical: driveSelected ? !!modelValue(currentDriveIndex, isCriticalRole, false) : false
    readonly property real selectedDriveTotalSpace: driveSelected ? Number(modelValue(currentDriveIndex, totalSpaceRole, 0)) : 0
    readonly property real selectedDriveFreeSpace: driveSelected ? Number(modelValue(currentDriveIndex, freeSpaceRole, 0)) : 0
    readonly property real selectedDriveUsagePercent: driveSelected ? Number(modelValue(currentDriveIndex, usagePercentRole, 0)) : 0
    readonly property string selectedPortableName: portableSelected ? modelValue(currentPortableIndex, nameRole, "") : ""
    readonly property string selectedPortablePath: portableSelected ? modelValue(currentPortableIndex, pathRole, "") : ""
    readonly property string selectedPortableSubtitle: portableSelected ? modelValue(currentPortableIndex, subtitleRole, "") : ""
    readonly property string selectedFolderName: folderSelected ? modelValue(currentFolderIndex, nameRole, "") : ""
    readonly property string selectedFolderPath: folderSelected ? modelValue(currentFolderIndex, pathRole, "") : ""
    readonly property real aggregateUsagePercent: totalSpaceSum > 0 ? Math.max(0, Math.min(1, (totalSpaceSum - freeSpaceSum) / totalSpaceSum)) : 0
    readonly property string footerPrimaryText: {
        if (driveSelected) {
            return (selectedDriveName || selectedDrivePath) + " selected"
        }
        if (portableSelected) {
            return (selectedPortableName || selectedPortablePath) + " selected"
        }
        if (folderSelected) {
            return (selectedFolderName || selectedFolderPath) + " selected"
        }
        return driveCount + (driveCount === 1 ? " drive" : " drives")
            + ", " + portableCount + (portableCount === 1 ? " media device" : " media devices")
            + " and " + folderCount + (folderCount === 1 ? " shortcut" : " shortcuts")
    }
    readonly property string footerSecondaryText: {
        if (driveSelected) {
            if (!selectedDriveReady) {
                return "Drive is not ready"
            }
            let parts = []
            if (selectedDriveType.length > 0) parts.push(driveTypeLabel(selectedDriveType))
            if (selectedDriveFileSystem.length > 0) parts.push(selectedDriveFileSystem)
            if (selectedDrivePath.length > 0) parts.push(root.displayPath(selectedDrivePath))
            return parts.join(" • ")
        }
        if (portableSelected) {
            return selectedPortableSubtitle.length > 0 ? selectedPortableSubtitle : root.displayPath(selectedPortablePath)
        }
        if (folderSelected) {
            return root.displayPath(selectedFolderPath)
        }
        return systemInfoProvider.computerName + " • " + systemInfoProvider.osName
    }
    readonly property string footerStorageText: {
        if (driveSelected) {
            if (selectedDriveReady && selectedDriveTotalSpace > 0) {
                return formatBytes(selectedDriveFreeSpace) + " free"
            }
            return "Not ready"
        }
        if (totalSpaceSum > 0) {
            return formatBytes(freeSpaceSum) + " free"
        }
        return driveCount + (driveCount === 1 ? " drive" : " drives")
    }
    readonly property string footerStorageTooltipText: {
        if (driveSelected) {
            if (selectedDriveReady && selectedDriveTotalSpace > 0) {
                return formatBytes(selectedDriveFreeSpace) + " free of " + formatBytes(selectedDriveTotalSpace)
            }
            return "This drive is not ready"
        }
        if (totalSpaceSum > 0) {
            return formatBytes(freeSpaceSum) + " free of " + formatBytes(totalSpaceSum) + " across all drives"
        }
        return "Storage totals are unavailable"
    }
    readonly property real footerUsageValue: {
        if (driveSelected && selectedDriveReady && selectedDriveTotalSpace > 0) {
            return selectedDriveUsagePercent
        }
        if (!driveSelected && totalSpaceSum > 0) {
            return aggregateUsagePercent
        }
        return 0
    }
    readonly property bool footerStorageCritical: driveSelected && selectedDriveCritical

    // Dynamic layout spacing to fill larger window heights
    readonly property real baseContentHeight: (root.ultraLightMode ? 276 : 356)
                                              + flowLayout.implicitHeight
                                              + portableFlow.implicitHeight
                                              + quickAccessFlow.implicitHeight
    readonly property real extraHeight: Math.max(0, root.height - baseContentHeight)
    readonly property real gapAmount: Math.min(root.ultraLightMode ? 36 : 120, extraHeight / 3)

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
            visible: !root.effectsReduced
            layer.enabled: visible
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
            color: Theme.categoryNavigation
            opacity: themeController.isDark ? 0.05 : 0.03
            visible: !root.effectsReduced
            layer.enabled: visible
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
                implicitHeight: root.ultraLightMode ? 44 : 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.ultraLightMode ? 14 : 20
                    anchors.rightMargin: root.ultraLightMode ? 14 : 20
                    spacing: root.ultraLightMode ? 8 : 10

                    RecolorSvgIcon {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        sourcePath: "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
                        recolorColor: Theme.actionIconColor("system")
                        sourceSize: Qt.size(20, 20)
                    }

                    Label {
                        font.family: Theme.fontFamily
                        text: "System Information"
                        font.pixelSize: Theme.fontSizeTitle
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
            Item {
                id: dashboardCardContainer
                Layout.fillWidth: true
                Layout.leftMargin: root.ultraLightMode ? 12 : 16
                Layout.rightMargin: root.ultraLightMode ? 12 : 16
                Layout.topMargin: root.ultraLightMode ? 10 : 16
                Layout.bottomMargin: (root.ultraLightMode ? 10 : 20) + root.gapAmount
                implicitHeight: root.ultraLightMode ? 92 : 132

                // Shadow underlay (no children)
                Rectangle {
                    anchors.fill: parent
                    radius: root.ultraLightMode ? Theme.radiusMd : Theme.radiusLg
                    color: "transparent"
                    layer.enabled: !root.effectsReduced
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Qt.rgba(0, 0, 0, themeController.isDark ? 0.20 : 0.06)
                        shadowBlur: 0.8
                        shadowVerticalOffset: 3
                    }
                }

                SurfaceCard {
                    id: dashboardCard
                    anchors.fill: parent
                    cornerRadius: root.ultraLightMode ? Theme.radiusMd : Theme.radiusLg
                    surfaceColor: themeController.isDark
                        ? Theme.withAlpha(Theme.panelSurface, 0.78)
                        : Theme.withAlpha(Theme.panelSurface, 0.92)
                    strokeColor: Theme.panelBorder

                    RowLayout {
                    anchors.fill: parent
                    anchors.margins: root.ultraLightMode ? 12 : 16
                    spacing: 0

                    // Left Column (System Info)
                    ColumnLayout {
                        spacing: 4
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: 260

                        RowLayout {
                            spacing: 8
                            Label {
                                font.family: Theme.fontFamily
                                text: systemInfoProvider.computerName
                                font.pixelSize: Theme.scaledSize(15)
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
                            font.family: Theme.fontFamily
                            text: systemInfoProvider.cpuName || "Detecting CPU..."
                            font.pixelSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 6
                            Label {
                                font.family: Theme.fontFamily
                                text: systemInfoProvider.cpuCores + " Cores (" + systemInfoProvider.cpuArchitecture + ")"
                                font.pixelSize: Theme.fontSizeMicro
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
                                font.family: Theme.fontFamily
                                text: "Uptime: " + systemInfoProvider.uptime
                                font.pixelSize: Theme.fontSizeMicro
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
                        visible: !root.ultraLightMode

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
                                    onValChanged: {
                                        if (!root.effectsReduced) {
                                            requestPaint()
                                        }
                                    }
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
                                        ctx.strokeStyle = Theme.categoryNavigation;
                                        ctx.lineCap = "round";
                                        ctx.stroke();
                                    }
                                }

                                Label {
                                    font.family: Theme.fontFamily
                                    anchors.centerIn: parent
                                    text: Math.round(systemInfoProvider.ramUsage * 100) + "%"
                                    font.pixelSize: Theme.fontSizeMicro
                                    font.bold: true
                                    color: Theme.textPrimary
                                }
                            }

                            Label {
                                font.family: Theme.fontFamily
                                text: systemInfoProvider.usedRamGB.toFixed(1) + " / " + systemInfoProvider.totalRamGB.toFixed(0) + " GB"
                                font.pixelSize: Theme.scaledSize(9)
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
                                    onValChanged: {
                                        if (!root.effectsReduced) {
                                            requestPaint()
                                        }
                                    }
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
                                        ctx.strokeStyle = Theme.categoryInfo;
                                        ctx.lineCap = "round";
                                        ctx.stroke();
                                    }
                                }

                                Label {
                                    font.family: Theme.fontFamily
                                    anchors.centerIn: parent
                                    text: Math.round(systemInfoProvider.cpuUsage * 100) + "%"
                                    font.pixelSize: Theme.fontSizeMicro
                                    font.bold: true
                                    color: Theme.textPrimary
                                }
                            }

                            Label {
                                font.family: Theme.fontFamily
                                text: "CPU Load"
                                font.pixelSize: Theme.scaledSize(9)
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
                            font.family: Theme.fontFamily
                            text: "Unified Drive Usage"
                            font.pixelSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textPrimary
                        }

                        LinearProgress {
                            Layout.fillWidth: true
                            readonly property real usage: root.totalSpaceSum > 0 ? (root.totalSpaceSum - root.freeSpaceSum) / root.totalSpaceSum : 0.0
                            value: usage
                            trackColor: themeController.isDark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(0,0,0,0.05)
                            fillColor: usage > 0.90 ? Theme.danger : (usage > 0.75 ? Theme.warning : Theme.accent)
                        }

                        Label {
                            font.family: Theme.fontFamily
                            text: root.formatBytes(root.totalSpaceSum - root.freeSpaceSum) + " used of " + root.formatBytes(root.totalSpaceSum)
                            font.pixelSize: Theme.scaledSize(9)
                            color: Theme.textSecondary
                            opacity: 0.8
                        }
                    }
                }
            }
        } // end dashboardCardContainer

            // ── Drives Section Header ─────────────────────────────────────────
            Item {
                Layout.fillWidth: true
                implicitHeight: root.ultraLightMode ? 28 : 32

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.ultraLightMode ? 14 : 20
                    anchors.rightMargin: root.ultraLightMode ? 14 : 20
                    spacing: root.ultraLightMode ? 6 : 8

                    Rectangle {
                        width: 4
                        height: 14
                        radius: 2
                        color: Theme.accent
                    }

                    Label {
                        font.family: Theme.fontFamily
                        text: "Devices and Drives"
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: true
                        color: Theme.textPrimary
                    }
                }
            }

            // ── Drives Flow Layout ────────────────────────────────────────────
            Flow {
                id: flowLayout
                Layout.fillWidth: true
                Layout.leftMargin: root.ultraLightMode ? 12 : 16
                Layout.rightMargin: root.ultraLightMode ? 12 : 16
                Layout.topMargin: root.ultraLightMode ? 6 : 8
                Layout.bottomMargin: (root.ultraLightMode ? 10 : 16) + root.gapAmount
                spacing: root.ultraLightMode ? 8 : 12

                readonly property int minCardW: root.ultraLightMode ? 240 : 280
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
                        height: root.ultraLightMode ? 82 : 108
                        visible: true

                        // Shadow underlay (no children)
                        Rectangle {
                            id: cardShadow
                            anchors.fill: cardVisual
                            radius: cardVisual.radius
                            scale: cardVisual.scale
                            color: "transparent"

                            layer.enabled: !root.effectsReduced
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: themeController.isDark
                                    ? Qt.rgba(0, 0, 0, cardMouse.containsMouse ? 0.34 : (cardWrapper.isSelected ? 0.30 : 0.12))
                                    : Qt.rgba(0, 0, 0, cardMouse.containsMouse ? 0.10 : (cardWrapper.isSelected ? 0.08 : 0.03))
                                shadowBlur: cardMouse.containsMouse ? 0.8 : (cardWrapper.isSelected ? 0.7 : 0.4)
                                shadowVerticalOffset: cardMouse.containsMouse ? 5 : (cardWrapper.isSelected ? 3 : 2)
                                shadowHorizontalOffset: 0
                            }
                        }

                        Rectangle {
                            id: cardVisual
                            x: 0
                            y: !root.effectsReduced && cardMouse.containsMouse ? -2 : 0
                            width: parent.width
                            height: parent.height
                            radius: Theme.radiusMd
                            scale: !root.effectsReduced && cardMouse.containsMouse
                                ? 1.02
                                : (cardWrapper.isSelected ? 1.01 : 1.0)

                            color: {
                                if (themeController.isDark) {
                                    if (!root.effectsReduced && cardMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.86)
                                    return Theme.withAlpha(Theme.panelSurface, 0.62)
                                } else {
                                    if (!root.effectsReduced && cardMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.94)
                                    return Theme.withAlpha(Theme.panelSurface, 0.74)
                                }
                            }

                            border.color: {
                                if (cardWrapper.isSelected) {
                                    return Theme.accent
                                }
                                if (!root.effectsReduced && cardMouse.containsMouse) {
                                    return themeController.isDark
                                        ? Theme.withAlpha(Theme.accent, 0.46)
                                        : Theme.withAlpha(Theme.accent, 0.36)
                                }
                                return Theme.panelBorder
                            }
                            border.width: cardWrapper.isSelected ? 1.5 : 1

                            Behavior on color { enabled: !root.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
                            Behavior on border.color { enabled: !root.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
                            Behavior on scale { enabled: !root.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                            Behavior on y { enabled: !root.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                        } // end cardVisual

                        RowLayout {
                            anchors.fill: cardVisual
                                anchors.margins: root.ultraLightMode ? 10 : 14
                                spacing: root.ultraLightMode ? 10 : 14

                                // Drive icon column
                                Item {
                                    Layout.preferredWidth: root.ultraLightMode ? 38 : 48
                                    Layout.alignment: Qt.AlignVCenter

                                    IconTile {
                                        anchors.centerIn: parent
                                        tileSize: root.ultraLightMode ? 34 : 44
                                        iconSize: root.ultraLightMode ? 19 : 24
                                        cornerRadius: Theme.radiusMd
                                        source: root.driveIconSource(cardWrapper.driveType)
                                        iconColor: root.driveIconColor(cardWrapper.driveType)
                                        tileColor: Theme.withAlpha(
                                            root.driveIconColor(cardWrapper.driveType),
                                            (themeController.isDark ? 0.18 : 0.12)
                                                + (!root.effectsReduced && cardMouse.containsMouse ? 0.08 : 0))

                                        Behavior on tileColor { enabled: !root.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
                                    }
                                }

                                // Info column
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: root.ultraLightMode ? 4 : 5

                                    // Drive name + FS badge row
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Label {
                                            font.family: Theme.fontFamily
                                            text: cardWrapper.driveName || cardWrapper.drivePath
                                            font.pixelSize: Theme.fontSizeBody
                                            font.bold: true
                                            color: Theme.textPrimary
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        // FS badge
                                        InlineBadge {
                                            visible: !root.ultraLightMode && cardWrapper.fileSystem && cardWrapper.fileSystem.length > 0
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
                                        font.family: Theme.fontFamily
                                        text: cardWrapper.isReady
                                            ? (root.formatBytes(cardWrapper.freeSpace) + " free of " + root.formatBytes(cardWrapper.totalSpace))
                                            : "Not ready"
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: cardWrapper.isCritical ? Theme.danger : Theme.textSecondary
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
                                        visible: !root.ultraLightMode
                                        spacing: 4

                                        Label {
                                            font.family: Theme.fontFamily
                                            text: root.driveTypeLabel(cardWrapper.driveType)
                                            font.pixelSize: Theme.fontSizeMicro
                                            font.bold: true
                                            font.letterSpacing: 0.8
                                            color: root.driveIconColor(cardWrapper.driveType)
                                            opacity: 0.82
                                        }

                                        Item { Layout.fillWidth: true }

                                        // Warning icon for critical
                                        Label {
                                            font.family: Theme.fontFamily
                                            text: "⚠"
                                            font.pixelSize: Theme.fontSizeCaption
                                            color: Theme.danger
                                            visible: cardWrapper.isCritical
                                        }

                                        Label {
                                            font.family: Theme.fontFamily
                                            text: cardWrapper.isReady
                                                ? (Math.round(cardWrapper.usagePercent * 100) + "% used")
                                                : "—"
                                            font.pixelSize: Theme.fontSizeMicro
                                            color: cardWrapper.isCritical ? Theme.danger : Theme.textSecondary
                                            opacity: 0.75
                                        }
                                    }
                                }
                            }

                            // Mouse interaction
                            MouseArea {
                                id: cardMouse
                                anchors.fill: parent
                                hoverEnabled: !root.effectsReduced
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                cursorShape: Qt.PointingHandCursor

                                onClicked: function(mouse) {
                                    if (root.panel) root.panel.activated()
                                    root.forceActiveFocus()
                                    root.currentDriveIndex = cardWrapper.sourceIndex
                                    root.currentPortableIndex = -1
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

                        // Card appear animation
                        opacity: 0
                        Component.onCompleted: {
                            if (root.effectsReduced) {
                                opacity = 1
                            } else {
                                appearAnim.start()
                            }
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

            Item {
                Layout.fillWidth: true
                visible: root.portableCount > 0
                implicitHeight: visible ? (root.ultraLightMode ? 28 : 32) : 0

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.ultraLightMode ? 14 : 20
                    anchors.rightMargin: root.ultraLightMode ? 14 : 20
                    spacing: root.ultraLightMode ? 6 : 8

                    Rectangle {
                        width: 4
                        height: 14
                        radius: Theme.radiusSm
                        color: Theme.actionIconColor("media")
                    }

                    Label {
                        font.family: Theme.fontFamily
                        text: "Portable Media Devices"
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: true
                        color: Theme.textPrimary
                    }
                }
            }

            Flow {
                id: portableFlow
                Layout.fillWidth: true
                Layout.leftMargin: root.ultraLightMode ? 12 : 16
                Layout.rightMargin: root.ultraLightMode ? 12 : 16
                Layout.topMargin: root.portableCount > 0 ? (root.ultraLightMode ? 6 : 8) : 0
                Layout.bottomMargin: root.portableCount > 0 ? ((root.ultraLightMode ? 10 : 16) + root.gapAmount) : 0
                spacing: root.ultraLightMode ? 8 : 12
                visible: root.portableCount > 0

                readonly property int minCardW: root.ultraLightMode ? 210 : 250
                readonly property int cols: Math.max(1, Math.floor((width + spacing) / (minCardW + spacing)))
                readonly property real cardW: Math.floor((width - (cols - 1) * spacing) / cols)

                Repeater {
                    id: portableRepeater
                    model: root.portableIndexes
                    delegate: Item {
                        id: portableCardWrapper
                        readonly property int sourceIndex: modelData
                        readonly property string devicePath: root.modelValue(sourceIndex, root.pathRole, "")
                        readonly property string deviceName: root.modelValue(sourceIndex, root.nameRole, "")
                        readonly property string deviceType: root.modelValue(sourceIndex, root.driveTypeRole, "")
                        readonly property string subtitle: root.modelValue(sourceIndex, root.subtitleRole, "")
                        readonly property bool isReady: root.modelValue(sourceIndex, root.isReadyRole, true)
                        property real appearOffsetY: 10
                        width: portableFlow.cardW
                        height: root.ultraLightMode ? 58 : 76
                        visible: true
                        property bool isSelected: root.currentPortableIndex === sourceIndex
                        transform: Translate { y: portableCardWrapper.appearOffsetY }

                        Rectangle {
                            id: portableCardVisual
                            x: 0
                            y: !root.effectsReduced && portableMouse.containsMouse
                                ? -2
                                : (portableCardWrapper.isSelected ? -1 : 0)
                            width: parent.width
                            height: parent.height
                            radius: Theme.radiusSm
                            scale: !root.effectsReduced && portableMouse.containsMouse
                                ? 1.02
                                : (portableCardWrapper.isSelected ? 1.01 : 1.0)

                            color: {
                                if (portableCardWrapper.isSelected) {
                                    return themeController.isDark
                                        ? Theme.withAlpha(Theme.panelSurface, 0.90)
                                        : Theme.withAlpha(Theme.panelSurface, 0.97)
                                }
                                if (themeController.isDark) {
                                    if (!root.effectsReduced && portableMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.84)
                                    return Theme.withAlpha(Theme.panelSurface, 0.62)
                                } else {
                                    if (!root.effectsReduced && portableMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.92)
                                    return Theme.withAlpha(Theme.panelSurface, 0.74)
                                }
                            }

                            border.color: portableCardWrapper.isSelected
                                ? Theme.accent
                                : (!root.effectsReduced && portableMouse.containsMouse
                                    ? (themeController.isDark ? Theme.withAlpha(Theme.accent, 0.46) : Theme.withAlpha(Theme.accent, 0.36))
                                    : Theme.panelBorder)
                            border.width: portableCardWrapper.isSelected ? 1.5 : 1

                            Behavior on color { enabled: !root.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
                            Behavior on border.color { enabled: !root.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
                            Behavior on scale { enabled: !root.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                            Behavior on y { enabled: !root.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                        } // end portableCardVisual

                        RowLayout {
                            anchors.fill: portableCardVisual
                                anchors.margins: root.ultraLightMode ? 8 : 10
                                spacing: root.ultraLightMode ? 8 : 10

                                IconTile {
                                    tileSize: root.ultraLightMode ? 30 : 36
                                    iconSize: root.ultraLightMode ? 15 : 18
                                    cornerRadius: Theme.radiusSm
                                    source: root.portableIconSource(portableCardWrapper.deviceType)
                                    iconColor: root.portableIconColor(portableCardWrapper.deviceType)
                                    tileColor: Theme.withAlpha(
                                        root.portableIconColor(portableCardWrapper.deviceType),
                                        (themeController.isDark ? 0.15 : 0.10)
                                            + ((!root.effectsReduced && portableMouse.containsMouse) || portableCardWrapper.isSelected ? 0.10 : 0))
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Label {
                                            font.family: Theme.fontFamily
                                            text: portableCardWrapper.deviceName || portableCardWrapper.devicePath
                                            font.pixelSize: Theme.fontSizeLabel
                                            font.bold: true
                                            color: Theme.textPrimary
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        InlineBadge {
                                            visible: !root.ultraLightMode
                                            text: "READ ONLY"
                                            fillColor: Theme.withAlpha(root.portableIconColor(portableCardWrapper.deviceType), themeController.isDark ? 0.18 : 0.12)
                                            strokeColor: "transparent"
                                            textColor: root.portableIconColor(portableCardWrapper.deviceType)
                                            horizontalPadding: 7
                                            badgeHeight: 17
                                            fontSize: 8
                                            fontWeight: Font.Bold
                                        }
                                    }

                                    Label {
                                        font.family: Theme.fontFamily
                                        visible: !root.ultraLightMode
                                        text: portableCardWrapper.subtitle || "Portable media device"
                                        font.pixelSize: Theme.fontSizeMicro
                                        color: Theme.textSecondary
                                        opacity: 0.72
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            MouseArea {
                                id: portableMouse
                                anchors.fill: parent
                                hoverEnabled: !root.effectsReduced
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton

                                onClicked: function(mouse) {
                                    if (root.panel) root.panel.activated()
                                    root.forceActiveFocus()
                                    root.currentDriveIndex = -1
                                    root.currentPortableIndex = portableCardWrapper.sourceIndex
                                    root.currentFolderIndex = -1
                                    quickLookController.preview(portableCardWrapper.devicePath)
                                }

                                onDoubleClicked: function(mouse) {
                                    if (!portableCardWrapper.isReady) return
                                    root.controller.openPath(portableCardWrapper.devicePath)
                                }
                            }

                        opacity: 0
                        Component.onCompleted: {
                            if (root.effectsReduced) {
                                opacity = 1
                                appearOffsetY = 0
                            } else {
                                portableAppearAnim.start()
                            }
                        }

                        ParallelAnimation {
                            id: portableAppearAnim
                            NumberAnimation {
                                target: portableCardWrapper
                                property: "opacity"
                                from: 0; to: 1
                                duration: 260 + (index % 6) * 40
                                easing.type: Easing.OutCubic
                            }
                            NumberAnimation {
                                target: portableCardWrapper
                                property: "appearOffsetY"
                                from: 10; to: 0
                                duration: 300 + (index % 6) * 40
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }
            }

            // ── Quick Access Section Header ───────────────────────────────────
            Item {
                Layout.fillWidth: true
                implicitHeight: root.ultraLightMode ? 28 : 32

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.ultraLightMode ? 14 : 20
                    anchors.rightMargin: root.ultraLightMode ? 14 : 20
                    spacing: root.ultraLightMode ? 6 : 8

                    Rectangle {
                        width: 4
                        height: 14
                        radius: Theme.radiusSm
                        color: Theme.withAlpha(Theme.accent, 0.92)
                    }

                    Label {
                        font.family: Theme.fontFamily
                        text: "Quick Access"
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: true
                        color: Theme.textPrimary
                    }
                }
            }

            // ── Quick Access Flow Layout ──────────────────────────────────────
            Flow {
                id: quickAccessFlow
                Layout.fillWidth: true
                Layout.leftMargin: root.ultraLightMode ? 12 : 16
                Layout.rightMargin: root.ultraLightMode ? 12 : 16
                Layout.topMargin: root.ultraLightMode ? 6 : 8
                Layout.bottomMargin: (root.ultraLightMode ? 10 : 16) + root.gapAmount
                spacing: root.ultraLightMode ? 8 : 12

                readonly property int minCardW: root.ultraLightMode ? 150 : 180
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
                        property real appearOffsetY: 10
                        width: quickAccessFlow.cardW
                        height: root.ultraLightMode ? 52 : 68
                        visible: true
                        property bool isSelected: root.currentFolderIndex === sourceIndex
                        transform: Translate { y: folderCardWrapper.appearOffsetY }

                        // Shadow underlay (no children)
                        Rectangle {
                            id: folderCardShadow
                            anchors.fill: folderCardVisual
                            radius: folderCardVisual.radius
                            scale: folderCardVisual.scale
                            color: "transparent"

                            layer.enabled: !root.effectsReduced
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: themeController.isDark
                                    ? Qt.rgba(0, 0, 0, (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 0.32 : 0.10)
                                    : Qt.rgba(0, 0, 0, (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 0.08 : 0.02)
                                shadowBlur: (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 0.6 : 0.3
                                shadowVerticalOffset: (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 4 : 2
                            }
                        }

                        Rectangle {
                            id: folderCardVisual
                            x: 0
                            y: !root.effectsReduced && folderMouse.containsMouse
                                ? -2
                                : (folderCardWrapper.isSelected ? -1 : 0)
                            width: parent.width
                            height: parent.height
                            radius: Theme.radiusSm
                            scale: !root.effectsReduced && folderMouse.containsMouse
                                ? 1.02
                                : (folderCardWrapper.isSelected ? 1.01 : 1.0)

                            color: {
                                if (folderCardWrapper.isSelected) {
                                    return themeController.isDark
                                        ? Theme.withAlpha(Theme.panelSurface, 0.90)
                                        : Theme.withAlpha(Theme.panelSurface, 0.97)
                                }
                                if (themeController.isDark) {
                                    if (!root.effectsReduced && folderMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.84)
                                    return Theme.withAlpha(Theme.panelSurface, 0.62)
                                } else {
                                    if (!root.effectsReduced && folderMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.92)
                                    return Theme.withAlpha(Theme.panelSurface, 0.74)
                                }
                            }

                            border.color: folderCardWrapper.isSelected
                                ? Theme.accent
                                : (!root.effectsReduced && folderMouse.containsMouse
                                    ? (themeController.isDark ? Theme.withAlpha(Theme.accent, 0.46) : Theme.withAlpha(Theme.accent, 0.36))
                                    : Theme.panelBorder)
                            border.width: folderCardWrapper.isSelected ? 1.5 : 1

                            Behavior on color { enabled: !root.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
                            Behavior on border.color { enabled: !root.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
                            Behavior on scale { enabled: !root.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                            Behavior on y { enabled: !root.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                        } // end folderCardVisual

                        RowLayout {
                            anchors.fill: folderCardVisual
                                anchors.margins: root.ultraLightMode ? 8 : 10
                                spacing: root.ultraLightMode ? 8 : 10

                                IconTile {
                                    tileSize: root.ultraLightMode ? 28 : 32
                                    iconSize: root.ultraLightMode ? 14 : 16
                                    cornerRadius: Theme.radiusSm
                                    source: root.folderIconSource(folderCardWrapper.folderIcon)
                                    useOriginalColor: folderCardWrapper.folderIcon === "gdrive"
                                    iconColor: root.folderIconColor(folderCardWrapper.folderIcon)
                                    tileColor: Theme.withAlpha(
                                        root.folderIconColor(folderCardWrapper.folderIcon),
                                        (themeController.isDark ? 0.15 : 0.10)
                                            + ((!root.effectsReduced && folderMouse.containsMouse) || folderCardWrapper.isSelected ? 0.10 : 0))

                                    Behavior on tileColor { enabled: !root.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Label {
                                        font.family: Theme.fontFamily
                                        text: folderCardWrapper.folderName
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.bold: true
                                        color: Theme.textPrimary
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        font.family: Theme.fontFamily
                                        visible: !root.ultraLightMode
                                        text: "System Folder"
                                        font.pixelSize: Theme.fontSizeMicro
                                        color: Theme.textSecondary
                                        opacity: 0.6
                                    }
                                }
                            }

                            MouseArea {
                                id: folderMouse
                                anchors.fill: parent
                                hoverEnabled: !root.effectsReduced
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton | Qt.RightButton

                                onClicked: function(mouse) {
                                    if (root.panel) root.panel.activated()
                                    root.forceActiveFocus()
                                    if (mouse.button === Qt.RightButton) return
                                    root.currentDriveIndex = -1
                                    root.currentPortableIndex = -1
                                    root.currentFolderIndex = folderCardWrapper.sourceIndex
                                    quickLookController.preview(folderCardWrapper.folderPath)
                                }

                                onDoubleClicked: function(mouse) {
                                    root.controller.openPath(folderCardWrapper.folderPath)
                                }
                            }

                        // Staggered fade-in/slide-up animation
                        opacity: 0
                        Component.onCompleted: {
                            if (root.effectsReduced) {
                                opacity = 1
                                appearOffsetY = 0
                            } else {
                                folderAppearAnim.start()
                            }
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
                                property: "appearOffsetY"
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

        onAnalyzeRequested: function(path) {
            if (root.Window.window && root.Window.window.openDiskUsage) {
                root.Window.window.openDiskUsage(path)
            }
        }

        onEjectRequested: function(path, managedIsoMount) {
            if (managedIsoMount) {
                workspaceController.unmountIsoRoot(path)
            } else {
                workspaceController.requestEjectVolume(path)
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
        let portable = getPortableIndexes()
        let folders = getFolderIndexes()

        if (drives.length === 0 && portable.length === 0 && folders.length === 0) return

        let isDriveSelected = (root.currentDriveIndex >= 0)
        let isPortableSelected = (root.currentPortableIndex >= 0)
        let isFolderSelected = (root.currentFolderIndex >= 0)
        let m = workspaceController.placesModel

        function previewRow(row) {
            quickLookController.preview(m.data(m.index(row, 0), root.pathRole))
        }

        function selectDrive(row) {
            root.currentDriveIndex = row
            root.currentPortableIndex = -1
            root.currentFolderIndex = -1
            previewRow(row)
        }

        function selectPortable(row) {
            root.currentDriveIndex = -1
            root.currentPortableIndex = row
            root.currentFolderIndex = -1
            previewRow(row)
        }

        function selectFolder(row) {
            root.currentDriveIndex = -1
            root.currentPortableIndex = -1
            root.currentFolderIndex = row
            previewRow(row)
        }

        // Initial selection if none
        if (!isDriveSelected && !isPortableSelected && !isFolderSelected) {
            if (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                if (drives.length > 0) {
                    selectDrive(drives[0])
                } else if (portable.length > 0) {
                    selectPortable(portable[0])
                } else if (folders.length > 0) {
                    selectFolder(folders[0])
                }
                event.accepted = true
                return
            }
        }

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (isDriveSelected) {
                let path = m.data(m.index(root.currentDriveIndex, 0), Qt.UserRole + 2)
                if (path) root.controller.openPath(path)
            } else if (isPortableSelected) {
                let portablePath = m.data(m.index(root.currentPortableIndex, 0), Qt.UserRole + 2)
                if (portablePath) root.controller.openPath(portablePath)
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
                    selectDrive(drives[idx + 1])
                }
            } else if (isPortableSelected) {
                let idx = portable.indexOf(root.currentPortableIndex)
                if (idx >= 0 && idx < portable.length - 1) {
                    selectPortable(portable[idx + 1])
                }
            } else if (isFolderSelected) {
                let idx = folders.indexOf(root.currentFolderIndex)
                if (idx >= 0 && idx < folders.length - 1) {
                    selectFolder(folders[idx + 1])
                }
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            if (isDriveSelected) {
                let idx = drives.indexOf(root.currentDriveIndex)
                if (idx > 0) {
                    selectDrive(drives[idx - 1])
                }
            } else if (isPortableSelected) {
                let idx = portable.indexOf(root.currentPortableIndex)
                if (idx > 0) {
                    selectPortable(portable[idx - 1])
                }
            } else if (isFolderSelected) {
                let idx = folders.indexOf(root.currentFolderIndex)
                if (idx > 0) {
                    selectFolder(folders[idx - 1])
                }
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            if (isDriveSelected) {
                let idx = drives.indexOf(root.currentDriveIndex)
                let cols = flowLayout.cols
                if (idx >= 0 && idx + cols < drives.length) {
                    selectDrive(drives[idx + cols])
                } else if (portable.length > 0) {
                    selectPortable(portable[0])
                } else if (folders.length > 0) {
                    selectFolder(folders[0])
                }
            } else if (isPortableSelected) {
                let idx = portable.indexOf(root.currentPortableIndex)
                let cols = portableFlow.cols
                if (idx >= 0 && idx + cols < portable.length) {
                    selectPortable(portable[idx + cols])
                } else if (folders.length > 0) {
                    selectFolder(folders[0])
                }
            } else if (isFolderSelected) {
                let idx = folders.indexOf(root.currentFolderIndex)
                let cols = quickAccessFlow.cols
                if (idx >= 0 && idx + cols < folders.length) {
                    selectFolder(folders[idx + cols])
                }
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            if (isDriveSelected) {
                let idx = drives.indexOf(root.currentDriveIndex)
                let cols = flowLayout.cols
                if (idx - cols >= 0) {
                    selectDrive(drives[idx - cols])
                }
            } else if (isPortableSelected) {
                let idx = portable.indexOf(root.currentPortableIndex)
                let cols = portableFlow.cols
                if (idx - cols >= 0) {
                    selectPortable(portable[idx - cols])
                } else if (drives.length > 0) {
                    selectDrive(drives[drives.length - 1])
                }
            } else if (isFolderSelected) {
                let idx = folders.indexOf(root.currentFolderIndex)
                let cols = quickAccessFlow.cols
                if (idx - cols >= 0) {
                    selectFolder(folders[idx - cols])
                } else if (portable.length > 0) {
                    selectPortable(portable[portable.length - 1])
                } else if (drives.length > 0) {
                    selectDrive(drives[drives.length - 1])
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
        root.currentDrivePath = currentDriveIndex >= 0
            ? root.modelValue(currentDriveIndex, root.pathRole, "")
            : ""
        if (currentDriveIndex >= 0 && drivesRepeater) {
            Qt.callLater(() => {
                var item = drivesRepeater.itemAt(root.driveIndexes.indexOf(currentDriveIndex))
                if (item) ensureVisible(item)
            })
        }
    }

    onCurrentPortableIndexChanged: {
        root.currentPortablePath = currentPortableIndex >= 0
            ? root.modelValue(currentPortableIndex, root.pathRole, "")
            : ""
        if (currentPortableIndex >= 0 && portableRepeater) {
            Qt.callLater(() => {
                var item = portableRepeater.itemAt(root.portableIndexes.indexOf(currentPortableIndex))
                if (item) ensureVisible(item)
            })
        }
    }

    onCurrentFolderIndexChanged: {
        root.currentFolderPath = currentFolderIndex >= 0
            ? root.modelValue(currentFolderIndex, root.pathRole, "")
            : ""
        if (currentFolderIndex >= 0 && foldersRepeater) {
            Qt.callLater(() => {
                var item = foldersRepeater.itemAt(root.folderIndexes.indexOf(currentFolderIndex))
                if (item) ensureVisible(item)
            })
        }
    }
}
