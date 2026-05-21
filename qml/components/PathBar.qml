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
        if (lower === "pictures" || lower === "images") return "../assets/icons/image.svg";
        if (lower === "music") return "../assets/icons/music.svg";
        if (lower === "videos" || lower === "movies") return "../assets/icons/video.svg";
        
        return "../assets/icons/folder.svg";
    }

    // True when the active panel is showing the virtual devices:// root
    readonly property bool deviceRootMode: root.controller ? root.controller.isDeviceRoot : false

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
                    padding: 6
                    leftPadding: 8
                    rightPadding: 8
                    
                    contentItem: Row {
                        spacing: 4
                        Image {
                            id: thisPcIcon
                            source: "../assets/icons/computer.svg"
                            width: 14
                            height: 14
                            anchors.verticalCenter: parent.verticalCenter
                            sourceSize: Qt.size(28, 28)
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                colorization: 1.0
                                colorizationColor: root.deviceRootMode ? Theme.accent : Theme.textSecondary
                            }
                        }
                        Text {
                            text: "This PC"
                            font.pixelSize: 12
                            font.bold: root.deviceRootMode
                            color: root.deviceRootMode ? Theme.accent : Theme.textSecondary
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    
                    background: Rectangle {
                        color: thisPcCrumb.down 
                               ? Theme.surfaceActive 
                               : (thisPcCrumb.hovered ? Theme.itemHoverFill : "transparent")
                        radius: 6
                        
                        Behavior on color { ColorAnimation { duration: 100 } }
                    }
                    
                    onClicked: {
                        if (root.controller && !root.deviceRootMode) {
                            root.controller.openPath("devices://")
                        }
                    }
                }

                // ── Separator (only if not at devices://) ──
                Item {
                    id: separatorThisPc
                    width: 16
                    height: 24
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.deviceRootMode

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
                    visible: !root.deviceRootMode
                    model: {
                        if (root.deviceRootMode) return []
                        let path = root.path
                        if (!path) return []

                        let parts = path.split(/[/\\]/).filter(p => p.length > 0)
                        let result = []
                        let current = ""

                        // Handle Windows drive root
                        if (path.includes(":") && parts.length > 0) {
                            current = parts[0] + "\\"
                            result.push({ 
                                name: parts[0], 
                                path: current,
                                isDrive: true
                            })
                            parts.shift()
                        }

                        for (let p of parts) {
                            current += (current.endsWith("\\") || current.endsWith("/") ? "" : "/") + p
                            result.push({ 
                                name: p, 
                                path: current,
                                isDrive: false
                            })
                        }
                        return result
                    }

                    delegate: Row {
                        spacing: 4
                        anchors.verticalCenter: parent.verticalCenter
                        
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
                                    source: root.getFolderIcon(modelData.name, modelData.isDrive, false)
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
                                    text: modelData.name
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
                                    root.controller.openPath(modelData.path)
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
                                onClicked: root.openMenu(modelData.path, separatorSegment)
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

    // Dynamic dropdown menu components
    ThemedContextMenu {
        id: dropdownMenu
        implicitWidth: 180
    }

    Component {
        id: menuItemComponent
        ThemedMenuItem {
            id: itemRoot
            property string fullPath
            implicitWidth: 172
            implicitHeight: visible ? 30 : 0
            iconColor: Theme.accent

            background: Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: 4
                color: !itemRoot.enabled
                        ? "transparent"
                        : itemRoot.down
                                ? itemRoot.pressedFill
                                : itemRoot.hovered
                                        ? itemRoot.hoverFill
                                        : "transparent"
                Behavior on color { ColorAnimation { duration: 100 } }
            }

            onTriggered: {
                if (root.controller && fullPath) {
                    root.controller.openPath(fullPath)
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
            let parts = path.split(/[/\\]/).filter(p => p.length > 0)
            let displayName = parts.length > 0 ? parts[parts.length - 1] : path

            let iconSource = "../assets/icons/folder.svg"
            if (parentPath === "devices://") {
                iconSource = "../assets/icons/hard-drive.svg"
                displayName = displayName.replace(/[/\\]$/, "")
            }

            let item = menuItemComponent.createObject(null, {
                "text": displayName,
                "icon.source": iconSource,
                "fullPath": path
            })
            dropdownMenu.insertItem(i, item)
        }

        dropdownMenu.popup(visualTarget, 0, visualTarget.height)
    }
}

