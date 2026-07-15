import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"
import "storage"

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
        storageDriveGrid.refreshPositioners()
        quickAccessGrid.refreshPositioner()
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
    readonly property int sectionRole: Qt.UserRole + 17
    readonly property int subtitleRole: Qt.UserRole + 18
    readonly property int canUnmountRole: Qt.UserRole + 21
    readonly property int canSafelyRemoveRole: Qt.UserRole + 22
    readonly property int canMountRole: Qt.UserRole + 23
    readonly property int mountIdRole: Qt.UserRole + 16
    readonly property int actionPendingRole: Qt.UserRole + 24
    readonly property int deviceDescriptionRole: Qt.UserRole + 25
    readonly property bool resizeOptimized: root.liveResizeActive
    readonly property bool effectsReduced: root.resizeOptimized

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

    function previewDrive(row) {
        quickLookController.previewDrive({
            rootPath: modelValue(row, root.pathRole, ""),
            name: modelValue(row, root.nameRole, ""),
            totalBytes: modelValue(row, root.totalSpaceRole, 0),
            freeBytes: modelValue(row, root.freeSpaceRole, 0),
            fileSystem: modelValue(row, root.fileSystemRole, ""),
            driveType: modelValue(row, root.driveTypeRole, ""),
            critical: modelValue(row, root.isCriticalRole, false),
            deviceDescription: modelValue(row, root.deviceDescriptionRole, ""),
            blockDevice: ""
        })
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
        if (iconName === "mega") return "qrc:/qt/qml/FM/qml/assets/filetypes-next/mega.svg"
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
    readonly property real baseContentHeight: 356
                                              + storageDriveGrid.driveFlowImplicitHeight
                                              + storageDriveGrid.portableFlowImplicitHeight
                                              + quickAccessGrid.flowImplicitHeight
    readonly property real extraHeight: Math.max(0, root.height - baseContentHeight)
    readonly property real gapAmount: Math.min(120, extraHeight / 3)

    // ── Premium Ambient Background ────────────────────────────────────────────

    Item {
        anchors.fill: parent
        z: -1

        AmbientPanelBackground {
            anchors.fill: parent
            strength: 0.78
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

            SystemSummaryCard {
                id: systemSummary
                storageRoot: root
            }

            StorageDriveGrid {
                id: storageDriveGrid
                storageRoot: root
                contextMenu: driveContextMenu
            }

            QuickAccessGrid {
                id: quickAccessGrid
                storageRoot: root
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

        onMountRequested: function(mountId) {
            workspaceController.requestMountVolume(mountId)
        }

        onPropertiesRequested: function(path) {
            propertiesController.load(path)
        }
    }

    // ── Keyboard navigation ───────────────────────────────────────────────────

    StorageKeyboardNavigator {
        id: keyboardNavigator
        storageRoot: root
        flickable: mainFlickable
        layout: mainLayout
        driveGrid: storageDriveGrid
        quickAccessGrid: quickAccessGrid
    }

    Keys.onPressed: event => keyboardNavigator.handleKey(event)
    onCurrentDriveIndexChanged: keyboardNavigator.currentDriveIndexChanged()
    onCurrentPortableIndexChanged: keyboardNavigator.currentPortableIndexChanged()
    onCurrentFolderIndexChanged: keyboardNavigator.currentFolderIndexChanged()

}
