import QtQuick
import QtQuick.Controls
import "../style"

Item {
    id: root

    required property var workspaceController
    property var propertiesController
    property var quickLookPopup

    property alias leftPanelView: leftPanel
    property alias rightPanelView: rightPanel
    property bool liveResizeActive: false
    property bool externalScrollActive: false
    property int externalScrollSuppressFileCountThreshold: 25
    property bool externalScrollOptimizationEnabled: false
    property int externalScrollFileCountThreshold: 96
    property bool splitResizing: false
    readonly property bool externalPreviewScrollActive: leftPanel.externalScrollAnySuppressionActive
                                                        || rightPanel.externalScrollAnySuppressionActive
    readonly property bool previewScrollActive: root.externalPreviewScrollActive
                                                || leftPanel.previewScrollActive
                                                || rightPanel.previewScrollActive
    readonly property bool isRenaming: leftPanel.isRenaming || rightPanel.isRenaming
    readonly property int activePanelBottomChromeHeight: root.workspaceController.activePanel === 0
                                                       ? leftPanel.bottomChromeHeight
                                                       : rightPanel.bottomChromeHeight
    property var pendingSplitState: null

    signal panelVisualStateChanged()

    Rectangle {
        anchors.fill: parent
        color: Theme.panelSurface
    }

    function traceRenameFocus(stage, detail) {
    }

    function activePanelView() {
        return root.workspaceController.activePanel === 0 ? leftPanel : rightPanel
    }

    function saveSplitState() {
        return splitView.saveState()
    }

    function restoreSplitState(state) {
        pendingSplitState = state
        if (!state || state === null || state === undefined) {
            return
        }
        restoreSplitStateLater.restart()
    }

    function splitEvenly() {
        const half = Math.max(280, Math.floor(splitView.width / 2))
        leftPanel.SplitView.preferredWidth = half
        rightPanel.SplitView.preferredWidth = half
    }

    function collapseToActivePanel() {
        expandSinglePanel()
    }

    function expandSinglePanel() {
        leftPanel.SplitView.preferredWidth = Math.max(280, splitView.width)
        rightPanel.SplitView.preferredWidth = 0
    }

    Timer {
        id: restoreSplitStateLater
        interval: 0
        repeat: false
        onTriggered: {
            if (!root.pendingSplitState || root.pendingSplitState === null || root.pendingSplitState === undefined) {
                return
            }
            splitView.restoreState(root.pendingSplitState)
        }
    }

    SplitView {
        id: splitView
        anchors.fill: parent
        anchors.topMargin: 0
        anchors.bottomMargin: 4
        orientation: Qt.Horizontal

        FilePanel {
            id: leftPanel
            SplitView.fillWidth: true
            SplitView.minimumWidth: 280
            SplitView.preferredWidth: 0
            controller: root.workspaceController.leftPanel
            workspaceController: root.workspaceController
            propertiesController: root.propertiesController
            quickLookPopup: root.quickLookPopup
            liveResizeActive: root.liveResizeActive
            externalScrollActive: root.externalScrollActive
            externalScrollSuppressFileCountThreshold: root.externalScrollSuppressFileCountThreshold
            externalScrollOptimizationEnabled: root.externalScrollOptimizationEnabled
            externalScrollFileCountThreshold: root.externalScrollFileCountThreshold
            active: root.workspaceController.activePanel === 0
            onGridIconSizeChanged: root.panelVisualStateChanged()
            onBriefRowHeightChanged: root.panelVisualStateChanged()
            onDetailsVisualStateChanged: root.panelVisualStateChanged()
            onShowActionBarChanged: root.panelVisualStateChanged()
            onActivated: {
                root.traceRenameFocus("left-panel-activated")
                root.workspaceController.activateLeft()
                focusContent()
            }
        }

        FilePanel {
            id: rightPanel
            SplitView.fillWidth: root.workspaceController.splitEnabled
            SplitView.minimumWidth: root.workspaceController.splitEnabled ? 280 : 0
            SplitView.maximumWidth: root.workspaceController.splitEnabled ? 16777215 : 0
            SplitView.preferredWidth: 0
            visible: root.workspaceController.splitEnabled || width > 0
            opacity: root.workspaceController.splitEnabled ? 1 : 0
            
            Behavior on opacity { OpacityAnimator { duration: Theme.motionNormal } }

            controller: root.workspaceController.rightPanel
            workspaceController: root.workspaceController
            propertiesController: root.propertiesController
            quickLookPopup: root.quickLookPopup
            liveResizeActive: root.liveResizeActive
            externalScrollActive: root.externalScrollActive
            externalScrollSuppressFileCountThreshold: root.externalScrollSuppressFileCountThreshold
            externalScrollOptimizationEnabled: root.externalScrollOptimizationEnabled
            externalScrollFileCountThreshold: root.externalScrollFileCountThreshold
            active: root.workspaceController.activePanel === 1
            onGridIconSizeChanged: root.panelVisualStateChanged()
            onBriefRowHeightChanged: root.panelVisualStateChanged()
            onDetailsVisualStateChanged: root.panelVisualStateChanged()
            onShowActionBarChanged: root.panelVisualStateChanged()
            onActivated: {
                root.traceRenameFocus("right-panel-activated")
                root.workspaceController.activateRight()
                focusContent()
            }
        }

        handle: Rectangle {
            implicitWidth: 4
            implicitHeight: 12
            color: "transparent"
            readonly property bool handleActive: SplitHandle.hovered || SplitHandle.pressed

            SplitHandle.onPressedChanged: {
                root.splitResizing = SplitHandle.pressed
            }

            Rectangle {
                anchors.fill: parent
                color: Theme.accent
                opacity: SplitHandle.pressed ? 0.10 : (SplitHandle.hovered ? 0.05 : 0.0)
                radius: Theme.radiusSm
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.handleActive ? 2 : 1
                height: Math.max(0, parent.height - Theme.panelRadius * 2)
                radius: width / 2
                color: parent.handleActive
                       ? Theme.accent
                       : Theme.panelStrokeSubtle
                opacity: SplitHandle.pressed ? 0.78 : (SplitHandle.hovered ? 0.44 : (themeController.isDark ? 0.16 : 0.32))
                
                Behavior on width { NumberAnimation { duration: 100 } }
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }
        }
    }

    OperationsDrawer {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 20
        anchors.bottomMargin: 20 + root.activePanelBottomChromeHeight + 10
        width: 320
        z: 20
    }

    Connections {
        target: root.workspaceController
        function onFocusActivePanelRequested() {
            root.traceRenameFocus("focusActivePanelRequested")
            if (root.workspaceController.activePanel === 0) {
                leftPanel.focusContent()
            } else {
                rightPanel.focusContent()
            }
        }
        function onActivePanelChanged() {
            root.traceRenameFocus("activePanelChanged-schedule")
            Qt.callLater(() => {
                root.traceRenameFocus("activePanelChanged-fire")
                if (root.workspaceController.activePanel === 0) {
                    leftPanel.focusContent()
                } else {
                    rightPanel.focusContent()
                }
            })
        }
        function onSplitEnabledChanged() {
            root.traceRenameFocus("splitEnabledChanged-schedule")
            if (root.pendingSplitState !== null && root.pendingSplitState !== undefined) {
                restoreSplitStateLater.restart()
            }
            Qt.callLater(() => {
                root.traceRenameFocus("splitEnabledChanged-fire")
                if (root.workspaceController.activePanel === 0) {
                    leftPanel.focusContent()
                } else {
                    rightPanel.focusContent()
                }
            })
        }
    }
}
