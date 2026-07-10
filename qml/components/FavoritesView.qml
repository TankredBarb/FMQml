import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"
import "dialogs"
import "filepanel"

FocusScope {
    id: root

    property var controller
    property var panel
    property var favoritesBackend
    property var quickLookPopup
    property bool liveResizeActive: false

    signal activated()

    readonly property int pinnedCount: root.favoritesBackend ? root.favoritesBackend.pinnedCount : 0
    readonly property int frequentCount: root.favoritesBackend ? root.favoritesBackend.frequentCount : 0
    readonly property int modelCount: root.pinnedCount + root.frequentCount
    readonly property color tagAccent: Theme.categoryAction
    readonly property bool ultraLightMode: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.ultraLightMode
                                           : false
    readonly property bool resizeOptimized: root.liveResizeActive
    readonly property bool effectsReduced: root.resizeOptimized || root.ultraLightMode
    readonly property bool wideFavoritesLayout: width >= 860
    property string contextFavoriteId: ""
    property string contextTargetPath: ""
    property bool contextTargetExists: false
    property bool contextTargetIsDirectory: false
    property string selectedFavoriteId: ""
    property string selectedName: ""
    property string selectedTargetPath: ""
    property bool selectedTargetExists: false
    property bool selectedTargetIsDirectory: false
    property bool selectedIsPinned: false
    property bool previewScrollActive: false
    property bool previewScrollbarPressed: false
    property string pendingPreviewPath: ""
    property string labelEditTargetPath: ""
    property int labelEditPinnedIndex: -1
    property string tagEditTargetPath: ""
    property int tagEditPinnedIndex: -1

    function visitLabel(count) {
        return count === 1 ? "1 visit" : count + " visits"
    }

    function tagsLabel(tags) {
        if (!tags || tags.length === 0) {
            return ""
        }

        const values = []
        for (let i = 0; i < tags.length; ++i) {
            const tag = String(tags[i] || "").trim()
            if (tag.length > 0) {
                values.push("#" + tag)
            }
        }
        return values.join(" ")
    }

    function selectedList() {
        return root.selectedIsPinned ? pinnedList : frequentList
    }

    function favoritesMotionActive() {
        return root.previewScrollbarPressed
            || (pinnedList && (pinnedList.moving || pinnedList.flicking))
            || (frequentList && (frequentList.moving || frequentList.flicking))
    }

    function favoritePreviewSuppressed() {
        return root.previewScrollActive || root.previewScrollbarPressed
    }

    function markFavoritePreviewScrollActive() {
        if (root.resizeOptimized || root.modelCount <= 0) {
            return
        }
        root.previewScrollActive = true
        favoritePreviewSyncTimer.stop()
        previewScrollStopTimer.restart()
    }

    function handleFavoriteScrollbarPressed(pressed) {
        root.previewScrollbarPressed = pressed
        if (pressed) {
            root.markFavoritePreviewScrollActive()
            return
        }
        previewScrollStopTimer.restart()
    }

    function scheduleFavoritePreview(targetPath, exists) {
        favoritePreviewSyncTimer.stop()
        if (!exists || targetPath.length === 0 || typeof quickLookController === "undefined" || !quickLookController) {
            root.pendingPreviewPath = ""
            return
        }

        root.pendingPreviewPath = targetPath
        if (root.favoritePreviewSuppressed()) {
            return
        }
        favoritePreviewSyncTimer.restart()
    }

    function flushPendingFavoritePreview() {
        if (root.favoritePreviewSuppressed()) {
            return
        }
        if (root.pendingPreviewPath.length === 0
                || root.pendingPreviewPath !== root.selectedTargetPath
                || !root.selectedTargetExists
                || typeof quickLookController === "undefined"
                || !quickLookController) {
            return
        }
        const path = root.pendingPreviewPath
        root.pendingPreviewPath = ""
        quickLookController.preview(path)
    }

    function selectRow(listView, index, favoriteId, name, targetPath, exists, isDirectory, isPinned) {
        root.activated()
        root.forceActiveFocus()
        listView.forceActiveFocus()
        listView.currentIndex = index
        root.selectedFavoriteId = favoriteId
        root.selectedName = name
        root.selectedTargetPath = targetPath
        root.selectedTargetExists = exists
        root.selectedTargetIsDirectory = isDirectory
        root.selectedIsPinned = isPinned
        if (isPinned) {
            frequentList.currentIndex = -1
        } else {
            pinnedList.currentIndex = -1
        }
        root.scheduleFavoritePreview(targetPath, exists)
    }

    function selectPinnedIndex(index) {
        if (root.pinnedCount <= 0) {
            return
        }
        const bounded = Math.max(0, Math.min(index, root.pinnedCount - 1))
        pinnedList.currentIndex = bounded
        pinnedList.positionViewAtIndex(bounded, ListView.Contain)
        Qt.callLater(() => {
            const item = pinnedList.itemAtIndex(bounded)
            if (item) {
                root.selectRow(pinnedList, bounded, item.favoriteId, item.itemName, item.itemTargetPath,
                               item.itemExists, item.itemIsDirectory, true)
            }
        })
    }

    function selectFrequentIndex(index) {
        if (root.frequentCount <= 0) {
            return
        }
        const bounded = Math.max(0, Math.min(index, root.frequentCount - 1))
        frequentList.currentIndex = bounded
        frequentList.positionViewAtIndex(bounded, ListView.Contain)
        Qt.callLater(() => {
            const item = frequentList.itemAtIndex(bounded)
            if (item) {
                root.selectRow(frequentList, bounded, item.favoriteId, item.itemName, item.itemTargetPath,
                               item.itemExists, item.itemIsDirectory, false)
            }
        })
    }

    function selectFirstAvailable() {
        if (root.pinnedCount > 0) {
            root.selectPinnedIndex(0)
        } else if (root.frequentCount > 0) {
            root.selectFrequentIndex(0)
        }
    }

    function openFavorite(favoriteId) {
        if (!root.favoritesBackend || favoriteId.length === 0 || root.selectedTargetPath.length === 0) {
            return
        }
        root.activated()
        root.favoritesBackend.openInPanel(root.selectedTargetPath, root.selectedTargetIsDirectory)
    }

    function openFavoriteTarget(targetPath, isDirectory) {
        if (!root.favoritesBackend || targetPath.length === 0) {
            return
        }
        root.activated()
        root.favoritesBackend.openInPanel(targetPath, isDirectory)
    }

    function openQuickLookForCurrentFavorite() {
        if (!root.selectedTargetExists
                || root.selectedTargetPath.length === 0
                || typeof quickLookController === "undefined"
                || !quickLookController
                || !root.quickLookPopup) {
            return false
        }

        favoritePreviewSyncTimer.stop()
        root.pendingPreviewPath = ""
        quickLookController.preview(root.selectedTargetPath)
        root.quickLookPopup.previewPath = root.selectedTargetPath
        root.quickLookPopup.open()
        return true
    }

    function removeFavorite(targetPath) {
        if (!root.favoritesBackend || targetPath.length === 0 || !root.selectedIsPinned) {
            return
        }
        root.activated()
        root.favoritesBackend.unpinPath(targetPath)
    }

    function removeCurrentFavorite() {
        if (!root.selectedIsPinned) {
            if (root.pinnedCount > 0) {
                root.selectPinnedIndex(0)
            }
            return
        }
        root.removeFavorite(root.selectedTargetPath)
    }

    function moveSelectedPinned(offset) {
        if (!root.favoritesBackend || !root.selectedIsPinned || root.selectedTargetPath.length === 0) {
            return
        }

        const oldIndex = pinnedList.currentIndex
        const changed = offset < 0
            ? root.favoritesBackend.movePinnedUp(root.selectedTargetPath)
            : root.favoritesBackend.movePinnedDown(root.selectedTargetPath)
        if (changed) {
            Qt.callLater(() => root.selectPinnedIndex(oldIndex + offset))
        }
    }

    function editSelectedPinnedLabel() {
        if (!root.selectedIsPinned || root.selectedTargetPath.length === 0) {
            return
        }
        root.labelEditTargetPath = root.selectedTargetPath
        root.labelEditPinnedIndex = pinnedList.currentIndex
        labelEditField.text = root.selectedName
        labelEditDialog.open()
    }

    function applyPinnedLabelEdit() {
        if (!root.favoritesBackend || root.labelEditTargetPath.length === 0) {
            return
        }

        const changed = root.favoritesBackend.setPinnedLabel(root.labelEditTargetPath, labelEditField.text)
        labelEditDialog.close()
        if (changed && root.labelEditPinnedIndex >= 0) {
            Qt.callLater(() => root.selectPinnedIndex(root.labelEditPinnedIndex))
        }
    }

    function editSelectedPinnedTags() {
        if (!root.favoritesBackend || !root.selectedIsPinned || root.selectedTargetPath.length === 0) {
            return
        }
        root.tagEditTargetPath = root.selectedTargetPath
        root.tagEditPinnedIndex = pinnedList.currentIndex
        tagEditField.text = root.favoritesBackend.tagsForPath(root.selectedTargetPath).join(", ")
        tagEditDialog.open()
    }

    function applyPinnedTagsEdit() {
        if (!root.favoritesBackend || root.tagEditTargetPath.length === 0) {
            return
        }

        const changed = root.favoritesBackend.setPinnedTags(root.tagEditTargetPath, tagEditField.text.split(","))
        tagEditDialog.close()
        if (changed && root.tagEditPinnedIndex >= 0) {
            Qt.callLater(() => root.selectPinnedIndex(root.tagEditPinnedIndex))
        }
    }

    function popupContextMenu(listView, index, favoriteId, name, targetPath, exists, isDirectory, isPinned, x, y) {
        root.selectRow(listView, index, favoriteId, name, targetPath, exists, isDirectory, isPinned)
        root.contextFavoriteId = favoriteId
        root.contextTargetPath = targetPath
        root.contextTargetExists = exists
        root.contextTargetIsDirectory = isDirectory
        favoriteContextMenu.popup(x, y)
    }

    Shortcut {
        sequence: "Delete"
        context: Qt.WidgetWithChildrenShortcut
        enabled: root.activeFocus && root.selectedIsPinned && root.selectedTargetPath.length > 0
        onActivated: root.removeCurrentFavorite()
    }

    Timer {
        id: favoritePreviewSyncTimer
        interval: 90
        repeat: false
        onTriggered: {
            if (root.favoritePreviewSuppressed()) {
                return
            }
            root.flushPendingFavoritePreview()
        }
    }

    Timer {
        id: previewScrollStopTimer
        interval: 220
        repeat: false
        onTriggered: {
            if (root.favoritesMotionActive()) {
                root.previewScrollActive = true
                previewScrollStopTimer.restart()
                return
            }
            root.previewScrollActive = false
            root.flushPendingFavoritePreview()
        }
    }

    function handleKey(event) {
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            root.markFavoritePreviewScrollActive()
        }
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (root.selectedFavoriteId.length > 0) {
                root.openFavorite(root.selectedFavoriteId)
            } else {
                root.selectFirstAvailable()
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Space && event.modifiers === Qt.NoModifier) {
            if (root.selectedFavoriteId.length > 0) {
                root.openQuickLookForCurrentFavorite()
            } else {
                root.selectFirstAvailable()
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Delete) {
            root.removeCurrentFavorite()
            event.accepted = true
        } else if (event.key === Qt.Key_Up && (event.modifiers & Qt.ControlModifier) && root.selectedIsPinned) {
            root.moveSelectedPinned(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down && (event.modifiers & Qt.ControlModifier) && root.selectedIsPinned) {
            root.moveSelectedPinned(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            if (root.selectedFavoriteId.length === 0) {
                root.selectFirstAvailable()
            } else if (root.selectedIsPinned) {
                if (pinnedList.currentIndex < root.pinnedCount - 1) {
                    root.selectPinnedIndex(pinnedList.currentIndex + 1)
                } else if (root.frequentCount > 0) {
                    root.selectFrequentIndex(0)
                }
            } else if (frequentList.currentIndex < root.frequentCount - 1) {
                root.selectFrequentIndex(frequentList.currentIndex + 1)
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            if (root.selectedFavoriteId.length === 0) {
                root.selectFirstAvailable()
            } else if (!root.selectedIsPinned) {
                if (frequentList.currentIndex > 0) {
                    root.selectFrequentIndex(frequentList.currentIndex - 1)
                } else if (root.pinnedCount > 0) {
                    root.selectPinnedIndex(root.pinnedCount - 1)
                }
            } else if (pinnedList.currentIndex > 0) {
                root.selectPinnedIndex(pinnedList.currentIndex - 1)
            }
            event.accepted = true
        }
    }

    Keys.priority: Keys.BeforeItem
    Keys.onPressed: (event) => root.handleKey(event)

    Item {
        anchors.fill: parent
        z: -1

        AmbientPanelBackground {
            anchors.fill: parent
            startColor: themeController.isDark
                        ? Theme.withAlpha(Theme.categoryNavigation, 0.11)
                        : Theme.withAlpha(Theme.categoryNavigation, 0.075)
            strength: 0.74
        }

        Rectangle {
            width: parent.width * 0.52
            height: width
            radius: width / 2
            x: -parent.width * 0.12
            y: -parent.height * 0.14
            color: Theme.categoryNavigation
            opacity: themeController.isDark ? 0.07 : 0.04
            visible: !root.effectsReduced
            layer.enabled: visible
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: 150
            }
        }

        Rectangle {
            width: parent.width * 0.46
            height: width
            radius: width / 2
            x: parent.width * 0.62
            y: parent.height * 0.48
            color: root.tagAccent
            opacity: themeController.isDark ? 0.055 : 0.032
            visible: !root.effectsReduced
            layer.enabled: visible
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: 130
            }
        }
    }

    component SectionHeader : RowLayout {
        property string title: ""

        Layout.fillWidth: true
        Layout.preferredHeight: root.ultraLightMode ? 22 : 26
        spacing: root.ultraLightMode ? 6 : 8

        Label {
            text: title
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeCaption
            font.weight: Font.DemiBold
            opacity: 0.82
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.panelBorder
            opacity: 0.8
        }
    }

    component EmptySectionRow : Rectangle {
        property string iconSource: "qrc:/qt/qml/FM/qml/assets/icons/star.svg"
        property string title: ""
        property string subtitle: ""
        property color iconColor: Theme.textSecondary

        Layout.fillWidth: true
        Layout.preferredHeight: root.ultraLightMode ? 44 : 52
        radius: Theme.radiusSm
        color: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.78 : 0.92)
        border.color: Theme.panelBorder

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: root.ultraLightMode ? 10 : 12
            anchors.rightMargin: root.ultraLightMode ? 10 : 12
            spacing: root.ultraLightMode ? 8 : 10

            Image {
                Layout.preferredWidth: root.ultraLightMode ? 16 : 18
                Layout.preferredHeight: root.ultraLightMode ? 16 : 18
                source: iconSource
                sourceSize: Qt.size(root.ultraLightMode ? 16 : 18, root.ultraLightMode ? 16 : 18)
                opacity: 0.78
                layer.enabled: true
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: iconColor
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Label {
                    Layout.fillWidth: true
                    text: title
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: subtitle
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    elide: Text.ElideRight
                }
            }
        }
    }

    component StatTile : Rectangle {
        property string iconSource: ""
        property string title: ""
        property string value: ""
        property color accentColor: Theme.accent

        Layout.fillWidth: true
        Layout.preferredHeight: root.ultraLightMode ? 46 : 56
        radius: Theme.radiusMd
        color: Theme.withAlpha(accentColor, themeController.isDark ? 0.105 : 0.065)
        border.color: Theme.withAlpha(accentColor, themeController.isDark ? 0.30 : 0.22)
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: root.ultraLightMode ? 10 : 12
            anchors.rightMargin: root.ultraLightMode ? 10 : 12
            spacing: root.ultraLightMode ? 8 : 10

            RecolorSvgIcon {
                Layout.preferredWidth: root.ultraLightMode ? 16 : 18
                Layout.preferredHeight: root.ultraLightMode ? 16 : 18
                sourcePath: iconSource
                recolorColor: accentColor
                cacheKey: "favorites-stat"
                sourceSize: Qt.size(32, 32)
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    text: value
                    color: Theme.textPrimary
                    font.pixelSize: root.ultraLightMode ? Theme.fontSizeBody : Theme.scaledSize(15)
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: title
                    color: Theme.textSecondary
                    font.pixelSize: root.ultraLightMode ? Theme.fontSizeMicro : Theme.fontSizeCaption
                    elide: Text.ElideRight
                }
            }
        }
    }

    component FavoriteRow : ItemDelegate {
        id: row

        property var listView
        property bool rowPinned: true
        readonly property bool isCurrent: listView && listView.activeFocus && listView.currentIndex === index
        readonly property int actionHitMargin: root.ultraLightMode
                                                ? (rowPinned ? 136 : 58)
                                                : (rowPinned ? 160 : 68)
        readonly property string favoriteId: model.id || ""
        readonly property string itemName: model.name || ""
        readonly property string itemTargetPath: model.targetPath || ""
        readonly property string itemDisplayPath: model.displayPath || ""
        readonly property string itemSuffix: model.suffix || ""
        readonly property var itemTags: model.tags || []
        readonly property string itemTagsText: root.tagsLabel(itemTags)
        readonly property bool itemExists: model.exists === true
        readonly property bool itemIsDirectory: model.isDirectory === true
        readonly property bool itemHasCustomLabel: model.hasCustomLabel === true
        readonly property int itemVisitCount: model.visitCount || 0
        readonly property real itemUsageProgress: model.usageProgress || 0

        width: listView ? listView.width : 1
        height: root.ultraLightMode ? (rowPinned ? 44 : 48) : (rowPinned ? 54 : 58)
        padding: 0

        contentItem: RowLayout {
            anchors.fill: parent
            anchors.leftMargin: root.ultraLightMode ? 10 : 12
            anchors.rightMargin: root.ultraLightMode ? 6 : 8
            spacing: root.ultraLightMode ? 8 : 10

            FileIconCell {
                Layout.preferredWidth: root.ultraLightMode ? 18 : 22
                Layout.preferredHeight: root.ultraLightMode ? 18 : 22
                path: row.itemTargetPath
                isDirectory: row.itemIsDirectory
                suffix: row.itemSuffix
                useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
                showThumbnail: false
                iconSize: root.ultraLightMode ? 18 : 22
                opacity: row.itemExists ? 1.0 : 0.45
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: row.itemName
                    color: !row.itemExists ? Theme.textSecondary
                         : row.itemHasCustomLabel ? Theme.categoryInfo
                         : Theme.textPrimary
                    font.pixelSize: root.ultraLightMode ? Theme.fontSizeLabel : Theme.fontSizeBody
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        Layout.maximumWidth: Math.max(80, row.width * 0.38)
                        visible: !root.ultraLightMode && row.itemExists && row.rowPinned && row.itemTagsText.length > 0
                        text: row.itemTagsText
                        color: root.tagAccent
                        font.pixelSize: Theme.fontSizeCaption
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: {
                            if (!row.itemExists) {
                                return "Missing target - " + row.itemDisplayPath
                            }
                            if (!row.rowPinned) {
                                return root.visitLabel(row.itemVisitCount) + " - " + row.itemDisplayPath
                            }
                            if (row.itemTagsText.length > 0) {
                                return "- " + row.itemDisplayPath
                            }
                            return row.itemDisplayPath
                        }
                        color: row.itemExists ? Theme.textSecondary : Theme.warning
                        font.pixelSize: root.ultraLightMode ? Theme.fontSizeMicro : Theme.fontSizeCaption
                        elide: Text.ElideRight
                    }
                }

                LinearProgress {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.ultraLightMode ? 3 : 4
                    visible: !row.rowPinned
                    value: row.itemUsageProgress
                    trackColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.55)
                    fillColor: Theme.categoryUtility
                    preserveMinimumFill: true
                }
            }

            IconButton {
                Layout.preferredWidth: root.ultraLightMode ? 26 : 30
                Layout.preferredHeight: root.ultraLightMode ? 26 : 30
                visible: row.itemExists
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/folder-open.svg"
                iconTone: "open"
                iconSize: 15
                onClicked: root.openFavoriteTarget(row.itemTargetPath, row.itemIsDirectory)
                ToolTip.visible: hovered
                ToolTip.text: "Open in panel"
            }

            IconButton {
                Layout.preferredWidth: root.ultraLightMode ? 26 : 30
                Layout.preferredHeight: root.ultraLightMode ? 26 : 30
                visible: row.rowPinned
                enabled: row.itemTargetPath.length > 0
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/rename.svg"
                iconTone: "rename"
                iconSize: 15
                onClicked: {
                    root.selectRow(row.listView, index, row.favoriteId, row.itemName, row.itemTargetPath,
                                   row.itemExists, row.itemIsDirectory, row.rowPinned)
                    root.editSelectedPinnedLabel()
                }
                ToolTip.visible: hovered
                ToolTip.text: "Edit Label"
            }

            IconButton {
                Layout.preferredWidth: root.ultraLightMode ? 26 : 30
                Layout.preferredHeight: root.ultraLightMode ? 26 : 30
                visible: row.rowPinned
                enabled: row.itemTargetPath.length > 0
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/tag.svg"
                iconTone: "action"
                svgRecolorColor: root.tagAccent
                iconSize: 15
                onClicked: {
                    root.selectRow(row.listView, index, row.favoriteId, row.itemName, row.itemTargetPath,
                                   row.itemExists, row.itemIsDirectory, row.rowPinned)
                    root.editSelectedPinnedTags()
                }
                ToolTip.visible: hovered
                ToolTip.text: "Edit Tags"
            }

            IconButton {
                Layout.preferredWidth: root.ultraLightMode ? 26 : 30
                Layout.preferredHeight: root.ultraLightMode ? 26 : 30
                visible: row.rowPinned
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/star-off.svg"
                iconTone: "favorite"
                iconSize: 15
                onClicked: {
                    root.selectRow(row.listView, index, row.favoriteId, row.itemName, row.itemTargetPath,
                                   row.itemExists, row.itemIsDirectory, row.rowPinned)
                    root.removeFavorite(row.itemTargetPath)
                }
                ToolTip.visible: hovered
                ToolTip.text: "Unpin from Favorites"
            }
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: {
                if (row.pressed) return Theme.surfaceActive
                if (row.isCurrent) return Theme.itemCurrentFill
                if (row.hovered) return Theme.itemHoverFill
                return Theme.panelSurfaceSoft
            }
            border.color: {
                if (row.isCurrent) return Theme.itemCurrentBorder
                return row.itemExists ? Theme.panelBorder : Theme.withAlpha(Theme.warning, 0.36)
            }
            border.width: row.isCurrent ? 2 : 1
        }

        MouseArea {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.rightMargin: row.actionHitMargin
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            z: 2

            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton) {
                    const p = mapToItem(root, mouse.x, mouse.y)
                    root.popupContextMenu(row.listView, index, row.favoriteId, row.itemName, row.itemTargetPath,
                                          row.itemExists, row.itemIsDirectory, row.rowPinned, p.x, p.y)
                } else {
                    root.selectRow(row.listView, index, row.favoriteId, row.itemName, row.itemTargetPath,
                                   row.itemExists, row.itemIsDirectory, row.rowPinned)
                }
            }

            onDoubleClicked: (mouse) => {
                if (mouse.button === Qt.LeftButton && row.itemExists) {
                    root.openFavoriteTarget(row.itemTargetPath, row.itemIsDirectory)
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.ultraLightMode ? 16 : 24
        z: 1
        spacing: root.ultraLightMode ? 10 : 14

        RowLayout {
            Layout.fillWidth: true
            spacing: root.ultraLightMode ? 9 : 12

            Image {
                Layout.preferredWidth: root.ultraLightMode ? 22 : 28
                Layout.preferredHeight: root.ultraLightMode ? 22 : 28
                source: "qrc:/qt/qml/FM/qml/assets/icons/star.svg"
                sourceSize: Qt.size(root.ultraLightMode ? 22 : 28, root.ultraLightMode ? 22 : 28)
                opacity: 1.0
                layer.enabled: true
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: Theme.accent
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: "Favorites"
                    color: Theme.textPrimary
                    font.pixelSize: root.ultraLightMode ? Theme.fontSizeTitle : Theme.scaledSize(18)
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: "Pinned paths, frequent folders, and tags will appear here."
                    visible: !root.ultraLightMode
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeLabel
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root.ultraLightMode ? 6 : 10

            StatTile {
                title: "Pinned"
                value: String(root.pinnedCount)
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/star.svg"
                accentColor: Theme.accent
            }

            StatTile {
                title: "Frequent"
                value: String(root.frequentCount)
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/folder-open.svg"
                accentColor: Theme.categoryUtility
            }

            StatTile {
                title: "Tags"
                value: String(root.favoritesBackend ? root.favoritesBackend.tagCount : 0)
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/tag.svg"
                accentColor: root.tagAccent
            }

            IconButton {
                Layout.preferredWidth: root.ultraLightMode ? 26 : 30
                Layout.preferredHeight: root.ultraLightMode ? 26 : 30
                visible: root.frequentCount > 0
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/delete.svg"
                iconTone: "delete"
                iconSize: 14
                onClicked: root.favoritesBackend.clearFrequent()
                ToolTip.visible: hovered
                ToolTip.text: "Clear Frequent"
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.modelCount > 0
            columns: root.wideFavoritesLayout ? 2 : 1
            columnSpacing: root.ultraLightMode ? 8 : 12
            rowSpacing: root.ultraLightMode ? 8 : 12

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: root.wideFavoritesLayout
                Layout.preferredHeight: root.pinnedCount > 0
                                        ? Math.min(root.ultraLightMode ? 300 : 360,
                                                   Math.max(root.ultraLightMode ? 70 : 88,
                                                            pinnedList.contentHeight + (root.ultraLightMode ? 26 : 34)))
                                        : (root.ultraLightMode ? 66 : 84)
                Layout.maximumHeight: root.wideFavoritesLayout
                                      ? 10000
                                      : (root.pinnedCount > 0
                                         ? Math.min(root.ultraLightMode ? 300 : 360,
                                                    Math.max(root.ultraLightMode ? 44 : 54,
                                                             pinnedList.contentHeight + (root.ultraLightMode ? 26 : 34)))
                                         : (root.ultraLightMode ? 66 : 84))
                spacing: root.ultraLightMode ? 4 : 6

                SectionHeader {
                    title: "Pinned"
                }

                ListView {
                    id: pinnedList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: root.ultraLightMode ? 4 : 6
                    model: root.favoritesBackend ? root.favoritesBackend.pinnedModel : null
                    currentIndex: -1
                    focus: true
                    Keys.priority: Keys.BeforeItem
                    Keys.onPressed: (event) => root.handleKey(event)
                    onMovementStarted: root.markFavoritePreviewScrollActive()
                    onFlickStarted: root.markFavoritePreviewScrollActive()
                    onMovingChanged: if (moving) root.markFavoritePreviewScrollActive()
                    onFlickingChanged: if (flicking) root.markFavoritePreviewScrollActive()
                    onContentYChanged: root.markFavoritePreviewScrollActive()
                    visible: root.pinnedCount > 0

                    delegate: FavoriteRow {
                        listView: pinnedList
                        rowPinned: true
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: pinnedScrollBar
                        policy: pinnedList.contentHeight > pinnedList.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
                        onPressedChanged: root.handleFavoriteScrollbarPressed(pressed)
                    }
                }

                EmptySectionRow {
                    visible: root.pinnedCount === 0
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons/star.svg"
                    iconColor: Theme.accent
                    title: "No pinned items"
                    subtitle: "Use Pin to Favorites from a file or folder menu."
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: root.ultraLightMode ? 4 : 6

                SectionHeader {
                    title: "Frequent"
                }

                ListView {
                    id: frequentList
                    Layout.fillWidth: true
                    Layout.fillHeight: root.wideFavoritesLayout
                    Layout.preferredHeight: root.frequentCount > 0
                                            ? Math.min(root.ultraLightMode ? 240 : 314, frequentList.contentHeight)
                                            : (root.ultraLightMode ? 36 : 44)
                    clip: true
                    spacing: root.ultraLightMode ? 4 : 6
                    model: root.favoritesBackend ? root.favoritesBackend.frequentModel : null
                    currentIndex: -1
                    focus: true
                    Keys.priority: Keys.BeforeItem
                    Keys.onPressed: (event) => root.handleKey(event)
                    onMovementStarted: root.markFavoritePreviewScrollActive()
                    onFlickStarted: root.markFavoritePreviewScrollActive()
                    onMovingChanged: if (moving) root.markFavoritePreviewScrollActive()
                    onFlickingChanged: if (flicking) root.markFavoritePreviewScrollActive()
                    onContentYChanged: root.markFavoritePreviewScrollActive()
                    visible: root.frequentCount > 0

                    delegate: FavoriteRow {
                        listView: frequentList
                        rowPinned: false
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: frequentScrollBar
                        policy: ScrollBar.AsNeeded
                        onPressedChanged: root.handleFavoriteScrollbarPressed(pressed)
                    }
                }

                EmptySectionRow {
                    visible: root.frequentCount === 0
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons/folder.svg"
                    iconColor: Theme.categoryUtility
                    title: "No frequent folders yet"
                    subtitle: "Open folders and they will appear here."
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.modelCount === 0

            EmptyState {
                anchors.centerIn: parent
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/star.svg"
                iconSize: 64
                iconOpacity: 0.58
                colorizeIcon: true
                iconColor: Theme.accent
                title: "No favorites yet"
                subtitle: "Pin files or folders, then open folders to build Frequent."
                hint: "favorites://"
                contentOpacity: 0.84
                maxTextWidth: 320
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 0
        acceptedButtons: Qt.LeftButton
        onClicked: {
            root.activated()
            root.forceActiveFocus()
        }
    }

    Dialog {
        id: labelEditDialog

        modal: true
        focus: true
        anchors.centerIn: parent
        width: Math.min(parent ? parent.width - 48 : 440, 440)
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: {
            labelEditField.forceActiveFocus()
            labelEditField.selectAll()
        }

        background: DialogShell {
            accentColor: Theme.accent
        }

        header: DialogHeader {
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/rename.svg"
            iconTint: Theme.accent
            title: "Edit Favorite Label"
            subtitle: root.labelEditTargetPath
            onCloseRequested: labelEditDialog.close()
        }

        contentItem: Item {
            implicitHeight: 126

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: "Label"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.Medium
                }

                PremiumTextField {
                    id: labelEditField

                    Layout.fillWidth: true
                    maximumLength: 160
                    placeholderText: "Use original name"
                    selectByMouse: true
                    Keys.onReturnPressed: root.applyPinnedLabelEdit()
                    Keys.onEnterPressed: root.applyPinnedLabelEdit()
                }

                Label {
                    Layout.fillWidth: true
                    text: "Empty label uses the original file or folder name."
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                }
            }
        }

        footer: DialogFooter {
            Item { Layout.fillWidth: true }

            DialogActionButton {
                text: "Cancel"
                onClicked: labelEditDialog.close()
            }

            DialogActionButton {
                text: "Save"
                highlighted: true
                primaryColor: Theme.accent
                onClicked: root.applyPinnedLabelEdit()
            }
        }
    }

    Dialog {
        id: tagEditDialog

        modal: true
        focus: true
        anchors.centerIn: parent
        width: Math.min(parent ? parent.width - 48 : 440, 440)
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: {
            tagEditField.forceActiveFocus()
            tagEditField.selectAll()
        }

        background: DialogShell {
            accentColor: root.tagAccent
        }

        header: DialogHeader {
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/info.svg"
            iconTint: root.tagAccent
            title: "Edit Favorite Tags"
            subtitle: root.tagEditTargetPath
            onCloseRequested: tagEditDialog.close()
        }

        contentItem: Item {
            implicitHeight: 126

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: "Tags"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.Medium
                }

                PremiumTextField {
                    id: tagEditField

                    Layout.fillWidth: true
                    maximumLength: 240
                    placeholderText: "work, photos, archive"
                    selectByMouse: true
                    Keys.onReturnPressed: root.applyPinnedTagsEdit()
                    Keys.onEnterPressed: root.applyPinnedTagsEdit()
                }

                Label {
                    Layout.fillWidth: true
                    text: "Separate tags with commas. Empty field clears tags."
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                }
            }
        }

        footer: DialogFooter {
            Item { Layout.fillWidth: true }

            DialogActionButton {
                text: "Cancel"
                onClicked: tagEditDialog.close()
            }

            DialogActionButton {
                text: "Save"
                highlighted: true
                primaryColor: root.tagAccent
                onClicked: root.applyPinnedTagsEdit()
            }
        }
    }

    ThemedContextMenu {
        id: favoriteContextMenu

        ThemedMenuItem {
            text: "Open in Panel"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/folder-open.svg"
            iconColor: Theme.actionIconColor("open")
            enabled: root.contextFavoriteId.length > 0 && root.contextTargetExists
            onTriggered: root.openFavorite(root.contextFavoriteId)
        }

        ThemedMenuItem {
            text: "Open File"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/open.svg"
            iconColor: Theme.actionIconColor("open")
            visible: !root.contextTargetIsDirectory
            enabled: visible && root.contextTargetExists
            onTriggered: root.favoritesBackend.openPath(root.contextTargetPath)
        }

        ThemedMenuSeparator {}

        ThemedMenuItem {
            text: "Edit Label"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/rename.svg"
            iconColor: Theme.actionIconColor("rename")
            visible: root.selectedIsPinned
            enabled: visible && root.contextTargetPath.length > 0
            onTriggered: root.editSelectedPinnedLabel()
        }

        ThemedMenuItem {
            text: "Edit Tags"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/tag.svg"
            iconColor: root.tagAccent
            visible: root.selectedIsPinned
            enabled: visible && root.contextTargetPath.length > 0
            onTriggered: root.editSelectedPinnedTags()
        }

        ThemedMenuItem {
            text: "Unpin from Favorites"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/star-off.svg"
            iconColor: Theme.actionIconColor("favorite")
            visible: root.selectedIsPinned
            enabled: visible && root.contextTargetPath.length > 0
            onTriggered: root.removeFavorite(root.contextTargetPath)
        }

        ThemedMenuSeparator {
            visible: root.selectedIsPinned
        }

        ThemedMenuItem {
            text: "Move Up"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/arrow-up.svg"
            iconColor: Theme.actionIconColor("move")
            visible: root.selectedIsPinned
            enabled: visible && pinnedList.currentIndex > 0
            onTriggered: root.moveSelectedPinned(-1)
        }

        ThemedMenuItem {
            text: "Move Down"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/arrow-down.svg"
            iconColor: Theme.actionIconColor("move")
            visible: root.selectedIsPinned
            enabled: visible && pinnedList.currentIndex >= 0 && pinnedList.currentIndex < root.pinnedCount - 1
            onTriggered: root.moveSelectedPinned(1)
        }

        ThemedMenuSeparator {
            visible: root.selectedIsPinned
        }

        ThemedMenuItem {
            text: Qt.platform.os === "windows" ? "Show in Explorer"
                  : Qt.platform.os === "osx" ? "Reveal in Finder"
                  : "Open Containing Folder"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/reveal.svg"
            iconColor: Theme.actionIconColor("navigation")
            visible: root.contextTargetIsDirectory
            enabled: visible && root.contextTargetPath.length > 0 && root.contextTargetExists
            onTriggered: {
                if (root.favoritesBackend) {
                    root.favoritesBackend.revealPath(root.contextTargetPath)
                }
            }
        }

        ThemedMenuItem {
            text: "Copy Path"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/clipboard-copy.svg"
            iconColor: Theme.actionIconColor("copy")
            enabled: root.contextTargetPath.length > 0
            onTriggered: {
                if (typeof workspaceController !== "undefined" && workspaceController) {
                    workspaceController.copyTextToClipboard(root.contextTargetPath)
                }
            }
        }

        ThemedMenuItem {
            text: Qt.platform.os === "windows" ? "Open in PowerShell" : "Open in Terminal"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/terminal.svg"
            iconColor: Theme.actionIconColor("terminal")
            visible: root.contextTargetIsDirectory
            enabled: visible && root.contextTargetPath.length > 0 && root.contextTargetExists
            onTriggered: {
                if (root.favoritesBackend) {
                    root.favoritesBackend.openTerminalAtPath(root.contextTargetPath)
                }
            }
        }

        ThemedMenuSeparator {}

        ThemedMenuItem {
            text: "Properties"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons/info.svg"
            iconColor: Theme.actionIconColor("info")
            enabled: root.contextTargetPath.length > 0 && root.contextTargetExists
            onTriggered: {
                if (typeof propertiesController !== "undefined" && propertiesController) {
                    propertiesController.load(root.contextTargetPath)
                }
            }
        }
    }
}
