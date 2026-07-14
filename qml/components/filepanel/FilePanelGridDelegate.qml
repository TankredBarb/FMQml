import "../../style"
import "../common"
import FM
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

Item {
    id: gridDelegate

    required property var panel
    required property var view
    required property var theme
    required property int index
    required property string name
    required property string path
    required property string iconName
    required property string suffix
    required property string mimeType
    required property bool isDirectory
    required property bool isSelected
    required property bool isHidden
    required property bool isImage
    required property bool hasThumbnail
    required property int thumbnailRevision
    required property bool isArchiveFile
    required property bool isIsoImageFile
    required property string primaryBadgeKind
    required property bool isPinned
    property bool isRenaming: false
    property bool currentItem: GridView.isCurrentItem
    property bool panelActive: panel.active
    property real dragStartX: 0
    property real dragStartY: 0
    property bool dragCandidate: false
    property bool dragStarted: false
    property bool badgePressed: false
    property bool suppressClickAfterDrag: false
    property string thumbnailFailedPath: ""
    property int thumbnailRetryAttempt: 0
    property int thumbnailRetryRevision: 0
    readonly property bool lightweightActive: panel.lightweightDelegates && !isRenaming
    readonly property color selectedStateFill: Theme.withAlpha(Theme.activeAccent, theme.isDark ? (panel.active ? 0.34 : 0.2) : (panel.active ? 0.28 : 0.16))
    readonly property color currentStateFill: Theme.withAlpha(Theme.activeAccent, theme.isDark ? (panel.active ? 0.18 : 0.11) : (panel.active ? 0.14 : 0.09))
    readonly property int contentMargin: 10
    readonly property int contentSpacing: 6
    readonly property int renameEditorTop: contentMargin + panel.gridIconSize + contentSpacing
    readonly property int renameEditorSideMargin: contentMargin
    readonly property int renameEditorAvailableHeight: Math.max(30, height - renameEditorTop - contentMargin)
    readonly property bool canLoadThumbnail: panel.effectiveUseNativeIcons && panel.effectiveShowThumbnails && !panel.thumbnailLoadingPaused && !gridDelegate.lightweightActive && !isDirectory && hasThumbnail && gridDelegate.thumbnailFailedPath !== path
    readonly property bool canScheduleThumbnail: gridDelegate.canLoadThumbnail && !panel.thumbnailSchedulingPaused
    property bool thumbnailLoadEnabled: false
    readonly property bool thumbnailRequestActive: thumbnailLoadEnabled && canLoadThumbnail
    property real visualOffsetY: 0

    function startRename() {
        isRenaming = true;
    }

    function cancelRename() {
        isRenaming = false;
    }

    function focusRenameEditor(selectText) {
        if (!isRenaming || !gridRenameLoader.item) {
            panel.traceRenameFocus("grid-focusRenameEditor-reject", "path=" + path + " hasItem=" + Boolean(gridRenameLoader.item));
            return false;
        }
        panel.traceRenameFocus("grid-focusRenameEditor-before", "path=" + path + " select=" + (selectText === true));
        gridRenameLoader.item.forceActiveFocus();
        if (selectText === true)
            gridRenameLoader.item.select(0, gridRenameLoader.item.defaultSelectionEnd());

        panel.traceRenameFocus("grid-focusRenameEditor-after", "path=" + path + " activeFocus=" + gridRenameLoader.item.activeFocus);
        return gridRenameLoader.item.activeFocus;
    }

    function renameEditorHasFocus() {
        return Boolean(gridRenameLoader.item && gridRenameLoader.item.activeFocus);
    }

    function isPointOnBadge(x, y) {
        function findBadge(item) {
            if (item.objectName === "gridSelectionBadge")
                return item;

            for (var i = 0; i < item.children.length; i++) {
                var result = findBadge(item.children[i]);
                if (result)
                    return result;

            }
            return null;
        }

        var badge = findBadge(gridDelegate);
        if (!badge || !badge.visible)
            return false;

        var mapped = badge.mapFromItem(gridMouseArea, x, y);
        return mapped.x >= 0 && mapped.y >= 0 && mapped.x < badge.width && mapped.y < badge.height;
    }

    function isPointOnDragSurface(x, y) {
        function within(item) {
            if (!item || !item.visible)
                return false;

            const mapped = item.mapFromItem(gridDelegate, x, y);
            return mapped.x >= 0 && mapped.y >= 0 && mapped.x < item.width && mapped.y < item.height;
        }

        return within(gridIconCell);
    }

    function queueThumbnailLoad(clearExisting) {
        if (clearExisting === true || !canLoadThumbnail)
            thumbnailLoadEnabled = false;

        if (canScheduleThumbnail && !thumbnailLoadEnabled)
            thumbnailDelayTimer.restart();
        else
            thumbnailDelayTimer.stop();
    }

    function scheduleThumbnailRetry() {
        if (!thumbnailRequestActive || thumbnailRetryAttempt >= 3)
            return ;

        thumbnailRetryTimer.interval = 350 * Math.pow(2, thumbnailRetryAttempt);
        thumbnailRetryTimer.restart();
    }

    function itemStateFill(hovered, fallbackColor) {
        if (gridDelegate.isSelected)
            return gridDelegate.selectedStateFill;

        if (gridDelegate.currentItem)
            return gridDelegate.currentStateFill;

        if (hovered && !panel.hoverSuppressed)
            return Theme.itemNeutralHoverFill;

        return fallbackColor;
    }

    width: view.cellWidth
    height: view.cellHeight
    z: isRenaming ? 100 : 0
    opacity: isHidden ? 0.55 : 1
    onPathChanged: {
        isRenaming = false;
        visualOffsetY = 0;
        thumbnailFailedPath = "";
        thumbnailRetryAttempt = 0;
        thumbnailRetryRevision = 0;
        queueThumbnailLoad(true);
        if (gridDelegate.lightweightActive)
            return ;

        Qt.callLater(() => {
            if (hoverGrid) {
                hoverGrid.enabled = false;
                hoverGrid.enabled = true;
            }
        });
    }
    onThumbnailRevisionChanged: {
        thumbnailFailedPath = "";
        thumbnailRetryAttempt = 0;
        thumbnailRetryRevision = 0;
        if (!thumbnailLoadEnabled)
            queueThumbnailLoad();

    }
    Component.onCompleted: {
        queueThumbnailLoad(true);
        if (gridDelegate.lightweightActive)
            return ;

        Qt.callLater(() => {
            if (hoverGrid) {
                hoverGrid.enabled = false;
                hoverGrid.enabled = true;
            }
        });
    }
    GridView.onPooled: {
        isRenaming = false;
        visualOffsetY = 0;
        thumbnailLoadEnabled = false;
        thumbnailFailedPath = "";
        thumbnailRetryAttempt = 0;
        thumbnailRetryRevision = 0;
        // A path can still be visible in the other panel. Cancellation needs
        // per-delegate ownership before pooled delegates may cancel shared jobs.
        if (panel.controller.hoveredPath === path)
            panel.clearHoveredItem(path);

    }
    GridView.onReused: {
        isRenaming = false;
        visualOffsetY = 0;
        thumbnailFailedPath = "";
        thumbnailRetryAttempt = 0;
        thumbnailRetryRevision = 0;
        queueThumbnailLoad(true);
        opacity = Qt.binding(() => {
            return isHidden ? 0.55 : 1;
        });
        if (gridDelegate.lightweightActive)
            return ;

        Qt.callLater(() => {
            if (hoverGrid) {
                hoverGrid.enabled = false;
                hoverGrid.enabled = true;
            }
        });
    }

    Connections {
        function onResizeOptimizedChanged() {
            gridDelegate.queueThumbnailLoad();
        }

        function onUltraLightModeChanged() {
            gridDelegate.queueThumbnailLoad();
        }

        function onThumbnailLoadingPausedChanged() {
            gridDelegate.queueThumbnailLoad();
        }

        function onThumbnailSchedulingPausedChanged() {
            gridDelegate.queueThumbnailLoad();
        }

        target: root
    }

    Timer {
        id: thumbnailDelayTimer

        interval: 100 + (Math.max(0, index) % 16) * 28
        repeat: false
        onTriggered: {
            gridDelegate.thumbnailLoadEnabled = gridDelegate.canScheduleThumbnail;
            if (gridDelegate.thumbnailLoadEnabled && typeof thumbnailController !== "undefined" && thumbnailController)
                thumbnailController.requestThumbnail(path, panel.gridIconSize * 2, panel.gridIconSize * 2, 100, "visible");

        }
    }

    Timer {
        id: thumbnailRetryTimer

        repeat: false
        onTriggered: {
            if (!gridDelegate.thumbnailRequestActive)
                return ;

            gridDelegate.thumbnailRetryAttempt += 1;
            gridDelegate.thumbnailRetryRevision += 1;
        }
    }

    Rectangle {
        id: gridSelectionBackground

        anchors.fill: parent
        anchors.margins: 4
        visible: !gridDelegate.lightweightActive
        radius: Theme.radiusMd
        color: gridDelegate.itemStateFill(hoverGrid.hovered, "transparent")
        border.color: "transparent"
        border.width: 0

        transform: Translate {
            y: gridDelegate.visualOffsetY
        }

    }

    HoverHandler {
        id: hoverGrid

        enabled: !gridDelegate.lightweightActive && !panel.externalScrollAnySuppressionActive
        cursorShape: panel.internalDragEnabled ? panel.itemHoverCursorShape(gridDelegate, point.position.x, point.position.y) : Qt.ArrowCursor
        onHoveredChanged: {
            if (panel.hoverSuppressed)
                return ;

            if (hovered) {
                panel.setHoveredItem(gridDelegate, path, point.position);
                if (panel.internalDragEnabled)
                    panel.updateHoverDragCursor(gridDelegate, point.position.x, point.position.y);

            } else {
                panel.clearHoveredItem(path);
                panel.clearHoverDragCursor(gridDelegate);
            }
        }
        onPointChanged: {
            if (hovered)
                panel.setHoveredItem(gridDelegate, path, point.position);

            if (hovered && panel.internalDragEnabled)
                panel.updateHoverDragCursor(gridDelegate, point.position.x, point.position.y);

        }
    }

    Connections {
        function onHoverSuppressedChanged() {
            if (!panel.hoverSuppressed && !gridDelegate.lightweightActive) {
                Qt.callLater(() => {
                    if (hoverGrid) {
                        hoverGrid.enabled = false;
                        hoverGrid.enabled = true;
                        if (hoverGrid.hovered)
                            panel.setHoveredItem(gridDelegate, path, hoverGrid.point.position);

                    }
                });
            }
        }

        target: root
    }

    Connections {
        function onUseNativeIconsChanged() {
            gridDelegate.queueThumbnailLoad();
        }

        function onShowThumbnailsChanged() {
            gridDelegate.queueThumbnailLoad();
        }

        target: typeof appSettings !== "undefined" ? appSettings : null
        ignoreUnknownSignals: true
    }

    Connections {
        function onLoadingChanged() {
            if (panel.controller && panel.controller.directoryModel && !panel.controller.directoryModel.loading && !gridDelegate.lightweightActive) {
                Qt.callLater(() => {
                    if (hoverGrid) {
                        hoverGrid.enabled = false;
                        hoverGrid.enabled = true;
                        if (hoverGrid.hovered)
                            panel.setHoveredItem(gridDelegate, path, hoverGrid.point.position);

                    }
                });
            }
        }

        target: panel.controller ? panel.controller.directoryModel : null
        ignoreUnknownSignals: true
    }

    Loader {
        id: gridRenameLoader

        z: 20
        anchors.top: parent.top
        anchors.topMargin: gridDelegate.renameEditorTop
        width: Math.max(0, parent.width - gridDelegate.renameEditorSideMargin * 2)
        height: Math.min(36, gridDelegate.renameEditorAvailableHeight)
        x: gridDelegate.renameEditorSideMargin
        active: isRenaming
        visible: active

        sourceComponent: TextField {
            id: gridRenameInput

            property bool committing: false
            property bool canceling: false

            function commitRename() {
                if (index >= 0) {
                    const idx = index;
                    const txt = text.trim();
                    const ctrl = panel.controller;
                    committing = true;
                    Qt.callLater(function() {
                        const adminRenameAvailable = panel.Window.window && panel.Window.window.adminModeActive && panel.Window.window.adminModeActive() && ctrl.pathKindFor(path) === "local" && ctrl.renameAsAdministrator;
                        const renamed = adminRenameAvailable ? ctrl.renameAsAdministrator(idx, txt) : ctrl.rename(idx, txt);
                        if (renamed) {
                            isRenaming = false;
                        } else {
                            committing = false;
                            if (gridRenameLoader.item) {
                                gridRenameLoader.item.forceActiveFocus();
                                gridRenameLoader.item.selectAll();
                            }
                        }
                    });
                }
            }

            function defaultSelectionEnd() {
                const lastDot = name.lastIndexOf(".");
                return !isDirectory && lastDot > 0 ? lastDot : name.length;
            }

            text: name
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            color: Theme.textPrimary
            selectByMouse: true
            leftPadding: 10
            rightPadding: 10
            topPadding: 4
            bottomPadding: 4
            selectionColor: Theme.withAlpha(Theme.focusRing, theme.isDark ? 0.38 : 0.24)
            selectedTextColor: Theme.textPrimary
            clip: true
            opacity: 0
            scale: 0.97
            Keys.priority: Keys.AfterItem
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_A && (event.modifiers & Qt.ControlModifier)) {
                    gridRenameInput.selectAll();
                    event.accepted = true;
                    return ;
                }
                if (event.key === Qt.Key_F2) {
                    if (gridRenameInput.selectionStart === 0 && gridRenameInput.selectionEnd === gridRenameInput.defaultSelectionEnd())
                        gridRenameInput.selectAll();
                    else
                        gridRenameInput.select(0, gridRenameInput.defaultSelectionEnd());
                    event.accepted = true;
                    return ;
                }
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Home || event.key === Qt.Key_End || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown)
                    event.accepted = true;

            }
            Keys.onReturnPressed: (event) => {
                gridRenameInput.commitRename();
                event.accepted = true;
            }
            Keys.onEnterPressed: (event) => {
                gridRenameInput.commitRename();
                event.accepted = true;
            }
            Keys.onEscapePressed: (event) => {
                canceling = true;
                panel.traceRenameFocus("grid-escape-cancel", "path=" + path);
                isRenaming = false;
                panel.cancelInlineRename();
                event.accepted = true;
            }
            onActiveFocusChanged: {
                panel.traceRenameFocus("grid-textField-activeFocus-changed", "path=" + path + " value=" + activeFocus);
                if (!activeFocus && isRenaming && !committing && !canceling)
                    panel.recoverInlineRenameFocus("grid-editor-focus-lost");

            }
            Component.onCompleted: {
                panel.traceRenameFocus("grid-textField-completed-before-focus", "path=" + path);
                opacity = 1;
                scale = 1;
                forceActiveFocus();
                select(0, gridRenameInput.defaultSelectionEnd());
                panel.traceRenameFocus("grid-textField-completed-after-focus", "path=" + path + " activeFocus=" + activeFocus);
            }

            Behavior on opacity {
                enabled: !panel.effectsReduced

                NumberAnimation {
                    duration: 100
                    easing.type: Easing.OutQuad
                }

            }

            Behavior on scale {
                enabled: !panel.effectsReduced

                NumberAnimation {
                    duration: 100
                    easing.type: Easing.OutQuad
                }

            }

            background: Rectangle {
                color: Theme.withAlpha(Theme.panelSurfaceStrong, theme.isDark ? 0.92 : 0.96)
                radius: Theme.controlRadius
                border.color: gridRenameInput.activeFocus ? Theme.withAlpha(Theme.focusRing, 0.9) : Theme.withAlpha(Theme.panelBorder, 0.7)
                border.width: gridRenameInput.activeFocus ? 1.25 : 1
                layer.enabled: !panel.effectsReduced

                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Theme.withAlpha(Theme.shadow, theme.isDark ? 0.22 : 0.12)
                    shadowBlur: 8
                    shadowVerticalOffset: 1
                }

            }

        }

    }

    FileGridResizeDelegate {
        anchors.fill: parent
        visible: gridDelegate.lightweightActive
        z: 5
        panel: root
        index: gridDelegate.index
        name: gridDelegate.name
        path: gridDelegate.path
        iconName: gridDelegate.iconName
        suffix: gridDelegate.suffix
        mimeType: gridDelegate.mimeType
        isDirectory: gridDelegate.isDirectory
        isSelected: gridDelegate.isSelected
        isHidden: gridDelegate.isHidden
        isArchiveFile: gridDelegate.isArchiveFile
        isIsoImageFile: gridDelegate.isIsoImageFile
        primaryBadgeKind: gridDelegate.primaryBadgeKind
        isPinned: gridDelegate.isPinned
        currentItem: gridDelegate.currentItem
        panelActive: gridDelegate.panelActive
        gridIconSize: panel.gridIconSize
        onClicked: (mouse) => {
            return panel.handleItemClick(index, mouse);
        }
        onRightClicked: panel.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
        onDoubleClicked: panel.openItem(index)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: gridDelegate.contentMargin
        spacing: gridDelegate.contentSpacing
        visible: !gridDelegate.lightweightActive

        Item {
            id: gridIconFrame

            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: panel.gridIconSize
            Layout.preferredHeight: panel.gridIconSize

            FileIconCell {
                id: gridIconCell

                anchors.centerIn: parent
                width: Math.max(28, Math.round(panel.gridIconSize * 0.8))
                height: width
                path: gridDelegate.path
                name: gridDelegate.name
                iconName: gridDelegate.iconName
                mimeType: gridDelegate.mimeType
                isDirectory: gridDelegate.isDirectory
                hasThumbnail: gridDelegate.hasThumbnail
                primaryBadgeKind: gridDelegate.primaryBadgeKind
                isPinned: gridDelegate.isPinned
                suffix: gridDelegate.suffix
                useNativeIcons: panel.effectiveUseNativeIcons
                thumbnailSource: gridDelegate.thumbnailRequestActive ? panel.thumbnailSourceFor(gridDelegate.path, gridDelegate.thumbnailRevision + gridDelegate.thumbnailRetryRevision * 1e+06) : ""
                showThumbnail: gridDelegate.thumbnailRequestActive
                iconSize: width
                onThumbnailError: {
                    gridDelegate.thumbnailFailedPath = gridDelegate.path;
                    gridDelegate.thumbnailLoadEnabled = false;
                }
                onThumbnailSoftMiss: gridDelegate.scheduleThumbnailRetry()
            }

        }

        Label {
            Layout.fillWidth: true
            visible: !isRenaming
            text: {
                if (isDirectory || !suffix)
                    return name;

                const extLen = suffix.length;
                if (extLen > 0 && name.endsWith("." + suffix)) {
                    const baseName = name.substring(0, name.length - extLen - 1);
                    return baseName + "<font color='" + TextColors.fileExtensionText.toString() + "'>." + suffix + "</font>";
                }
                return name;
            }
            textFormat: Text.StyledText
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            color: isDirectory ? TextColors.folderNameText : TextColors.fileNameText
            wrapMode: Text.Wrap
            maximumLineCount: 2
        }

        transform: Translate {
            y: gridDelegate.visualOffsetY
        }

    }

    MouseArea {
        id: gridMouseArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        visible: !gridDelegate.lightweightActive
        scrollGestureEnabled: false
        preventStealing: gridDelegate.dragCandidate || gridDelegate.badgePressed
        cursorShape: panel.internalDragEnabled && containsMouse ? panel.itemHoverCursorShape(gridDelegate, mouseX, mouseY) : Qt.ArrowCursor
        onWheel: (wheel) => {
            wheel.accepted = false;
        }
        onPressed: (mouse) => {
            panel.cancelInlineRenameForNavigation("grid-item-press");
            gridDelegate.badgePressed = mouse.button === Qt.LeftButton && gridDelegate.isPointOnBadge(mouse.x, mouse.y);
            gridDelegate.dragCandidate = panel.internalDragEnabled && mouse.button === Qt.LeftButton && !gridDelegate.isRenaming && !gridDelegate.badgePressed && (gridDelegate.isSelected || gridDelegate.isPointOnDragSurface(mouse.x, mouse.y));
            gridDelegate.dragStarted = false;
            gridDelegate.dragStartX = mouse.x;
            gridDelegate.dragStartY = mouse.y;
        }
        onPositionChanged: (mouse) => {
            if (gridDelegate.badgePressed)
                return ;

            if (!gridDelegate.dragCandidate)
                return ;

            if (gridDelegate.dragStarted) {
                panel.updateSelectionDragPosition(mouse, gridDelegate);
            } else {
                gridDelegate.dragStarted = panel.updateSelectionDragCandidate(gridDelegate.index, gridDelegate.path, gridDelegate.dragStartX, gridDelegate.dragStartY, mouse.x, mouse.y, mouse);
                if (gridDelegate.dragStarted)
                    panel.updateSelectionDragPosition(mouse, gridDelegate);

            }
        }
        onReleased: (mouse) => {
            if (gridDelegate.dragStarted) {
                panel.finishSelectionDrag(mouse, gridDelegate);
                gridDelegate.suppressClickAfterDrag = true;
                gridSuppressClickReset.restart();
            }
            gridDelegate.dragCandidate = false;
            gridDelegate.dragStarted = false;
            gridDelegate.badgePressed = false;
        }
        onCanceled: {
            if (gridDelegate.dragStarted && panel.internalDragEnabled && panel.dragCoordinator)
                panel.dragCoordinator.cancelDrag("Drag canceled.");

            gridDelegate.dragCandidate = false;
            gridDelegate.dragStarted = false;
            gridDelegate.badgePressed = false;
        }
        onClicked: (mouse) => {
            if (gridDelegate.suppressClickAfterDrag) {
                gridDelegate.suppressClickAfterDrag = false;
                return ;
            }
            if (mouse.button === Qt.RightButton)
                panel.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile);
            else if (gridDelegate.isPointOnBadge(mouse.x, mouse.y))
                panel.controller.directoryModel.toggleSelected(gridDelegate.index);
            else
                panel.handleItemClick(index, mouse);
        }
        onDoubleClicked: (mouse) => {
            panel.openItem(index);
        }
    }

    Timer {
        id: gridSuppressClickReset

        interval: 0
        repeat: false
        onTriggered: gridDelegate.suppressClickAfterDrag = false
    }

    SelectionToggleBadge {
        objectName: "gridSelectionBadge"
        x: 8
        y: 8 + gridDelegate.visualOffsetY
        z: 30
        badgeSize: Math.max(16, Math.min(21, Math.round(panel.gridIconSize * 0.24)))
        markSize: Math.max(7, Math.round(badgeSize * 0.4))
        markStroke: badgeSize >= 22 ? 1.2 : 1
        available: panel.showSelectionBadges && !gridDelegate.lightweightActive
        controller: panel.controller
        panel: root
        index: gridDelegate.index
        selected: gridDelegate.isSelected
        hovered: hoverGrid.hovered
        currentItem: gridDelegate.currentItem
        scrolling: panel.hoverSuppressed || gridDelegate.isRenaming
    }

}
