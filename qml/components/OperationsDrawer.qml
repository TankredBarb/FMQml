import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Window
import FM
import "common"
import "../style"

Item {
    id: root

    readonly property var queue: workspaceController.operationQueue
    readonly property var operationErrorInfo: queue.lastError || ({})
    readonly property bool hasOperationError: queue.error.length > 0
    readonly property string operationErrorTitle: operationErrorInfo.title || "Operation failed"
    readonly property string operationErrorMessage: operationErrorInfo.message || queue.error
    readonly property string operationErrorPath: operationErrorInfo.path || ""
    readonly property string operationErrorDisplayPath: workspaceController && workspaceController.displayPath
                                                        ? workspaceController.displayPath(operationErrorPath)
                                                        : operationErrorPath
    readonly property string operationErrorItemSummary: operationErrorInfo.itemSummary || ""
    readonly property int operationErrorItemCount: Number(operationErrorInfo.itemCount || 0)
    readonly property var operationErrorActions: operationErrorInfo.actions || []
    readonly property bool canRetry: operationErrorActions.indexOf("retry") >= 0
    readonly property bool canRefresh: operationErrorActions.indexOf("refresh") >= 0
    readonly property bool canCopyPath: operationErrorActions.indexOf("copyPath") >= 0 && operationErrorPath.length > 0
    readonly property bool canRestartAsAdmin: operationErrorInfo.code === "accessDenied"
                                              && operationErrorActions.indexOf("restartAsAdmin") >= 0
                                              && typeof adminController !== "undefined"
                                              && adminController
                                              && adminController.canRelaunchAsAdmin
                                              && !adminController.isElevated
    readonly property bool busy: queue.busy
    readonly property bool active: busy || queue.error.length > 0
    readonly property bool chipVisible: active && !expanded
    readonly property bool cardVisible: active && expanded
    readonly property string chipTitle: hasOperationError ? operationErrorTitle : "File operations"
    readonly property string chipSubtitle: hasOperationError
                                           ? operationErrorMessage
                                           : (queue.currentLabel || "Preparing...")
    readonly property string chipMeta: busy
                                       ? (queue.completedItems + "/" + queue.totalItems)
                                       : "Review"
    readonly property color solidPanelSurface: root.opaque(Theme.panelSurface)
    readonly property color solidPanelSurfaceSoft: root.opaque(Theme.panelSurfaceSoft)
    readonly property color solidPanelSurfaceStrong: root.opaque(Theme.panelSurfaceStrong)
    readonly property color solidSurfaceActive: root.opaque(Theme.surfaceActive)
    readonly property color drawerSurface: root.opaque(themeController.isDark ? Theme.panelSurfaceStrong : Theme.panelSurface)
    readonly property color drawerBorder: root.hasOperationError
                                           ? root.dangerBorder
                                           : (themeController.isDark ? Theme.panelStroke : Theme.panelStrokeStrong)
    readonly property color drawerWash: Theme.withAlpha(
        root.hasOperationError ? Theme.danger : Theme.accent,
        themeController.isDark ? 0.026 : 0.038)
    readonly property color quietBorder: themeController.isDark ? Theme.panelStroke : Theme.panelStrokeStrong
    readonly property color accentBorder: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.44 : 0.34)
    readonly property color dangerBorder: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.58 : 0.46)
    readonly property color warningBorder: Theme.withAlpha(Theme.warning, themeController.isDark ? 0.52 : 0.40)
    readonly property int compactWidth: Theme.scaledSize(248)
    readonly property int expandedWidth: Theme.scaledSize(360)
    readonly property int compactIconSize: Theme.scaledSize(28)
    readonly property int expandedIconSize: Theme.scaledSize(40)
    readonly property int smallPillHeight: Theme.scaledSize(20)
    readonly property int actionButtonHeight: Theme.scaledSize(34)

    property bool expanded: false
    property int previewDelayMs: 900
    property int previewVisibleMs: 1200
    property int errorDismissMs: 3000
    property bool userPinnedExpanded: false

    implicitWidth: expanded ? expandedWidth : compactWidth
    implicitHeight: expanded ? expandedCard.height : compactChip.height
    visible: active
    y: active ? 0 : 18

    Behavior on y { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
    Behavior on implicitWidth { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    Behavior on implicitHeight { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    function opaque(c) {
        return Qt.rgba(c.r, c.g, c.b, 1)
    }

    function expand(pin) {
        if (pin === true) {
            userPinnedExpanded = true
        }
        expanded = true
        scheduleCollapse()
    }

    function collapse(force) {
        if (force === true) {
            userPinnedExpanded = false
        }
        if (hasOperationError) {
            expanded = true
            return
        }
        if (!busy) {
            expanded = false
            userPinnedExpanded = false
            return
        }
        if (!userPinnedExpanded) {
            expanded = false
        }
    }

    function scheduleCollapse() {
        collapseTimer.stop()
        if (!active || hasOperationError || !expanded || userPinnedExpanded || expandedHover.hovered) {
            return
        }
        collapseTimer.restart()
    }

    function scheduleErrorDismiss() {
        errorDismissTimer.stop()
        if (!hasOperationError) {
            return
        }
        errorDismissTimer.restart()
    }

    Timer {
        id: previewDelayTimer
        interval: root.previewDelayMs
        repeat: false
        onTriggered: {
            if (root.busy && !root.hasOperationError && !root.userPinnedExpanded) {
                root.expanded = true
                root.scheduleCollapse()
            }
        }
    }

    Timer {
        id: collapseTimer
        interval: root.previewVisibleMs
        repeat: false
        onTriggered: root.collapse(false)
    }

    Timer {
        id: errorDismissTimer
        interval: root.errorDismissMs
        repeat: false
        onTriggered: {
            if (root.hasOperationError) {
                root.queue.clearError()
            }
        }
    }

    Connections {
        target: root.queue

        function onBusyChanged() {
            if (!root.active) {
                root.expanded = false
                root.userPinnedExpanded = false
                previewDelayTimer.stop()
                collapseTimer.stop()
                errorDismissTimer.stop()
                return
            }

            if (root.hasOperationError) {
                root.expanded = true
                previewDelayTimer.stop()
                collapseTimer.stop()
                root.scheduleErrorDismiss()
                return
            }

            if (root.busy) {
                if (!root.userPinnedExpanded && !root.expanded) {
                    previewDelayTimer.restart()
                }
            }
        }

        function onErrorChanged() {
            if (root.hasOperationError) {
                root.expanded = true
                root.userPinnedExpanded = false
                previewDelayTimer.stop()
                collapseTimer.stop()
                root.scheduleErrorDismiss()
            } else if (root.busy) {
                if (!root.userPinnedExpanded && !root.expanded) {
                    previewDelayTimer.restart()
                } else {
                    root.scheduleCollapse()
                }
            } else {
                root.expanded = false
                root.userPinnedExpanded = false
                previewDelayTimer.stop()
                collapseTimer.stop()
                errorDismissTimer.stop()
            }
        }

        function onCurrentLabelChanged() {
            if (root.busy && root.expanded && !root.userPinnedExpanded) {
                root.scheduleCollapse()
            }
        }
    }

    AmbientPanelBackground {
        id: compactChip
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: root.width
        height: Math.max(Theme.scaledSize(54), compactContent.implicitHeight + Theme.scaledSize(20))
        cornerRadius: Theme.scaledSize(16)
        visible: root.chipVisible
        baseColor: root.drawerSurface
        strength: 0.46
        border.color: root.drawerBorder
        border.width: 1

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.58
            shadowVerticalOffset: 9
            shadowOpacity: themeController.isDark ? 0.26 : 0.20
            shadowColor: Theme.shadow
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Math.max(0, parent.cornerRadius - 1)
            color: root.drawerWash
            border.color: "transparent"
            border.width: 0
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 2
            radius: 1
            color: root.hasOperationError ? Theme.danger : Theme.accent
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.expand(true)
        }

        RowLayout {
            id: compactContent
            anchors.fill: parent
            anchors.leftMargin: Theme.scaledSize(12)
            anchors.rightMargin: Theme.scaledSize(12)
            spacing: Theme.scaledSize(10)

            Rectangle {
                Layout.preferredWidth: root.compactIconSize
                Layout.preferredHeight: root.compactIconSize
                radius: Theme.scaledSize(9)
                color: root.solidPanelSurface
                border.color: root.hasOperationError ? root.dangerBorder : root.accentBorder
                border.width: 1

                RecolorSvgIcon {
                    id: compactIcon
                    anchors.centerIn: parent
                    width: Theme.scaledSize(14)
                    height: Theme.scaledSize(14)
                    sourcePath: root.hasOperationError
                            ? "../assets/icons/info.svg"
                            : "../assets/icons/refresh.svg"
                    recolorColor: root.hasOperationError ? Theme.danger : Theme.accent
                    sourceSize: Qt.size(20, 20)
                    fillMode: Image.PreserveAspectFit

                    RotationAnimation on rotation {
                        from: 0
                        to: 360
                        duration: 1800
                        loops: Animation.Infinite
                        running: root.busy && !root.hasOperationError
                    }

                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.scaledSize(2)

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.scaledSize(8)

                    Label {
                        Layout.fillWidth: true
                        text: root.chipTitle
                        color: root.hasOperationError ? Theme.danger : Theme.textPrimary
                        font.pixelSize: Theme.fontSizeCaption
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        text: root.chipMeta
                        color: root.hasOperationError ? Theme.danger : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeMicro
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.maximumWidth: Math.round(root.width * 0.28)
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: root.chipSubtitle
                    color: root.hasOperationError ? Theme.danger : Theme.textSecondary
                    font.pixelSize: Theme.fontSizeMicro
                    elide: Text.ElideMiddle
                    maximumLineCount: 1
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4
                    radius: 2
                    color: root.quietBorder
                    visible: root.busy

                    Rectangle {
                        width: Math.max(4, parent.width * Math.max(0, Math.min(1, root.queue.progress)))
                        height: parent.height
                        radius: 2
                        color: root.hasOperationError ? Theme.danger : Theme.accent
                    }
                }
            }
        }
    }

    AmbientPanelBackground {
        id: expandedCard
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: root.width
        height: content.implicitHeight + Theme.scaledSize(28)
        cornerRadius: Theme.scaledSize(18)
        visible: root.cardVisible
        scale: visible ? 1.0 : 0.98
        baseColor: root.drawerSurface
        strength: 0.50
        border.color: root.drawerBorder
        border.width: 1

        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.64
            shadowVerticalOffset: 10
            shadowOpacity: themeController.isDark ? 0.30 : 0.24
            shadowColor: Theme.shadow
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Math.max(0, parent.cornerRadius - 1)
            color: root.drawerWash
            border.color: "transparent"
            border.width: 0
        }

        HoverHandler {
            id: expandedHover
            onHoveredChanged: {
                root.scheduleCollapse()
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            radius: 1.5
            color: root.hasOperationError ? Theme.danger : Theme.accent
        }

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: Theme.scaledSize(14)
            spacing: Theme.scaledSize(10)

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.scaledSize(12)

                Rectangle {
                    Layout.preferredWidth: root.expandedIconSize
                    Layout.preferredHeight: root.expandedIconSize
                    radius: Theme.scaledSize(12)
                    color: root.solidPanelSurface
                    border.color: root.hasOperationError ? root.dangerBorder : root.accentBorder
                    border.width: 1

                    RecolorSvgIcon {
                        anchors.centerIn: parent
                        width: Theme.scaledSize(20)
                        height: Theme.scaledSize(20)
                        sourcePath: root.hasOperationError
                                ? "../assets/icons/info.svg"
                                : "../assets/icons/refresh.svg"
                        recolorColor: root.hasOperationError ? Theme.danger : Theme.accent
                        sourceSize: Qt.size(20, 20)

                        RotationAnimation on rotation {
                            from: 0
                            to: 360
                            duration: 1800
                            loops: Animation.Infinite
                            running: root.busy && !root.hasOperationError
                        }

                    }
                }

                ColumnLayout {
                    spacing: Theme.scaledSize(2)
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0

                    Label {
                        text: root.hasOperationError ? root.operationErrorTitle : "Operations"
                        font.bold: true
                        font.pixelSize: Theme.scaledSize(15)
                        color: root.hasOperationError ? Theme.danger : Theme.textPrimary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.scaledSize(6)
                        visible: root.busy

                        Rectangle {
                            radius: Theme.scaledSize(9)
                            implicitHeight: root.smallPillHeight
                            implicitWidth: itemsLabel.implicitWidth + Theme.scaledSize(14)
                            color: root.solidPanelSurface
                            border.color: root.quietBorder
                            border.width: 1

                            Label {
                                id: itemsLabel
                                anchors.centerIn: parent
                                text: root.queue.completedItems + "/" + root.queue.totalItems
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeMicro
                                font.bold: true
                            }
                        }

                        Rectangle {
                            visible: root.queue.speedText !== ""
                            radius: Theme.scaledSize(9)
                            implicitHeight: root.smallPillHeight
                            implicitWidth: speedLabel.implicitWidth + Theme.scaledSize(14)
                            color: root.solidPanelSurface
                            border.color: root.accentBorder
                            border.width: 1

                            Label {
                                id: speedLabel
                                anchors.centerIn: parent
                                text: root.queue.speedText
                                color: Theme.accent
                                font.pixelSize: Theme.fontSizeMicro
                                font.bold: true
                            }
                        }

                        Rectangle {
                            visible: root.queue.elapsedTimeText !== ""
                            radius: Theme.scaledSize(9)
                            implicitHeight: root.smallPillHeight
                            implicitWidth: elapsedLabel.implicitWidth + Theme.scaledSize(14)
                            color: root.solidPanelSurface
                            border.color: root.quietBorder
                            border.width: 1

                            Label {
                                id: elapsedLabel
                                anchors.centerIn: parent
                                text: root.queue.elapsedTimeText
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeMicro
                                font.bold: true
                            }
                        }
                    }
                }

                Button {
                    id: collapseBtn
                    visible: !root.hasOperationError
                    Layout.preferredWidth: Math.max(Theme.scaledSize(64), implicitWidth)
                    Layout.preferredHeight: Theme.scaledSize(30)
                    Layout.alignment: Qt.AlignTop | Qt.AlignRight
                    text: "Hide"

                    background: Rectangle {
                        radius: Theme.scaledSize(9)
                        color: collapseBtn.pressed
                               ? root.solidSurfaceActive
                               : (collapseBtn.hovered ? root.solidPanelSurfaceSoft : root.solidPanelSurface)
                        border.color: root.quietBorder
                        border.width: 1
                    }

                    contentItem: Label {
                        text: collapseBtn.text
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeCaption
                        font.bold: true
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        root.userPinnedExpanded = false
                        root.expanded = false
                        previewDelayTimer.stop()
                        collapseTimer.stop()
                    }
                }

            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.scaledSize(6)
                visible: root.busy

                ProgressBar {
                    id: pBar
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: root.queue.progress

                    background: Rectangle {
                        implicitHeight: Theme.scaledSize(10)
                        color: root.quietBorder
                        radius: Theme.scaledSize(5)
                    }

                    contentItem: Item {
                        Rectangle {
                            width: pBar.visualPosition * parent.width
                            height: parent.height
                            radius: Theme.scaledSize(5)
                            color: root.hasOperationError ? Theme.danger : Theme.accent

                        }
                    }

                    Behavior on value {
                        NumberAnimation { duration: 180 }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Rectangle {
                        radius: Theme.scaledSize(9)
                        implicitHeight: root.smallPillHeight
                        implicitWidth: pctLabel.implicitWidth + Theme.scaledSize(14)
                        color: root.solidPanelSurface
                        border.color: root.accentBorder
                        border.width: 1

                        Label {
                            id: pctLabel
                            anchors.centerIn: parent
                            text: Math.round(root.queue.progress * 100) + "%"
                            color: Theme.accent
                            font.pixelSize: Theme.fontSizeMicro
                            font.bold: true
                        }
                    }

                    Item { Layout.preferredWidth: Theme.scaledSize(8) }

                    Label {
                        text: root.queue.remainingTimeText
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeMicro
                        font.bold: true
                        visible: root.queue.remainingTimeText !== ""
                        elide: Text.ElideRight
                        Layout.maximumWidth: Math.round(root.width * 0.25)
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: root.queue.currentLabel || "Preparing..."
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeCaption
                        font.bold: true
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.ceil(msgLabel.font.pixelSize * 2.2) + Theme.scaledSize(24)
                color: root.solidPanelSurface
                radius: Theme.scaledSize(14)
                border.color: root.hasOperationError ? root.dangerBorder : root.quietBorder
                border.width: 1

                Label {
                    id: msgLabel
                    anchors.fill: parent
                    anchors.margins: Theme.scaledSize(10)
                    text: root.hasOperationError ? root.operationErrorMessage
                                                 : (root.queue.currentLabel || "Initializing...")
                    color: root.hasOperationError ? Theme.danger : Theme.textPrimary
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: "Segoe UI Semibold, Arial"
                    wrapMode: Text.Wrap
                    elide: Text.ElideMiddle
                    maximumLineCount: 2
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Rectangle {
                visible: root.hasOperationError && root.operationErrorPath.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.ceil(errorPathLabel.font.pixelSize * 2.2) + Theme.scaledSize(20)
                radius: Theme.scaledSize(12)
                color: root.solidPanelSurface
                border.color: root.quietBorder
                border.width: 1

                Label {
                    id: errorPathLabel
                    anchors.fill: parent
                    anchors.margins: Theme.scaledSize(10)
                    text: root.operationErrorDisplayPath
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeMicro
                    wrapMode: Text.WrapAnywhere
                    maximumLineCount: 2
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Rectangle {
                visible: root.hasOperationError && root.operationErrorItemSummary.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.ceil(failedItemsLabel.font.pixelSize * 2.2) + Theme.scaledSize(20)
                radius: Theme.scaledSize(12)
                color: root.solidPanelSurface
                border.color: root.warningBorder
                border.width: 1

                Label {
                    id: failedItemsLabel
                    anchors.fill: parent
                    anchors.margins: Theme.scaledSize(10)
                    text: root.operationErrorItemCount > 1
                          ? ("Failed items (" + root.operationErrorItemCount + "): " + root.operationErrorItemSummary)
                          : ("Failed item: " + root.operationErrorItemSummary)
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeMicro
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.scaledSize(8)

                Button {
                    id: retryBtn
                    visible: root.hasOperationError && root.canRetry
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.actionButtonHeight
                    text: "Retry"

                    background: Rectangle {
                        radius: Theme.scaledSize(9)
                        color: retryBtn.pressed
                               ? root.solidSurfaceActive
                               : (retryBtn.hovered ? root.solidPanelSurfaceSoft : root.solidPanelSurface)
                        border.color: root.accentBorder
                        border.width: 1
                    }

                    contentItem: Label {
                        text: retryBtn.text
                        color: Theme.accent
                        font.bold: true
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.queue.retryLastOperation()
                }

                Button {
                    id: refreshBtn
                    visible: root.hasOperationError && root.canRefresh
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.actionButtonHeight
                    text: "Refresh"

                    background: Rectangle {
                        radius: Theme.scaledSize(9)
                        color: refreshBtn.pressed
                               ? root.solidSurfaceActive
                               : (refreshBtn.hovered ? root.solidPanelSurfaceSoft : root.solidPanelSurface)
                        border.color: root.accentBorder
                        border.width: 1
                    }

                    contentItem: Label {
                        text: refreshBtn.text
                        color: Theme.accent
                        font.bold: true
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        const window = root.Window.window
                        root.queue.clearError()
                        if (window && window.refreshActivePanel) {
                            window.refreshActivePanel()
                        }
                    }
                }

                Button {
                    id: copyPathBtn
                    visible: root.hasOperationError && root.canCopyPath
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.actionButtonHeight
                    text: "Copy path"

                    background: Rectangle {
                        radius: Theme.scaledSize(9)
                        color: copyPathBtn.pressed
                               ? root.solidSurfaceActive
                               : (copyPathBtn.hovered ? root.solidPanelSurfaceSoft : root.solidPanelSurface)
                        border.color: root.accentBorder
                        border.width: 1
                    }

                    contentItem: Label {
                        text: copyPathBtn.text
                        color: Theme.accent
                        font.bold: true
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: workspaceController.copyTextToClipboard(root.operationErrorDisplayPath)
                }

                Button {
                    id: adminBtn
                    visible: root.hasOperationError && root.canRestartAsAdmin
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.actionButtonHeight
                    text: "Run as admin"

                    background: Rectangle {
                        radius: Theme.scaledSize(9)
                        color: adminBtn.pressed
                               ? root.solidSurfaceActive
                               : (adminBtn.hovered ? root.solidPanelSurfaceSoft : root.solidPanelSurface)
                        border.color: root.warningBorder
                        border.width: 1
                    }

                    contentItem: Label {
                        text: adminBtn.text
                        color: Theme.warning
                        font.bold: true
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        const window = root.Window.window
                        if (window && window.relaunchAsAdmin) {
                            window.relaunchAsAdmin()
                        }
                    }
                }

                Button {
                    id: cancelBtn
                    visible: !root.hasOperationError
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.actionButtonHeight
                    text: "Cancel operation"

                    background: Rectangle {
                        radius: Theme.scaledSize(9)
                        color: cancelBtn.pressed
                               ? root.solidSurfaceActive
                               : (cancelBtn.hovered ? root.solidPanelSurfaceSoft : root.solidPanelSurface)
                        border.color: root.hasOperationError
                                      ? root.quietBorder
                                      : root.dangerBorder
                        border.width: 1
                    }

                    contentItem: Label {
                        text: cancelBtn.text
                        color: Theme.danger
                        font.bold: true
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.queue.cancel()
                }
            }
        }
    }
}
