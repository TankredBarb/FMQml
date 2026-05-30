import QtQuick
import QtQuick.Controls
import "../style"

Item {
    id: root

    required property var workspaceController
    property var propertiesController

    property alias leftPanelView: leftPanel
    property alias rightPanelView: rightPanel
    property bool liveResizeActive: false
    property bool splitResizing: false
    readonly property bool previewScrollActive: leftPanel.previewScrollActive || rightPanel.previewScrollActive
    readonly property bool isRenaming: leftPanel.isRenaming || rightPanel.isRenaming
    property var pendingSplitState: null

    signal panelVisualStateChanged()

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
        anchors.margins: 10
        orientation: Qt.Horizontal

        FilePanel {
            id: leftPanel
            SplitView.fillWidth: true
            SplitView.minimumWidth: 280
            SplitView.preferredWidth: 0
            controller: root.workspaceController.leftPanel
            workspaceController: root.workspaceController
            propertiesController: root.propertiesController
            liveResizeActive: root.liveResizeActive
            active: root.workspaceController.activePanel === 0
            onGridIconSizeChanged: root.panelVisualStateChanged()
            onBriefRowHeightChanged: root.panelVisualStateChanged()
            onDetailsVisualStateChanged: root.panelVisualStateChanged()
            onActivated: {
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
            liveResizeActive: root.liveResizeActive
            active: root.workspaceController.activePanel === 1
            onGridIconSizeChanged: root.panelVisualStateChanged()
            onBriefRowHeightChanged: root.panelVisualStateChanged()
            onDetailsVisualStateChanged: root.panelVisualStateChanged()
            onActivated: {
                root.workspaceController.activateRight()
                focusContent()
            }
        }

        handle: Rectangle {
            implicitWidth: 12
            implicitHeight: 12
            color: "transparent"

            SplitHandle.onPressedChanged: {
                root.splitResizing = SplitHandle.pressed
            }
            
            // Interaction overlay
            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                color: Theme.accent
                opacity: SplitHandle.pressed ? 0.16 : (SplitHandle.hovered ? 0.08 : 0)
                radius: 5
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }

            // The actual divider line
            Rectangle {
                anchors.centerIn: parent
                width: (SplitHandle.hovered || SplitHandle.pressed) ? 3 : 2
                height: parent.height - 18
                radius: width / 2
                color: (SplitHandle.hovered || SplitHandle.pressed) ? Theme.accent : Theme.border
                opacity: SplitHandle.pressed ? 1.0 : (SplitHandle.hovered ? 0.9 : 0.58)
                
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
        anchors.bottomMargin: 20 + Math.max(leftPanel.footerHeight, rightPanel.footerHeight) + 10
        width: 320
        z: 20
    }

    Connections {
        target: root.workspaceController
        function onFocusActivePanelRequested() {
            if (root.workspaceController.activePanel === 0) {
                leftPanel.focusContent()
            } else {
                rightPanel.focusContent()
            }
        }
        function onActivePanelChanged() {
            Qt.callLater(() => {
                if (root.workspaceController.activePanel === 0) {
                    leftPanel.focusContent()
                } else {
                    rightPanel.focusContent()
                }
            })
        }
        function onSplitEnabledChanged() {
            if (root.pendingSplitState !== null && root.pendingSplitState !== undefined) {
                restoreSplitStateLater.restart()
            }
            Qt.callLater(() => {
                if (root.workspaceController.activePanel === 0) {
                    leftPanel.focusContent()
                } else {
                    rightPanel.focusContent()
                }
            })
        }
    }
}
