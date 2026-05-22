import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQml.Models
import "../style"

Pane {
    id: root

    padding: 0

    function syncTreeToActivePath() {
        syncTimer.restart()
    }

    Timer {
        id: syncTimer
        interval: 120
        repeat: false
        onTriggered: {
            let panel = workspaceController.activePanel === 0
                ? workspaceController.leftPanel
                : workspaceController.rightPanel
            
            if (!panel) return;

            // Handle virtual root (This PC)
            if (panel.isDeviceRoot) {
                if (foldersTree.selectionModel) {
                    foldersTree.selectionModel.clear()
                }
                return
            }

            let targetPath = panel.currentPath
            let index = workspaceController.treeModel.indexForPath(targetPath)
            
            if (!index || !index.valid) {
                if (foldersTree.selectionModel) {
                    foldersTree.selectionModel.clear()
                }
                return
            }

            // QML TreeView doesn't have expandToIndex. We must expand ancestors top-down.
            let current = index
            let ancestors = []
            while (current && current.valid) {
                let p = workspaceController.treeModel.parentIndex(current)
                if (p && p.valid) {
                    ancestors.unshift(p)
                    current = p
                } else {
                    break
                }
            }
            
            for (let i = 0; i < ancestors.length; ++i) {
                let r = foldersTree.rowAtIndex(ancestors[i])
                if (r >= 0) {
                    if (!foldersTree.isExpanded(r)) {
                        foldersTree.expand(r)
                        foldersTree.forceLayout() // MUST force layout so the next child's row can be resolved
                    }
                }
            }

            let finalRow = foldersTree.rowAtIndex(index)
            if (finalRow >= 0) {
                if (foldersTree.selectionModel) {
                    foldersTree.selectionModel.setCurrentIndex(index, 
                        ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows | ItemSelectionModel.Current)
                }
                // TreeView inherits from TableView, so we use positionViewAtRow
                foldersTree.positionViewAtRow(finalRow, TableView.Contain)
            } else {
                foldersTree.forceLayout()
                finalRow = foldersTree.rowAtIndex(index)
                if (finalRow >= 0) {
                    if (foldersTree.selectionModel) {
                        foldersTree.selectionModel.setCurrentIndex(index, 
                            ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows | ItemSelectionModel.Current)
                    }
                    foldersTree.positionViewAtRow(finalRow, TableView.Contain)
                }
            }
        }
    }

    function pathsEqual(lhs, rhs) {
        if (!lhs || !rhs) return false;
        
        // Strip trailing slashes
        let cleanLhs = lhs.replace(/[/\\]$/, "")
        let cleanRhs = rhs.replace(/[/\\]$/, "")

        if (Qt.platform.os === "windows") {
            let eq = cleanLhs.toLowerCase() === cleanRhs.toLowerCase()
            // console.log("[Sidebar] pathsEqual(win) lhs:", lhs, "rhs:", rhs, "eq:", eq) // too noisy, let's keep quiet for now or only log if close
            return eq
        }
        return cleanLhs === cleanRhs
    }

    function iconToneFor(name, active, hovered) {
        let base = Theme.textSecondary
        switch (String(name)) {
        case "computer":
            base = "#6366f1"
            break
        case "home":
            base = "#8b5cf6"
            break
        case "desktop":
            base = "#0ea5e9"
            break
        case "download":
            base = "#22c55e"
            break
        case "document":
            base = "#f59e0b"
            break
        case "image":
            base = "#ec4899"
            break
        case "music":
            base = "#a855f7"
            break
        case "video":
            base = "#ef4444"
            break
        case "drive":
        case "hard-drive":
            base = "#3b82f6"
            break
        case "folder":
        case "file-manager":
            base = "#22c55e"
            break
        default:
            base = Theme.accent
            break
        }

        if (active) {
            return Qt.lighter(base, themeController.isDark ? 1.12 : 1.05)
        }
        if (hovered) {
            return Qt.lighter(base, themeController.isDark ? 1.08 : 1.03)
        }
        return base
    }

    background: Rectangle {
        color: themeController.isDark ? Theme.surface : Theme.bg

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                          themeController.isDark ? 0.05 : 0.03)
        }

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            color: themeController.isDark
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                : Qt.rgba(1, 1, 1, 0.18)
        }

        border.color: themeController.isDark
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
            : Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.72)
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 8
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 7
            spacing: 10

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.92)
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
                border.width: 1
            }

            Label {
                text: "Places"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.2
                color: Theme.textPrimary
                opacity: 0.82
            }
        }

        ListView {
            id: placesList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 1
            model: workspaceController.placesModel
            clip: true
            interactive: contentHeight > height

            header: Item {
                width: placesList.width
                height: 40

                readonly property bool isActive: {
                    let panel = workspaceController.activePanel === 0
                        ? workspaceController.leftPanel
                        : workspaceController.rightPanel
                    return panel.isDeviceRoot
                }

                Rectangle {
                    id: thisPcBg
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    radius: 9

                    color: {
                        if (parent.isActive)
                            return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.16 : 0.11)
                        if (thisPcMouse.containsPress)
                            return Theme.surfaceActive
                        if (thisPcMouse.containsMouse)
                            return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.07 : 0.05)
                        return "transparent"
                    }

                    border.color: parent.isActive
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.32)
                        : (thisPcMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : "transparent")
                    border.width: parent.isActive || thisPcMouse.containsMouse ? 1 : 0

                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }

                    // Active indicator bar
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        anchors.leftMargin: 4
                        width: 3
                        radius: 1.5
                        visible: thisPcBg.parent.isActive
                        color: Theme.accent
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 6
                        spacing: 10

                        Image {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            source: "../assets/icons/computer.svg"
                            sourceSize: Qt.size(20, 20)
                            asynchronous: true
                            cache: true
                            opacity: thisPcBg.parent.isActive || thisPcMouse.containsMouse ? 1 : 0.86
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                colorization: 1.0
                                colorizationColor: root.iconToneFor("computer", thisPcBg.parent.isActive, thisPcMouse.containsMouse)
                            }
                        }

                        Label {
                            text: "This PC"
                            Layout.fillWidth: true
                            font.pixelSize: 13
                            font.weight: thisPcBg.parent.isActive ? Font.Medium : Font.Normal
                            color: Theme.textPrimary
                            opacity: thisPcBg.parent.isActive ? 1.0 : 0.92
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: thisPcMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            let panel = workspaceController.activePanel === 0
                                ? workspaceController.leftPanel
                                : workspaceController.rightPanel
                            panel.openPath("devices://")
                        }
                    }
                }
            }

            delegate: ItemDelegate {
                id: placeDelegate
                width: placesList.width
                height: 40
                padding: 0

                readonly property bool isActive: root.pathsEqual(model.path, (
                    workspaceController.activePanel === 0
                        ? workspaceController.leftPanel.currentPath
                        : workspaceController.rightPanel.currentPath
                ))

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 12
                    spacing: 10

                    Image {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        source: model.icon === "drive"
                                ? "../assets/icons/hard-drive.svg"
                                : "../assets/icons/" + model.icon + ".svg"
                        sourceSize: Qt.size(20, 20)
                        asynchronous: true
                        cache: true
                        opacity: isActive || placeDelegate.hovered ? 1 : 0.86
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            colorization: 1.0
                            colorizationColor: root.iconToneFor(model.icon, isActive, placeDelegate.hovered)
                        }
                    }

                    Label {
                        text: model.name
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        font.weight: isActive ? Font.Medium : Font.Normal
                        color: Theme.textPrimary
                        opacity: isActive ? 1.0 : 0.92
                        elide: Text.ElideRight
                    }
                }

                background: Rectangle {
                    radius: 9
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6

                    color: {
                        if (isActive)
                            return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.16 : 0.11)
                        if (placeDelegate.down)
                            return Theme.surfaceActive
                        if (placeDelegate.hovered)
                            return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.07 : 0.05)
                        return "transparent"
                    }

                    border.color: isActive ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.32)
                                            : (placeDelegate.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : "transparent")
                    border.width: isActive || placeDelegate.hovered ? 1 : 0

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        anchors.leftMargin: 4
                        width: 3
                        radius: 1.5
                        visible: isActive
                        color: Theme.accent
                    }

                    Behavior on color {
                        ColorAnimation { duration: Theme.motionFast }
                    }
                }

                onClicked: {
                    let panel = workspaceController.activePanel === 0
                        ? workspaceController.leftPanel
                        : workspaceController.rightPanel
                    panel.openPath(model.path)
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.topMargin: 10
            Layout.bottomMargin: 10
            color: Theme.border
            opacity: 0.95
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 7
            spacing: 10

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.92)
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
                border.width: 1
            }

            Label {
                text: "Folders"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.2
                color: Theme.textPrimary
                opacity: 0.82
            }
        }

        TreeView {
            id: foldersTree
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 1
            model: workspaceController.treeModel
            clip: true

            delegate: ItemDelegate {
                id: folderDelegate
                required property TreeView treeView
                required property int row
                required property bool isTreeNode
                required property bool expanded
                required property bool hasChildren
                required property int depth

                width: foldersTree.width
                implicitWidth: foldersTree.width > 0 ? foldersTree.width : 1
                implicitHeight: 40
                height: implicitHeight
                padding: 0
                focusPolicy: Qt.NoFocus

                readonly property bool isActive: root.pathsEqual(model.path, (
                    workspaceController.activePanel === 0
                        ? workspaceController.leftPanel.currentPath
                        : workspaceController.rightPanel.currentPath
                ))
                readonly property real baseIndent: 14
                readonly property real indentStep: 20
                readonly property real indicatorSlot: 18
                readonly property real iconSize: 20

                background: Rectangle {
                    radius: 9
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6

                    color: {
                        if (isActive)
                            return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.18 : 0.12)
                        if (rowMouse.down)
                            return Theme.surfaceActive
                        if (rowMouse.containsMouse)
                            return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.07 : 0.05)
                        return "transparent"
                    }

                    border.color: isActive ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.34)
                                            : (rowMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20) : "transparent")
                    border.width: isActive || rowMouse.containsMouse ? 1 : 0

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        anchors.leftMargin: 4
                        width: (isActive || rowMouse.containsMouse) ? 3 : 0
                        radius: 1.5
                        color: isActive ? Theme.accent : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55)
                        
                        Behavior on width { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutQuad } }
                    }

                    Behavior on color {
                        ColorAnimation { duration: Theme.motionFast }
                    }
                }

                contentItem: Item {
                    anchors.fill: parent

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        z: 1
                        onClicked: function(mouse) {
                            panel.openPath(model.path)
                            mouse.accepted = true
                        }
                    }

                    Rectangle {
                        id: depthGuide
                        visible: folderDelegate.isTreeNode && folderDelegate.depth > 0
                        x: folderDelegate.baseIndent + (folderDelegate.depth * folderDelegate.indentStep) - 8
                        y: 4
                        width: 1
                        height: parent.height - 8
                        color: Theme.border
                        opacity: folderDelegate.isActive ? 0.40 : (rowMouse.containsMouse ? 0.34 : 0.24)
                    }

                    Item {
                        id: disclosureArea
                        z: 2
                        x: folderDelegate.baseIndent + (folderDelegate.isTreeNode ? folderDelegate.depth * folderDelegate.indentStep : 0)
                        y: 0
                        width: folderDelegate.indicatorSlot
                        height: parent.height
                        visible: folderDelegate.isTreeNode && folderDelegate.hasChildren
                        opacity: folderDelegate.isActive ? 1 : (rowMouse.containsMouse ? 0.96 : 0.78)

                        Canvas {
                            id: chevronCanvas
                            anchors.centerIn: parent
                            width: 12
                            height: 12
                            rotation: folderDelegate.expanded ? 90 : 0
                            opacity: folderDelegate.hasChildren ? 1 : 0.35
                            
                            Behavior on rotation {
                                NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutQuad }
                            }

                            Behavior on opacity {
                                NumberAnimation { duration: Theme.motionFast }
                            }
                            
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                ctx.strokeStyle = folderDelegate.isActive || rowMouse.containsMouse
                                    ? Theme.textPrimary
                                    : Theme.textSecondary;
                                ctx.lineWidth = 1.25;
                                ctx.lineCap = "round";
                                ctx.lineJoin = "round";
                                ctx.beginPath();
                                ctx.moveTo(4, 2.5);
                                ctx.lineTo(7.5, 6);
                                ctx.lineTo(4, 9.5);
                                ctx.stroke();
                            }
                            
                            Connections {
                                target: folderDelegate
                                function onIsActiveChanged() { chevronCanvas.requestPaint(); }
                            }
                            Connections {
                                target: rowMouse
                                function onContainsMouseChanged() { chevronCanvas.requestPaint(); }
                            }
                            Connections {
                                target: themeController
                                function onThemeChanged() { chevronCanvas.requestPaint(); }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: function(mouse) {
                                folderDelegate.treeView.toggleExpanded(folderDelegate.row)
                                mouse.accepted = true
                            }
                        }
                    }

                    Item {
                        id: rowArea
                        z: 1
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: folderDelegate.baseIndent
                            + (folderDelegate.isTreeNode ? folderDelegate.depth * folderDelegate.indentStep : 0)
                            + folderDelegate.indicatorSlot + 8
                        anchors.rightMargin: 12

                        RowLayout {
                            anchors.fill: parent
                            spacing: 10

                            Image {
                                Layout.preferredWidth: folderDelegate.iconSize
                                Layout.preferredHeight: folderDelegate.iconSize
                                source: model.icon ? (model.icon === "drive"
                                        ? "../assets/icons/hard-drive.svg"
                                        : "../assets/icons/" + model.icon + ".svg") : ""
                                sourceSize: Qt.size(folderDelegate.iconSize, folderDelegate.iconSize)
                                asynchronous: true
                                cache: true
                                opacity: folderDelegate.isActive || rowMouse.containsMouse ? 1 : 0.84
                                layer.enabled: true
                                layer.effect: MultiEffect {
                                    colorization: 1.0
                                    colorizationColor: root.iconToneFor(model.icon, folderDelegate.isActive, rowMouse.containsMouse)
                                }
                            }

                            Label {
                                text: model.name || ""
                                Layout.fillWidth: true
                                font.pixelSize: 13
                                font.letterSpacing: 0.2
                                font.weight: isActive || rowMouse.containsMouse ? Font.Medium : Font.Normal
                                color: Theme.textPrimary
                                opacity: isActive || rowMouse.containsMouse ? 1.0 : 0.92
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                readonly property var panel: workspaceController.activePanel === 0
                    ? workspaceController.leftPanel
                    : workspaceController.rightPanel
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
    }

    Connections {
        target: workspaceController
        function onActivePanelChanged() {
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onCurrentPathChanged() {
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onPathNavigated() {
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onCurrentPathChanged() {
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onPathNavigated() {
            root.syncTreeToActivePath()
        }
    }

    Component.onCompleted: syncTreeToActivePath()
}
