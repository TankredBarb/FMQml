import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Item {
    id: root

    required property var controller
    property int currentDriveIndex: -1

    // ── Helper functions ──────────────────────────────────────────────────────

    function driveIconSource(driveType) {
        // All icons are mapped to available assets
        switch (String(driveType)) {
        case "usb":     return "../assets/icons/hard-drive.svg"
        case "optical": return "../assets/icons/hard-drive.svg"
        case "network": return "../assets/icons/hard-drive.svg"
        default:        return "../assets/icons/hard-drive.svg"
        }
    }

    function driveIconColor(driveType) {
        switch (String(driveType)) {
        case "usb":     return "#22c55e"
        case "optical": return "#f59e0b"
        case "network": return "#8b5cf6"
        case "ssd":     return "#06b6d4"
        default:        return "#3b82f6"
        }
    }

    function driveTypeLabel(driveType) {
        switch (String(driveType)) {
        case "usb":     return "USB"
        case "optical": return "Optical"
        case "network": return "Network"
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
        return "../assets/icons/" + iconName + ".svg"
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
            if (m.data(m.index(i, 0), Qt.UserRole + 4 /* IsDriveRole */)) {
                sum += m.data(m.index(i, 0), Qt.UserRole + 5 /* TotalSpaceRole */)
            }
        }
        return sum
    }

    readonly property real freeSpaceSum: {
        var sum = 0
        var m = workspaceController.placesModel
        for (var i = 0; i < m.rowCount(); i++) {
            if (m.data(m.index(i, 0), Qt.UserRole + 4 /* IsDriveRole */)) {
                sum += m.data(m.index(i, 0), Qt.UserRole + 6 /* FreeSpaceRole */)
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
                    color: themeController.isDark ? "#1a2333" : "#dbe2e9"
                }
                GradientStop {
                    position: 1.0
                    color: themeController.isDark ? "#111520" : "#edf2f6"
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
                        source: "../assets/icons/computer.svg"
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
                    color: Theme.border
                    opacity: 0.35
                }
            }

            // ── Premium Dashboard Card ────────────────────────────────────────
            Rectangle {
                id: dashboardCard
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 16
                Layout.bottomMargin: 20 + root.gapAmount
                height: 132
                radius: 12

                color: themeController.isDark
                    ? Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.45)
                    : Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.55)

                border.color: Theme.border
                border.width: 1

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

                            Rectangle {
                                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                                radius: 4
                                implicitWidth: osLabel.implicitWidth + 12
                                implicitHeight: 18

                                Label {
                                    id: osLabel
                                    anchors.centerIn: parent
                                    text: systemInfoProvider.osName
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: Theme.accent
                                }
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

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: 6

                            readonly property real usage: root.totalSpaceSum > 0 ? (root.totalSpaceSum - root.freeSpaceSum) / root.totalSpaceSum : 0.0

                            Rectangle {
                                anchors.fill: parent
                                radius: 3
                                color: themeController.isDark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(0,0,0,0.05)
                            }

                            Rectangle {
                                radius: 3
                                height: parent.height
                                width: parent.width * parent.usage
                                color: parent.usage > 0.90 ? "#ef4444" : (parent.usage > 0.75 ? "#f59e0b" : Theme.accent)

                                Behavior on width { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                            }
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
                    model: workspaceController.placesModel
                    delegate: Item {
                        id: cardWrapper
                        width: model.isDrive ? flowLayout.cardW : 0
                        height: model.isDrive ? 108 : 0
                        visible: model.isDrive

                        Rectangle {
                            id: card
                            anchors.fill: parent
                            radius: 10
                            scale: cardMouse.containsMouse ? 1.02 : (cardWrapper.isSelected ? 1.01 : 1.0)
                            y: cardMouse.containsMouse ? -2 : 0

                            color: {
                                if (themeController.isDark) {
                                    if (cardMouse.containsMouse) return Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.88)
                                    return Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.4)
                                } else {
                                    if (cardMouse.containsMouse) return Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.96)
                                    return Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.45)
                                }
                            }

                            border.color: {
                                if (cardWrapper.isSelected) {
                                    return Theme.accent
                                }
                                if (cardMouse.containsMouse) {
                                    return themeController.isDark 
                                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5) 
                                        : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4)
                                }
                                return Theme.border
                            }
                            border.width: cardWrapper.isSelected ? 1.5 : 1

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: themeController.isDark
                                    ? Qt.rgba(0, 0, 0, cardMouse.containsMouse ? 0.4 : (cardWrapper.isSelected ? 0.35 : 0.15))
                                    : Qt.rgba(0, 0, 0, cardMouse.containsMouse ? 0.12 : (cardWrapper.isSelected ? 0.1 : 0.04))
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

                                    Rectangle {
                                        width: 44
                                        height: 44
                                        radius: 10
                                        anchors.centerIn: parent
                                        color: Qt.rgba(
                                            Qt.color(root.driveIconColor(model.driveType)).r,
                                            Qt.color(root.driveIconColor(model.driveType)).g,
                                            Qt.color(root.driveIconColor(model.driveType)).b,
                                            (themeController.isDark ? 0.18 : 0.12) + (cardMouse.containsMouse ? 0.08 : 0))

                                        Behavior on color { ColorAnimation { duration: Theme.motionFast } }

                                        Image {
                                            anchors.centerIn: parent
                                            width: 24
                                            height: 24
                                            source: root.driveIconSource(model.driveType)
                                            sourceSize: Qt.size(24, 24)
                                            asynchronous: true
                                            cache: true
                                            layer.enabled: true
                                            layer.effect: MultiEffect {
                                                colorization: 1.0
                                                colorizationColor: root.driveIconColor(model.driveType)
                                            }
                                        }
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
                                            text: model.name || model.path
                                            font.pixelSize: 13
                                            font.bold: true
                                            color: Theme.textPrimary
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        // FS badge
                                        Rectangle {
                                            implicitWidth: fsBadgeText.implicitWidth + 8
                                            implicitHeight: 17
                                            radius: 4
                                            visible: model.fileSystem && model.fileSystem.length > 0
                                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                                           themeController.isDark ? 0.18 : 0.12)

                                            Label {
                                                id: fsBadgeText
                                                anchors.centerIn: parent
                                                text: model.fileSystem || ""
                                                font.pixelSize: 9
                                                font.bold: true
                                                font.letterSpacing: 0.5
                                                color: Theme.accent
                                            }
                                        }
                                    }

                                    // Free space text
                                    Label {
                                        text: model.isReady
                                            ? (root.formatBytes(model.freeSpace) + " free of " + root.formatBytes(model.totalSpace))
                                            : "Not ready"
                                        font.pixelSize: 11
                                        color: model.isCritical ? "#ef4444" : Theme.textSecondary
                                        opacity: 0.88
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // Progress bar
                                    Item {
                                        Layout.fillWidth: true
                                        implicitHeight: 6

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 3
                                            color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b,
                                                           themeController.isDark ? 0.3 : 0.5)
                                        }

                                        Rectangle {
                                            id: progressFill
                                            anchors.left: parent.left
                                            anchors.top: parent.top
                                            anchors.bottom: parent.bottom
                                            radius: 3
                                            width: model.isReady
                                                ? Math.max(radius * 2, parent.width * Math.min(1.0, model.usagePercent))
                                                : 0

                                            color: root.progressColor(model.usagePercent, model.isCritical)

                                            Behavior on width { NumberAnimation  { duration: 400; easing.type: Easing.OutCubic } }
                                            Behavior on color { ColorAnimation   { duration: Theme.motionFast } }
                                        }
                                    }

                                    // Drive type tag + percent row
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Label {
                                            text: root.driveTypeLabel(model.driveType)
                                            font.pixelSize: 10
                                            font.bold: true
                                            font.letterSpacing: 0.8
                                            color: root.driveIconColor(model.driveType)
                                            opacity: 0.82
                                        }

                                        Item { Layout.fillWidth: true }

                                        // Warning icon for critical
                                        Label {
                                            text: "⚠"
                                            font.pixelSize: 11
                                            color: "#ef4444"
                                            visible: model.isCritical
                                        }

                                        Label {
                                            text: model.isReady
                                                ? (Math.round(model.usagePercent * 100) + "% used")
                                                : "—"
                                            font.pixelSize: 10
                                            color: model.isCritical ? "#ef4444" : Theme.textSecondary
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
                                    if (!model.isDrive) return
                                    root.currentDriveIndex = index
                                    if (mouse.button === Qt.RightButton) {
                                        driveContextMenu.driveIndex = index
                                        driveContextMenu.drivePath  = model.path
                                        driveContextMenu.driveType  = model.driveType
                                        driveContextMenu.popup()
                                    }
                                }

                                onDoubleClicked: function(mouse) {
                                    if (!model.isDrive || !model.isReady) return
                                    root.controller.openPath(model.path)
                                }
                            }
                        }

                        // Card appear animation
                        opacity: 0
                        Component.onCompleted: {
                            if (model.isDrive) {
                                appearAnim.start()
                            } else {
                                opacity = 0
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

                        property bool isSelected: root.currentDriveIndex === index
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
                        radius: 2
                        color: Theme.accent
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
                    model: workspaceController.placesModel
                    delegate: Item {
                        id: folderCardWrapper
                        width: !model.isDrive ? quickAccessFlow.cardW : 0
                        height: !model.isDrive ? 68 : 0
                        visible: !model.isDrive

                        Rectangle {
                            id: folderCard
                            anchors.fill: parent
                            radius: 8
                            scale: folderMouse.containsMouse ? 1.02 : 1.0
                            y: folderMouse.containsMouse ? -2 : 0

                            color: {
                                if (themeController.isDark) {
                                    if (folderMouse.containsMouse) return Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.88)
                                    return Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.4)
                                } else {
                                    if (folderMouse.containsMouse) return Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.96)
                                    return Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.45)
                                }
                            }

                            border.color: folderMouse.containsMouse
                                ? (themeController.isDark ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5) : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4))
                                : Theme.border
                            border.width: 1

                            Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                            Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
                            Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                            Behavior on y { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: themeController.isDark
                                    ? Qt.rgba(0, 0, 0, folderMouse.containsMouse ? 0.35 : 0.12)
                                    : Qt.rgba(0, 0, 0, folderMouse.containsMouse ? 0.1 : 0.03)
                                shadowBlur: folderMouse.containsMouse ? 0.6 : 0.3
                                shadowVerticalOffset: folderMouse.containsMouse ? 4 : 2
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10

                                Rectangle {
                                    width: 32
                                    height: 32
                                    radius: 6
                                    color: Qt.rgba(
                                        Qt.color(root.folderIconColor(model.icon)).r,
                                        Qt.color(root.folderIconColor(model.icon)).g,
                                        Qt.color(root.folderIconColor(model.icon)).b,
                                        (themeController.isDark ? 0.15 : 0.1) + (folderMouse.containsMouse ? 0.08 : 0))

                                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }

                                    Image {
                                        anchors.centerIn: parent
                                        width: 16
                                        height: 16
                                        source: !model.isDrive ? root.folderIconSource(model.icon) : ""
                                        sourceSize: Qt.size(16, 16)
                                        layer.enabled: !model.isDrive
                                        layer.effect: MultiEffect {
                                            colorization: 1.0
                                            colorizationColor: root.folderIconColor(model.icon)
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Label {
                                        text: model.name
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

                                onClicked: function() {
                                    root.controller.openPath(model.path)
                                }
                            }
                        }

                        // Staggered fade-in/slide-up animation
                        opacity: 0
                        y: 10
                        Component.onCompleted: {
                            if (!model.isDrive) {
                                folderAppearAnim.start()
                            } else {
                                opacity = 0
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

    ThemedContextMenu {
        id: driveContextMenu

        property int    driveIndex: -1
        property string drivePath:  ""
        property string driveType:  ""

        ThemedMenuItem {
            text: "Open"
            icon.source: "../assets/icons/folder-plus.svg"
            iconColor: "#22c55e"
            onTriggered: root.controller.openPath(driveContextMenu.drivePath)
        }

        ThemedMenuSeparator {}

        ThemedMenuItem {
            text: "Eject"
            icon.source: "../assets/icons/arrow-up.svg"
            iconColor: "#f59e0b"
            visible: driveContextMenu.driveType === "usb" || driveContextMenu.driveType === "optical"
            enabled: visible
            onTriggered: root.controller.ejectDrive(driveContextMenu.drivePath)
        }

        ThemedMenuSeparator {
            visible: driveContextMenu.driveType === "usb" || driveContextMenu.driveType === "optical"
        }

        ThemedMenuItem {
            text: "Properties"
            icon.source: "../assets/icons/info.svg"
            iconColor: "#0ea5e9"
            onTriggered: propertiesController.load(driveContextMenu.drivePath)
        }
    }

    // ── Keyboard navigation ───────────────────────────────────────────────────

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            var idx = root.currentDriveIndex
            if (idx >= 0) {
                var m = workspaceController.placesModel
                var path = m.data(m.index(idx, 0), Qt.UserRole + 2) // PathRole
                if (path) root.controller.openPath(path)
            }
            event.accepted = true
        }
    }
}
