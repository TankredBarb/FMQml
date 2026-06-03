import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Rectangle {
    id: root

    property var controller
    property var placesModel
    property bool active: false
    property bool deviceRootMode: false
    property bool favoritesRootMode: false
    property int favoritesPinnedCount: 0
    property int favoritesFrequentCount: 0
    property int favoritesTagCount: 0
    property int viewMode: 0
    property string currentPath: ""
    property bool showLoadingRail: false
    property string statusMessage: ""
    property bool isCurrentPathArchive: false
    property real loadingProgress: -1
    property string loadingProgressText: ""
    property bool loadingCancelable: false
    readonly property color panelAccent: Theme.accent
    property int gridIconSize: 48
    property int gridIconMinSize: 32
    property int gridIconMaxSize: 96
    property int briefRowHeight: 28
    property int briefRowMinHeight: 22
    property int briefRowMaxHeight: 64
    property var loadingFolderNameProvider
    property int storageRevision: 0
    property string deviceRootPrimaryStatus: ""
    property string deviceRootSecondaryStatus: ""
    property string deviceRootStorageText: ""
    property string deviceRootStorageTooltip: ""
    property real deviceRootUsagePercent: 0
    property bool deviceRootStorageCritical: false

    signal gridIconSizeRequested(int value)
    signal briefRowHeightRequested(int value)
    signal cancelLoadingRequested()

    Connections {
        target: root.placesModel
        ignoreUnknownSignals: true
        function onDataChanged() { root.storageRevision += 1 }
        function onModelReset() { root.storageRevision += 1 }
        function onRowsInserted() { root.storageRevision += 1 }
        function onRowsRemoved() { root.storageRevision += 1 }
    }

    readonly property int pathRole: Qt.UserRole + 2
    readonly property int isDriveRole: Qt.UserRole + 4
    readonly property int totalSpaceRole: Qt.UserRole + 5
    readonly property int freeSpaceRole: Qt.UserRole + 6
    readonly property int usagePercentRole: Qt.UserRole + 8
    readonly property int isReadyRole: Qt.UserRole + 11
    readonly property int isCriticalRole: Qt.UserRole + 12
    readonly property int driveIndex: {
        storageRevision
        findDriveIndex()
    }
    readonly property bool hasDrive: driveIndex >= 0
    readonly property real totalSpace: {
        storageRevision
        hasDrive ? Number(placesModel.data(placesModel.index(driveIndex, 0), totalSpaceRole)) : 0
    }
    readonly property real freeSpace: {
        storageRevision
        hasDrive ? Number(placesModel.data(placesModel.index(driveIndex, 0), freeSpaceRole)) : 0
    }
    readonly property real usagePercent: {
        storageRevision
        hasDrive ? Number(placesModel.data(placesModel.index(driveIndex, 0), usagePercentRole)) : 0
    }
    readonly property bool driveReady: {
        storageRevision
        hasDrive ? !!placesModel.data(placesModel.index(driveIndex, 0), isReadyRole) : false
    }
    readonly property bool driveCritical: {
        storageRevision
        hasDrive ? !!placesModel.data(placesModel.index(driveIndex, 0), isCriticalRole) : false
    }
    readonly property bool zoomVisible: viewMode === 1 || viewMode === 2
    readonly property bool hasLoadingProgress: showLoadingRail && loadingProgress >= 0
    readonly property int zoomValue: viewMode === 1 ? gridIconSize : briefRowHeight
    readonly property int zoomMin: viewMode === 1 ? gridIconMinSize : briefRowMinHeight
    readonly property int zoomMax: viewMode === 1 ? gridIconMaxSize : briefRowMaxHeight
    readonly property int zoomStep: viewMode === 1 ? 4 : 2

    implicitHeight: 32
    color: Theme.panelSurfaceStrong
    border.color: active ? Theme.withAlpha(Theme.activeAccent, 0.34) : Theme.panelBorder
    border.width: 1

    function normalizePath(path) {
        let value = String(path || "").replace(/\\/g, "/")
        if (value.length >= 2 && value.charAt(1) === ":") {
            value = value.charAt(0).toUpperCase() + value.slice(1)
        }
        if (value.length === 2 && value.charAt(1) === ":") {
            value += "/"
        }
        return value
    }

    function driveRoot(path) {
        const value = normalizePath(path)
        if (value.length >= 3 && value.charAt(1) === ":" && value.charAt(2) === "/") {
            return value.slice(0, 3)
        }
        return value
    }

    function findDriveIndex() {
        if (!placesModel || !currentPath || String(currentPath).indexOf("archive://") === 0) {
            return -1
        }

        const current = normalizePath(currentPath)
        const rootPath = driveRoot(current)
        const rows = placesModel.rowCount()
        let bestIndex = -1
        let bestLength = 0

        for (let i = 0; i < rows; ++i) {
            const idx = placesModel.index(i, 0)
            if (!placesModel.data(idx, isDriveRole)) {
                continue
            }

            const drivePath = driveRoot(placesModel.data(idx, pathRole))
            if (drivePath.length === 0) {
                continue
            }

            if ((current === drivePath || current.indexOf(drivePath) === 0 || rootPath === drivePath)
                    && drivePath.length > bestLength) {
                bestIndex = i
                bestLength = drivePath.length
            }
        }

        return bestIndex
    }

    function formatBytes(bytes) {
        const value = Number(bytes)
        if (!isFinite(value) || value <= 0) return "-"
        const tb = 1024 * 1024 * 1024 * 1024
        const gb = 1024 * 1024 * 1024
        const mb = 1024 * 1024
        if (value >= tb) return (value / tb).toFixed(2) + " TB"
        if (value >= gb) return (value / gb).toFixed(1) + " GB"
        if (value >= mb) return Math.round(value / mb) + " MB"
        return Math.round(value / 1024) + " KB"
    }

    function statusText() {
        if (favoritesRootMode) {
            return "Favorites"
        }
        if (deviceRootMode) {
            return deviceRootPrimaryStatus
        }
        if (showLoadingRail) {
            if (root.loadingProgressText.length > 0) {
                return root.loadingProgressText
            }
            return isCurrentPathArchive ? "Loading archive..." : "Scanning folder"
        }
        if (statusMessage.length > 0) {
            return statusMessage
        }
        if (!controller || !controller.directoryModel) {
            return ""
        }

        const selected = controller.directoryModel.selectedCount
        const count = controller.directoryModel.count
        if (selected > 0) {
            return selected + " selected of " + count
        }
        return count + (count === 1 ? " item" : " items")
    }

    function secondaryStatusText() {
        if (favoritesRootMode) {
            return "Pinned " + root.favoritesPinnedCount
                    + " - Frequent " + root.favoritesFrequentCount
                    + " - Tags " + root.favoritesTagCount
        }
        if (deviceRootMode) {
            return deviceRootSecondaryStatus
        }
        if (!showLoadingRail) {
            return ""
        }
        if (loadingFolderNameProvider) {
            return "Reading items from " + loadingFolderNameProvider()
        }
        return "Reading items"
    }

    function storageText() {
        if (favoritesRootMode) {
            return ""
        }
        if (deviceRootMode) {
            return deviceRootStorageText
        }
        if (root.hasDrive && root.driveReady && root.totalSpace > 0) {
            return root.formatBytes(root.freeSpace) + " free"
        }
        return "Size unavailable"
    }

    function storageTooltipText() {
        if (favoritesRootMode) {
            return "Storage usage is not shown for virtual Favorites"
        }
        if (deviceRootMode) {
            return deviceRootStorageTooltip
        }
        if (root.hasDrive && root.driveReady && root.totalSpace > 0) {
            return root.formatBytes(root.freeSpace) + " free of " + root.formatBytes(root.totalSpace)
        }
        return "Storage size is unavailable for this location"
    }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            color: root.panelAccent
            visible: root.active
        }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 10
        spacing: 12

        BusyIndicator {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            running: root.showLoadingRail
            visible: running
        }

        Rectangle {
            Layout.preferredWidth: 7
            Layout.preferredHeight: 7
            radius: 4
            visible: !root.showLoadingRail && root.statusMessage.length > 0
            color: root.panelAccent
            opacity: 0.9
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 0

            Label {
                Layout.fillWidth: true
                text: root.statusText()
                color: Theme.textPrimary
                font.pixelSize: 11
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            Label {
                Layout.fillWidth: true
                visible: root.showLoadingRail || root.deviceRootMode
                         || root.favoritesRootMode
                text: root.secondaryStatusText()
                color: Theme.textSecondary
                font.pixelSize: 10
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.65 : 0.85)
            opacity: 0.9
        }

        RowLayout {
            Layout.preferredWidth: 188
            Layout.maximumWidth: 188
            Layout.minimumWidth: 136
            visible: !root.favoritesRootMode
            spacing: 8

            Label {
                Layout.preferredWidth: 84
                text: root.storageText()
                color: (root.deviceRootMode ? root.deviceRootStorageCritical : root.driveCritical)
                       ? Theme.danger : Theme.textSecondary
                font.pixelSize: 10
                elide: Text.ElideRight
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 8

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 4
                    radius: 2
                    color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.55 : 0.65)
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.deviceRootMode
                           ? (root.deviceRootUsagePercent > 0
                                  ? Math.max(4, Math.min(1, Math.max(0, root.deviceRootUsagePercent)) * parent.width)
                                  : 0)
                           : (root.hasDrive && root.driveReady && root.totalSpace > 0
                                  ? Math.max(4, Math.min(1, Math.max(0, root.usagePercent)) * parent.width)
                                  : 0)
                    height: 4
                    radius: 2
                    color: (root.deviceRootMode ? root.deviceRootStorageCritical : root.driveCritical)
                           ? Theme.danger : root.panelAccent

                    Behavior on width {
                        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                    }
                }
            }

            ToolTip.visible: storageHover.hovered
            ToolTip.text: root.storageTooltipText()

            HoverHandler {
                id: storageHover
            }
        }

        Rectangle {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 22
            radius: 4
            visible: root.showLoadingRail && root.loadingCancelable
            color: cancelMouse.pressed
                   ? Theme.withAlpha(Theme.danger, themeController.isDark ? 0.20 : 0.14)
                   : (cancelMouse.hovered
                      ? Theme.withAlpha(Theme.danger, themeController.isDark ? 0.13 : 0.08)
                      : Theme.panelSurfaceSoft)
            border.color: Theme.withAlpha(Theme.danger, cancelMouse.hovered ? 0.62 : 0.38)
            border.width: 1

            Label {
                anchors.centerIn: parent
                text: "Cancel"
                color: cancelMouse.hovered ? Theme.danger : Theme.textPrimary
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: cancelMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.cancelLoadingRequested()
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            visible: root.zoomVisible
                     && !root.favoritesRootMode
            color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.65 : 0.85)
            opacity: 0.9
        }

        Slider {
            id: zoomSlider
            Layout.preferredWidth: 104
            Layout.preferredHeight: 22
            visible: root.zoomVisible
                     && !root.favoritesRootMode
            from: root.zoomMin
            to: root.zoomMax
            stepSize: root.zoomStep
            snapMode: Slider.SnapAlways
            value: root.zoomValue
            focusPolicy: Qt.StrongFocus

            onMoved: {
                const snapped = Math.round(value / stepSize) * stepSize
                if (root.viewMode === 1) {
                    root.gridIconSizeRequested(snapped)
                } else if (root.viewMode === 2) {
                    root.briefRowHeightRequested(snapped)
                }
            }

            background: Item {
                anchors.fill: parent

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 4
                    radius: 2
                    color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.58 : 0.62)
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: zoomSlider.visualPosition * parent.width
                    height: 4
                    radius: 2
                    color: Theme.accent
                }
            }

            handle: Rectangle {
                x: zoomSlider.leftPadding + zoomSlider.visualPosition * (zoomSlider.availableWidth - width)
                y: zoomSlider.topPadding + zoomSlider.availableHeight / 2 - height / 2
                width: 10
                height: 10
                radius: 5
                color: zoomSlider.pressed ? Theme.accent : Theme.panelSurface
                border.color: Theme.accent
                border.width: 1
            }

            ToolTip.visible: hovered || pressed
            ToolTip.text: root.viewMode === 1
                          ? "Icon size: " + root.gridIconSize + " px"
                          : "Density: " + root.briefRowHeight + " px"
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 3
        visible: root.hasLoadingProgress
        color: Theme.withAlpha(root.panelAccent, themeController.isDark ? 0.16 : 0.10)

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * Math.max(0, Math.min(1, root.loadingProgress))
            color: root.panelAccent

            Behavior on width {
                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
            }
        }
    }
}
