import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "common"
import "../style"

Control {
    id: root

    property var controller
    required property string path
    property bool readOnly: true
    property bool backgroundVisible: false
    property int menuRequestId: 0
    property string menuParentPath: ""
    property string menuNextSegment: ""
    property var menuVisualTarget: null
    property var openPathHandler: null
    property var prepareNavigationHandler: null
    readonly property real maxCrumbWidth: Math.max(110, Math.min(220, width * 0.42))
    readonly property real maxLastCrumbWidth: Math.max(130, Math.min(300, width * 0.55))

    signal editRequested()

    implicitHeight: Theme.controlHeight

    function focusPath() {
        root.forceActiveFocus()
    }

    function openPath(path) {
        if (root.openPathHandler) {
            return root.openPathHandler(path)
        }
        return root.controller ? root.controller.openPath(path) : false
    }

    function prepareNavigation(reason) {
        if (root.prepareNavigationHandler) {
            root.prepareNavigationHandler(reason)
        }
    }

    function getFolderIcon(name, isDrive, isThisPc, isArchive, pathKind, path) {
        const val = String(path || "").replace(/%20/g, " ")
        if (val === "mega:///") {
            return "../assets/filetypes-next/mega.svg"
        }
        if (val.toLowerCase() === "mega:///cloud drive") {
            return "../assets/filetypes-next/mega-clouddrive.svg"
        }
        if (val.startsWith("mega://")) {
            return "../assets/icons/folder.svg"
        }
        if (val === "gdrive://") {
            return "../assets/filetypes-next/gdrive.svg"
        }
        if (val.toLowerCase() === "gdrive://my-drive") {
            return "../assets/filetypes-next/gdrive-mydrive.svg"
        }
        if (val.toLowerCase() === "gdrive://shared-with-me") {
            return "../assets/filetypes-next/gdrive-shared.svg"
        }
        if (val.toLowerCase() === "gdrive://shortcuts") {
            return "../assets/filetypes-next/gdrive-shortcut.svg"
        }
        if (val.toLowerCase() === "gdrive://trash") {
            return "../assets/filetypes-next/gdrive-trash.svg"
        }
        if (val.startsWith("gdrive://")) {
            return "../assets/icons/folder.svg"
        }
        if (val.toLowerCase().startsWith("instagram://")) {
            return "../assets/filetypes-next/instagram.svg"
        }
        if (val === "telegram://" || val === "telegram:///") {
            return "../assets/filetypes-next/telegram.svg"
        }
        if (val.toLowerCase() === "telegram://saved") {
            return "../assets/filetypes-next/telegram-saved.svg"
        }
        if (val.toLowerCase() === "telegram://chats") {
            return "../assets/filetypes-next/telegram-chats.svg"
        }
        if (val.toLowerCase() === "telegram://downloads") {
            return "../assets/filetypes-next/telegram-downloads.svg"
        }
        if (val.toLowerCase().startsWith("telegram://channel/")) {
            return "../assets/filetypes-next/telegram-badge-channel.svg"
        }
        if (val.toLowerCase().startsWith("telegram://chat/")) {
            return "../assets/filetypes-next/telegram-badge-chat.svg"
        }
        if (val.toLowerCase().startsWith("telegram://")) {
            return "../assets/filetypes-next/telegram.svg"
        }

        if (isThisPc) return "../assets/icons/computer.svg";
        if (isDrive) return "../assets/icons/hard-drive.svg";
        if (isArchive) return "../assets/icons/archive.svg";
        if (pathKind === "ftp") return "../assets/icons/ftp.svg";
        if (pathKind === "gdrive") return "../assets/icons/hard-drive.svg";
        if (pathKind === "instagram") return "../assets/filetypes-next/instagram.svg";
        if (pathKind === "remote") return "../assets/icons/computer.svg";
        return "../assets/icons/folder.svg";
    }

    function crumbIconTone(pathKind, isArchive, isDrive) {
        if (pathKind === "ftp") return "ftp"
        if (pathKind === "gdrive") return "gdrive"
        if (pathKind === "mega") return "gdrive"
        if (pathKind === "instagram") return "instagram"
        if (pathKind === "telegram") return "telegram"
        if (pathKind === "remote") return "remote"
        if (isArchive) return "archive"
        if (isDrive) return "hard-drive"
        return "folder"
    }

    function isArchiveCrumbPath(path) {
        const value = String(path || "")
        return value.indexOf("archive://") === 0 && value.endsWith("|/")
    }

    function virtualRootTitle() {
        if (root.favoritesRootMode) {
            return "Favorites"
        }
        if (root.deviceRootMode) {
            return "This PC"
        }
        return ""
    }

    function virtualRootSubtitle() {
        if (root.favoritesRootMode) {
            return "Pinned paths and frequent folders"
        }
        if (root.deviceRootMode) {
            return "Drives, devices, and storage"
        }
        return ""
    }

    // True when the active panel is showing the virtual devices:// root
    readonly property bool deviceRootMode: root.controller ? root.controller.isDeviceRoot : false
    readonly property bool favoritesRootMode: root.controller ? root.controller.isFavoritesRoot : false
    readonly property bool virtualRootMode: root.deviceRootMode || root.favoritesRootMode

    background: Rectangle {
        visible: root.backgroundVisible
        color: themeController.isDark ? Theme.surface : Theme.bg
        radius: Theme.controlRadius
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
                    implicitHeight: Math.max(28, Theme.controlHeight - 8)
                    
                    contentItem: Item {
                        RecolorSvgIcon {
                            anchors.centerIn: parent
                            sourcePath: "../assets/icons/computer.svg"
                            recolorColor: root.getIconColor("devices://", root.deviceRootMode, thisPcCrumb.hovered)
                            width: 14
                            height: 14
                            sourceSize: Qt.size(28, 28)
                        }
                    }

                    ToolTip.visible: hovered
                    ToolTip.text: "This PC"
                    
                    background: Rectangle {
                        color: thisPcCrumb.down 
                               ? Theme.surfaceActive 
                               : (thisPcCrumb.hovered ? Theme.itemHoverFill : "transparent")
                        radius: Theme.radiusForSide(Math.min(width, height))
                        
                        Behavior on color { ColorAnimation { duration: 100 } }
                    }
                    
                    onClicked: {
                        if (root.controller && !root.deviceRootMode) {
                            root.focusPath()
                            Qt.callLater(() => {
                                root.openPath("devices://")
                            })
                        }
                    }
                    onPressedChanged: if (pressed) root.prepareNavigation("pathbar-devices-press")
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
                    implicitHeight: Math.max(28, Theme.controlHeight - 8)

                    contentItem: Item {
                        RecolorSvgIcon {
                            anchors.centerIn: parent
                            sourcePath: "../assets/icons/star.svg"
                            recolorColor: root.getIconColor("favorites://", root.favoritesRootMode, favoritesCrumb.hovered)
                            width: 14
                            height: 14
                            sourceSize: Qt.size(28, 28)
                        }
                    }

                    ToolTip.visible: hovered
                    ToolTip.text: "Favorites"

                    background: Rectangle {
                        color: favoritesCrumb.down
                               ? Theme.surfaceActive
                               : (favoritesCrumb.hovered ? Theme.itemHoverFill : "transparent")
                        radius: Theme.radiusForSide(Math.min(width, height))
                    }

                    onClicked: {
                        if (root.controller && !root.favoritesRootMode) {
                            root.focusPath()
                            Qt.callLater(() => {
                                root.openPath("favorites://")
                            })
                        }
                    }
                    onPressedChanged: if (pressed) root.prepareNavigation("pathbar-favorites-press")
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.virtualRootMode
                    width: Math.min(360, Math.max(0, flickable.width - x - 12))
                    spacing: 0

                    Label {
                        width: parent.width
                        text: root.virtualRootTitle()
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Label {
                        width: parent.width
                        text: root.virtualRootSubtitle()
                        visible: parent.width >= 190
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        elide: Text.ElideRight
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
                        radius: Theme.radiusXs
                        color: Theme.itemHoverFill
                        opacity: separatorThisPc.interactive && thisPcSepMouseArea.containsMouse ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    Label {
                        text: "\u203A"
                        anchors.centerIn: parent
                        color: separatorThisPc.interactive && thisPcSepMouseArea.containsMouse ? Theme.accent : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTitle
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
                        readonly property string pathKind: modelData && modelData.pathKind !== undefined ? String(modelData.pathKind) : ""
                        readonly property bool isDrive: modelData && modelData.isDrive !== undefined ? Boolean(modelData.isDrive) : false
                        readonly property bool isArchive: modelData && modelData.isArchive !== undefined ? Boolean(modelData.isArchive) : root.isArchiveCrumbPath(path)
                        
                        readonly property bool isLast: index === pathRepeater.count - 1

                        ToolButton {
                            id: crumbBtn
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.min(implicitWidth, isLast ? root.maxLastCrumbWidth : root.maxCrumbWidth)
                            implicitHeight: Math.max(28, Theme.controlHeight - 8)
                            padding: 6
                            leftPadding: 8
                            rightPadding: 8
                            
                            contentItem: RowLayout {
                                spacing: 4
                                clip: true

                                RecolorSvgIcon {
                                    sourcePath: root.getFolderIcon(name, isDrive, false, isArchive, pathKind, path)
                                    recolorColor: root.getIconColor(root.crumbIconTone(pathKind, isArchive, isDrive), isLast, crumbBtn.hovered)
                                    readonly property bool isBrandedPath: {
                                        const val = path.toLowerCase().replace(/%20/g, " ");
                                        return val === "gdrive://"
                                            || val === "gdrive://my-drive"
                                            || val === "gdrive://shared-with-me"
                                            || val === "gdrive://shortcuts"
                                            || val === "gdrive://trash"
                                            || val === "mega:///"
                                            || val === "mega:///cloud drive"
                                            || val.startsWith("instagram://")
                                            || val.startsWith("telegram://");
                                    }
                                    recolorEnabled: !isBrandedPath
                                    Layout.preferredWidth: 14
                                    Layout.preferredHeight: 14
                                    Layout.alignment: Qt.AlignVCenter
                                    sourceSize: Qt.size(28, 28)
                                }

                                Text {
                                    id: crumbText
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    text: name
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLabel
                                    font.bold: isLast
                                    color: isLast ? Theme.accent : Theme.textPrimary
                                    elide: Text.ElideMiddle
                                    horizontalAlignment: Text.AlignLeft
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            ToolTip.visible: hovered
                            ToolTip.delay: 450
                            ToolTip.text: name
                            
                            background: Rectangle {
                                color: crumbBtn.down 
                                       ? Theme.surfaceActive 
                                       : (crumbBtn.hovered ? Theme.itemHoverFill : "transparent")
                                radius: Theme.radiusForSide(Math.min(width, height))
                                
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            
                            onClicked: {
                                if (root.controller) {
                                    root.focusPath()
                                    Qt.callLater(() => {
                                        root.openPath(path)
                                    })
                                }
                            }
                            onPressedChanged: if (pressed) root.prepareNavigation("pathbar-crumb-press")
                        }

                        Item {
                            id: separatorSegment
                            width: 16
                            height: 24
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !isLast

                            readonly property bool interactive: root.readOnly && !root.isArchiveCrumbPath(path)

                            Rectangle {
                                anchors.fill: parent
                                radius: Theme.radiusXs
                                color: Theme.itemHoverFill
                                opacity: separatorSegment.interactive && segSepMouseArea.containsMouse ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                            }

                            Label {
                                text: "\u203A"
                                anchors.centerIn: parent
                                color: separatorSegment.interactive && segSepMouseArea.containsMouse ? Theme.accent : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeTitle
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
            base = Theme.actionIconColor("system")
        } else if (lower === "favorites" || lower === "favorites://") {
            base = Theme.actionIconColor("favorite")
        } else if (lower === "archive") {
            base = Theme.actionIconColor("archive")
        } else if (lower === "ftp" || lower === "remote" || lower === "gdrive" || lower === "instagram" || lower === "telegram") {
            base = Theme.categoryNavigation
        } else if (lower.includes(":") || lower === "hard-drive") {
            base = Theme.actionIconColor("drive")
        } else {
            base = Theme.actionIconColor("folder")
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
        onClosed: {
            root.menuRequestId += 1
            if (root.controller && root.controller.cancelDirectorySuggestions) {
                root.controller.cancelDirectorySuggestions()
            }
            root.menuVisualTarget = null
        }
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
                radius: Theme.radiusForSide(height)
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
                    radius: Theme.radiusXs
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

                RecolorSvgIcon {
                    id: menuIcon
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    sourcePath: itemRoot.icon.source ? itemRoot.icon.source.toString() : ""
                    recolorColor: itemRoot.iconColor
                    sourceSize: Qt.size(32, 32)
                    smooth: true
                }

                Label {
                    text: itemRoot.text
                    color: itemRoot.isCurrent 
                        ? Theme.textPrimary 
                        : (itemRoot.hovered ? Theme.textPrimary : Theme.textSecondary)
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
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
                        root.openPath(fullPath)
                    })
                }
            }
        }
    }

    Component {
        id: placeholderMenuItemComponent
        ThemedMenuItem {
            enabled: false
            implicitWidth: dropdownMenu.width - dropdownMenu.leftPadding - dropdownMenu.rightPadding
            implicitHeight: 32
        }
    }

    function clearDropdownMenu() {
        while (dropdownMenu.count > 0) {
            let item = dropdownMenu.takeItem(0)
            if (item) {
                item.destroy()
            }
        }
    }

    function showPlaceholder(text) {
        clearDropdownMenu()
        let item = placeholderMenuItemComponent.createObject(null, {
            "text": text
        })
        dropdownMenu.insertItem(0, item)
    }

    function nextSegmentForParent(parentPath) {
        if (root.controller && root.controller.breadcrumbEntriesForPath && parentPath !== "devices://") {
            const crumbs = root.controller.breadcrumbEntriesForPath(root.path)
            const parentKey = normalizedMenuPath(parentPath)
            for (let i = 0; i + 1 < crumbs.length; ++i) {
                const crumbPath = String(crumbs[i].path || "")
                if (normalizedMenuPath(crumbPath) === parentKey) {
                    return normalizedMenuPath(String(crumbs[i + 1].path || ""))
                }
            }
        }

        let currentPath = root.path
        let parentPathLower = parentPath.toLowerCase()
        let currentPathLower = currentPath.toLowerCase()

        if (parentPath === "devices://") {
            let parts = currentPath.split(/[/\\]/).filter(p => p.length > 0)
            if (parts.length > 0) {
                return parts[0].toLowerCase().replace(/[/\\]$/, "")
            }
        } else if (currentPathLower.startsWith(parentPathLower)) {
            let remaining = currentPath.substring(parentPath.length)
            if (remaining.startsWith("/") || remaining.startsWith("\\")) {
                remaining = remaining.substring(1)
            }
            let parts = remaining.split(/[/\\]/).filter(p => p.length > 0)
            if (parts.length > 0) {
                return parts[0].toLowerCase()
            }
        }
        return ""
    }

    function normalizedMenuPath(value) {
        let text = String(value || "").replace(/\\/g, "/")
        while (text.length > 1 && text.endsWith("/")) {
            text = text.substring(0, text.length - 1)
        }
        return text.toLowerCase()
    }

    function suggestionIsCurrentChild(path, label, isDrive) {
        const nextPath = root.menuNextSegment
        if (nextPath.indexOf("://") > 0) {
            return normalizedMenuPath(path) === nextPath
        }
        if (isDrive) {
            const current = normalizedMenuPath(root.path)
            const candidate = normalizedMenuPath(path)
            return candidate.length > 0 && current.indexOf(candidate) === 0
        }
        return String(label || "").toLowerCase() === root.menuNextSegment
    }

    function populateMenu(parentPath, suggestions) {
        clearDropdownMenu()

        if (!suggestions || suggestions.length === 0) {
            showPlaceholder("No folders")
            return
        }

        for (let i = 0; i < suggestions.length; i++) {
            let entry = suggestions[i]
            let path = String(entry.path || "")
            if (path.length === 0) {
                continue
            }

            let displayName = String(entry.label || "")
            if (!displayName || displayName.length === 0) {
                let parts = path.split(/[/\\]/).filter(p => p.length > 0)
                displayName = parts.length > 0 ? parts[parts.length - 1] : path
            }

            let isDrive = Boolean(entry.isDrive)
            let isArchive = root.isArchiveCrumbPath(path)
            let iconSource = root.getFolderIcon(displayName, isDrive, false, isArchive, "", path)
            
            if (isDrive) {
                displayName = displayName.replace(/[/\\]$/, "")
            }

            let isCurrent = suggestionIsCurrentChild(path, displayName, isDrive)

            let pathLower = path.toLowerCase().replace(/%20/g, " ")
            let isBranded = pathLower === "gdrive://"
                || pathLower === "gdrive://my-drive"
                || pathLower === "gdrive://shared-with-me"
                || pathLower === "gdrive://shortcuts"
                || pathLower === "gdrive://trash"
                || pathLower === "mega:///"
                || pathLower === "mega:///cloud drive"

            let item = menuItemComponent.createObject(null, {
                "text": displayName,
                "icon.source": iconSource,
                "iconColor": root.getIconColor(isArchive ? "archive" : (isDrive ? "hard-drive" : "folder"), isCurrent, false),
                "recolorEnabled": !isBranded,
                "fullPath": path,
                "isCurrent": isCurrent
            })
            dropdownMenu.insertItem(dropdownMenu.count, item)
        }

        if (dropdownMenu.count === 0) {
            showPlaceholder("No folders")
        }
    }

    function openMenu(parentPath, visualTarget) {
        if (!root.controller || !root.controller.requestDirectorySuggestionEntries) return;

        let searchPath = parentPath;
        if (searchPath !== "devices://") {
            if (!searchPath.endsWith("/") && !searchPath.endsWith("\\")) {
                searchPath += "/";
            }
        }

        root.menuRequestId += 1
        root.menuParentPath = parentPath
        root.menuNextSegment = nextSegmentForParent(parentPath)
        root.menuVisualTarget = visualTarget

        showPlaceholder("Loading folders...")

        dropdownMenu.popup(visualTarget, 0, visualTarget.height)
        root.controller.requestDirectorySuggestionEntries(searchPath, root.menuRequestId, 240)
    }

    Connections {
        target: root.controller ? root.controller : null
        function onDirectorySuggestionEntriesReady(requestId, suggestions) {
            if (requestId !== root.menuRequestId || !dropdownMenu.visible) {
                return
            }
            root.populateMenu(root.menuParentPath, suggestions)
        }
    }
}
