import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Control {
    id: root

    property var controller
    required property string path
    property bool readOnly: true
    property bool backgroundVisible: false

    signal editRequested()

    implicitHeight: 36

    function focusPath() {
        root.forceActiveFocus()
    }

    // Helper to match specific folders with custom icons
    function getFolderIcon(name, isDrive, isThisPc) {
        if (isThisPc) return "../assets/icons/computer.svg";
        if (isDrive) return "../assets/icons/hard-drive.svg";
        
        let lower = name.toLowerCase();
        if (lower === "desktop") return "../assets/icons/desktop.svg";
        if (lower === "downloads") return "../assets/icons/download.svg";
        if (lower === "documents") return "../assets/icons/document.svg";
        if (lower === "pictures" || lower === "images") return "../assets/filetypes/image.svg";
        if (lower === "music") return "../assets/icons/music.svg";
        if (lower === "videos" || lower === "movies") return "../assets/icons/video.svg";
        
        return "../assets/icons/folder.svg";
    }

    // True when the active panel is showing the virtual devices:// root
    readonly property bool deviceRootMode: root.controller ? root.controller.isDeviceRoot : false
    readonly property bool favoritesRootMode: root.controller ? root.controller.isFavoritesRoot : false
    readonly property bool virtualRootMode: root.deviceRootMode || root.favoritesRootMode

    background: Rectangle {
        visible: root.backgroundVisible
        color: themeController.isDark ? Theme.surface : Theme.bg
        radius: Theme.radius
        border.color: root.activeFocus ? Theme.accent : Theme.border
        border.width: root.activeFocus ? 2 : 1
    }

    contentItem: Item {
        id: container
        clip: true

        Flickable {
            id: flickable
            anchors.fill: parent
            contentWidth: breadcrumbsRow.width + 16
            contentHeight: parent.height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            // Automatically scroll to the end (deepest directory) when path or width changes
            onContentWidthChanged: {
                if (contentWidth > width) {
                    contentX = contentWidth - width
                } else {
                    contentX = 0
                }
            }
            onWidthChanged: {
                if (contentWidth > width) {
                    contentX = contentWidth - width
                } else {
                    contentX = 0
                }
            }

            Row {
                id: breadcrumbsRow
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: 8
                rightPadding: 8
                spacing: 4

                // ── "This PC" crumb ──
                ToolButton {
                    id: thisPcCrumb
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.favoritesRootMode
                    padding: 6
                    leftPadding: 7
                    rightPadding: 7
                    implicitWidth: 30
                    implicitHeight: 28
                    
                    contentItem: Item {
                        Image {
                            id: thisPcIcon
                            anchors.centerIn: parent
                            source: "../assets/icons/computer.svg"
                            width: 14
                            height: 14
                            sourceSize: Qt.size(28, 28)
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                colorization: 1.0
                                colorizationColor: root.deviceRootMode ? Theme.accent : Theme.textSecondary
                            }
                        }
                    }

                    ToolTip.visible: hovered
                    ToolTip.text: "This PC"
                    
                    background: Rectangle {
                        color: thisPcCrumb.down 
                               ? Theme.surfaceActive 
                               : (thisPcCrumb.hovered ? Theme.itemHoverFill : "transparent")
                        radius: 6
                        
                        Behavior on color { ColorAnimation { duration: 100 } }
                    }
                    
                    onClicked: {
                        if (root.controller && !root.deviceRootMode) {
                            root.focusPath()
                            Qt.callLater(() => {
                                root.controller.openPath("devices://")
                            })
                        }
                    }
                }

                // ── Separator (only if not at devices://) ──
                ToolButton {
                    id: favoritesCrumb
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.favoritesRootMode
                    padding: 6
                    leftPadding: 7
                    rightPadding: 7
                    implicitWidth: 30
                    implicitHeight: 28

                    contentItem: Item {
                        Image {
                            anchors.centerIn: parent
                            source: "../assets/icons/star.svg"
                            width: 14
                            height: 14
                            sourceSize: Qt.size(28, 28)
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                colorization: 1.0
                                colorizationColor: Theme.accent
                            }
                        }
                    }

                    ToolTip.visible: hovered
                    ToolTip.text: "Favorites"

                    background: Rectangle {
                        color: favoritesCrumb.down
                               ? Theme.surfaceActive
                               : (favoritesCrumb.hovered ? Theme.itemHoverFill : "transparent")
                        radius: 6
                    }

                    onClicked: {
                        if (root.controller && !root.favoritesRootMode) {
                            root.focusPath()
                            Qt.callLater(() => {
                                root.controller.openPath("favorites://")
                            })
                        }
                    }
                }

                Item {
                    id: separatorThisPc
                    width: 16
                    height: 24
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.virtualRootMode

                    readonly property bool interactive: root.readOnly

                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: Theme.itemHoverFill
                        opacity: separatorThisPc.interactive && thisPcSepMouseArea.containsMouse ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    Label {
                        text: "\u203A"
                        anchors.centerIn: parent
                        color: separatorThisPc.interactive && thisPcSepMouseArea.containsMouse ? Theme.accent : Theme.textSecondary
                        font.pixelSize: 16
                        font.bold: true
                        opacity: separatorThisPc.interactive && thisPcSepMouseArea.containsMouse ? 1.0 : 0.6
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    MouseArea {
                        id: thisPcSepMouseArea
                        anchors.fill: parent
                        hoverEnabled: separatorThisPc.interactive
                        cursorShape: separatorThisPc.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
                        enabled: separatorThisPc.interactive
                        onClicked: root.openMenu("devices://", separatorThisPc)
                    }
                }

                // ── Path Segments ──
                Repeater {
                    id: pathRepeater
                    visible: !root.virtualRootMode
                    model: {
                        if (root.virtualRootMode) return []
                        if (!root.controller || !root.controller.breadcrumbEntriesForPath) return []
                        return root.controller.breadcrumbEntriesForPath(root.path)
                    }

                    delegate: Row {
                        required property int index
                        required property var modelData
                        spacing: 4
                        anchors.verticalCenter: parent.verticalCenter

                        readonly property string name: modelData && modelData.name !== undefined ? String(modelData.name) : ""
                        readonly property string path: modelData && modelData.path !== undefined ? String(modelData.path) : ""
                        readonly property bool isDrive: modelData && modelData.isDrive !== undefined ? Boolean(modelData.isDrive) : false
                        
                        readonly property bool isLast: index === pathRepeater.count - 1

                        ToolButton {
                            id: crumbBtn
                            anchors.verticalCenter: parent.verticalCenter
                            padding: 6
                            leftPadding: 8
                            rightPadding: 8
                            
                            contentItem: Row {
                                spacing: 4
                                Image {
                                    source: root.getFolderIcon(name, isDrive, false)
                                    width: 14
                                    height: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                    sourceSize: Qt.size(28, 28)
                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        colorization: 1.0
                                        colorizationColor: isLast ? Theme.accent : Theme.textPrimary
                                    }
                                }
                                Text {
                                    text: name
                                    font.pixelSize: 12
                                    font.bold: isLast
                                    color: isLast ? Theme.accent : Theme.textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            
                            background: Rectangle {
                                color: crumbBtn.down 
                                       ? Theme.surfaceActive 
                                       : (crumbBtn.hovered ? Theme.itemHoverFill : "transparent")
                                radius: 6
                                
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            
                            onClicked: {
                                if (root.controller) {
                                    root.focusPath()
                                    Qt.callLater(() => {
                                        root.controller.openPath(path)
                                    })
                                }
                            }
                        }

                        Item {
                            id: separatorSegment
                            width: 16
                            height: 24
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !isLast

                            readonly property bool interactive: root.readOnly

                            Rectangle {
                                anchors.fill: parent
                                radius: 4
                                color: Theme.itemHoverFill
                                opacity: separatorSegment.interactive && segSepMouseArea.containsMouse ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                            }

                            Label {
                                text: "\u203A"
                                anchors.centerIn: parent
                                color: separatorSegment.interactive && segSepMouseArea.containsMouse ? Theme.accent : Theme.textSecondary
                                font.pixelSize: 16
                                font.bold: true
                                opacity: separatorSegment.interactive && segSepMouseArea.containsMouse ? 1.0 : 0.6
                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                            }

                            MouseArea {
                                id: segSepMouseArea
                                anchors.fill: parent
                                hoverEnabled: separatorSegment.interactive
                                cursorShape: separatorSegment.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
                                enabled: separatorSegment.interactive
                                onClicked: root.openMenu(path, separatorSegment)
                            }
                        }
                    }

                }
            }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            acceptedButtons: Qt.LeftButton
            onClicked: {
                if (!root.readOnly) {
                    root.editRequested()
                } else {
                    root.focusPath()
                }
            }
            onWheel: (wheel) => {
                if (wheel.angleDelta.y !== 0) {
                    flickable.contentX = Math.max(0, Math.min(flickable.contentWidth - flickable.width, flickable.contentX - wheel.angleDelta.y))
                } else if (wheel.angleDelta.x !== 0) {
                    flickable.contentX = Math.max(0, Math.min(flickable.contentWidth - flickable.width, flickable.contentX - wheel.angleDelta.x))
                }
            }
        }
    }

    // Helper to get consistent icon colors
    function getIconColor(name, isCurrent, isHovered) {
        let base = Theme.textSecondary
        let lower = name.toLowerCase()
        
        if (lower === "this pc" || lower === "computer" || lower === "devices://") {
            base = "#6366f1"
        } else if (lower === "favorites" || lower === "favorites://") {
            base = Theme.categoryNavigation
        } else if (lower === "home") {
            base = "#8b5cf6"
        } else if (lower === "desktop") {
            base = "#0ea5e9"
        } else if (lower === "downloads") {
            base = "#22c55e"
        } else if (lower === "documents") {
            base = "#f59e0b"
        } else if (lower === "pictures" || lower === "images") {
            base = "#ec4899"
        } else if (lower === "music") {
            base = "#a855f7"
        } else if (lower === "videos" || lower === "movies") {
            base = "#ef4444"
        } else if (lower.includes(":") || lower === "hard-drive") {
            base = "#3b82f6"
        } else {
            // Default folder color
            base = "#22c55e"
        }

        if (isCurrent) {
            return Qt.lighter(base, themeController.isDark ? 1.2 : 1.1)
        }
        if (isHovered) {
            return Qt.lighter(base, themeController.isDark ? 1.1 : 1.05)
        }
        return base
    }

    // Dynamic dropdown menu components
    ThemedContextMenu {
        id: dropdownMenu
        implicitWidth: 240
        padding: 6
    }

    Component {
        id: menuItemComponent
        ThemedMenuItem {
            id: itemRoot
            property string fullPath
            property bool isCurrent: false
            implicitWidth: dropdownMenu.width - dropdownMenu.leftPadding - dropdownMenu.rightPadding
            implicitHeight: 32
            
            readonly property color accentColor: root.getIconColor(itemRoot.text, itemRoot.isCurrent, itemRoot.hovered)
            iconColor: accentColor
            highlighted: isCurrent

            background: Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: 6
                color: {
                    if (!itemRoot.enabled) return "transparent"
                    if (itemRoot.down) return itemRoot.pressedFill
                    if (itemRoot.hovered) return Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.12)
                    if (itemRoot.isCurrent) return Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.08)
                    return "transparent"
                }
                
                border.color: {
                    if (itemRoot.hovered) return Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.25)
                    if (itemRoot.isCurrent) return Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.15)
                    return "transparent"
                }
                border.width: 1

                // Active indicator on the left
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 5
                    width: 3
                    radius: 1.5
                    color: accentColor
                    visible: itemRoot.isCurrent
                }

                Behavior on color { ColorAnimation { duration: 120 } }
            }

            contentItem: RowLayout {
                spacing: 10
                anchors.fill: parent
                anchors.leftMargin: itemRoot.isCurrent ? 12 : 10
                anchors.rightMargin: 10

                Image {
                    id: menuIcon
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    source: itemRoot.icon.source
                    sourceSize: Qt.size(32, 32)
                    smooth: true
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        colorization: 1.0
                        colorizationColor: itemRoot.iconColor
                    }
                }

                Label {
                    text: itemRoot.text
                    color: itemRoot.isCurrent 
                        ? Theme.textPrimary 
                        : (itemRoot.hovered ? Theme.textPrimary : Theme.textSecondary)
                    font.pixelSize: 12
                    font.weight: itemRoot.isCurrent ? Font.DemiBold : (itemRoot.hovered ? Font.Medium : Font.Normal)
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
            }

            onTriggered: {
                if (root.controller && fullPath) {
                    root.focusPath()
                    Qt.callLater(() => {
                        root.controller.openPath(fullPath)
                    })
                }
            }
        }
    }

    function openMenu(parentPath, visualTarget) {
        if (!root.controller) return;

        let searchPath = parentPath;
        if (searchPath !== "devices://") {
            if (!searchPath.endsWith("/") && !searchPath.endsWith("\\")) {
                searchPath += "/";
            }
        }

        let suggestions = root.controller.getDirectorySuggestions(searchPath)
        if (suggestions.length === 0) return;

        // Find if any suggestion matches the next segment in the current path
        let nextSegment = ""
        let currentPath = root.path
        let parentPathLower = parentPath.toLowerCase()
        let currentPathLower = currentPath.toLowerCase()

        if (parentPath === "devices://") {
            let parts = currentPath.split(/[/\\]/).filter(p => p.length > 0)
            if (parts.length > 0) {
                // For drives, match the drive letter (e.g., "C:")
                nextSegment = parts[0].toLowerCase().replace(/[/\\]$/, "")
            }
        } else if (currentPathLower.startsWith(parentPathLower)) {
            let remaining = currentPath.substring(parentPath.length)
            if (remaining.startsWith("/") || remaining.startsWith("\\")) {
                remaining = remaining.substring(1)
            }
            let parts = remaining.split(/[/\\]/).filter(p => p.length > 0)
            if (parts.length > 0) {
                nextSegment = parts[0].toLowerCase()
            }
        }

        // Clear old items
        while (dropdownMenu.count > 0) {
            let item = dropdownMenu.takeItem(0)
            if (item) {
                item.destroy()
            }
        }

        // Populate new items
        for (let i = 0; i < suggestions.length; i++) {
            let path = suggestions[i]
            let displayName = root.controller.fileNameForPath(path)
            if (!displayName || displayName.length === 0) {
                let parts = path.split(/[/\\]/).filter(p => p.length > 0)
                displayName = parts.length > 0 ? parts[parts.length - 1] : path
            }

            // Use the helper for better icons
            let isDrive = (parentPath === "devices://")
            let iconSource = root.getFolderIcon(displayName, isDrive, false)
            
            if (isDrive) {
                displayName = displayName.replace(/[/\\]$/, "")
            }

            let isCurrent = (displayName.toLowerCase() === nextSegment)

            let item = menuItemComponent.createObject(null, {
                "text": displayName,
                "icon.source": iconSource,
                "fullPath": path,
                "isCurrent": isCurrent
            })
            dropdownMenu.insertItem(i, item)
        }

        dropdownMenu.popup(visualTarget, 0, visualTarget.height)
    }
}

