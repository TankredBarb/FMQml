import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"
import "toolbar"

ToolBar {
    id: root
    
    property alias pathEditorField: toolbarPathEditor.pathEditorField
    property alias pathEditing: toolbarPathEditor.pathEditing
    property alias pathEditError: toolbarPathEditor.pathEditError
    property alias pathEditProgress: toolbarPathEditor.pathEditProgress
    property var appRoot
    property var workspaceController
    property var activePanelView
    property bool previewVisible: false
    property bool searchReturnVisible: false
    signal previewToggleRequested(bool visible)
    signal searchReturnRequested()
    readonly property bool textEditingActive: pathEditing || toolbarSearch.editorActiveFocus
    
    height: 64

    background: AmbientPanelBackground {
        baseColor: Theme.panelSurfaceStrong
        endColor: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.88 : 0.82)
        strength: 0.68
        cornerRadius: Theme.panelRadius
        topLeftCornerRadius: 0
        topRightCornerRadius: 0

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Theme.innerRadius(parent.cornerRadius, 1)
            topLeftRadius: 0
            topRightRadius: 0
            color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.08 : 0.14)
        }

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: themeController.isDark
                   ? Theme.withAlpha(Theme.panelStrokeStrong, 0.38)
                   : Theme.withAlpha(Theme.panelStrokeStrong, 0.46)
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.panelRadius
            anchors.rightMargin: Theme.panelRadius
            height: 1
            color: themeController.isDark
                ? Theme.withAlpha(Theme.panelStrokeStrong, 0.52)
                : Theme.withAlpha(Theme.panelStrokeStrong, 0.62)
        }
    }

    readonly property var activeController: root.workspaceController.activePanel === 0
                                            ? root.workspaceController.leftPanel
                                            : root.workspaceController.rightPanel
    readonly property string activePath: root.workspaceController.activePanel === 0
                                         ? root.workspaceController.leftPanel.currentPath
                                         : root.workspaceController.rightPanel.currentPath
    readonly property bool activeIsFavoritesRoot: root.activeController ? root.activeController.isFavoritesRoot : false

    function focusPath() {
        toolbarPathEditor.focusPath()
    }

    function acceptPathEdit() {
        toolbarPathEditor.acceptPathEdit()
    }

    function cancelPathEdit() {
        toolbarPathEditor.cancelPathEdit()
    }

    function focusSearch(initialText) {
        if (root.activeIsFavoritesRoot) {
            return false
        }
        return toolbarSearch.focusSearch(initialText)
    }

    function openThemeSelector() {
        toolbarActions.openThemeSelector()
    }

    function openAppMenu() {
        appMenu.popup(appMenuButton, 0, appMenuButton.height + 4)
    }

    function openActivePath(path) {
        if (root.activePanelView && root.activePanelView.openPath) {
            return root.activePanelView.openPath(path)
        }
        return root.activeController ? root.activeController.openPath(path) : false
    }

    function prepareActiveNavigation(reason) {
        if (root.activePanelView && root.activePanelView.cancelInlineRenameForNavigation) {
            root.activePanelView.cancelInlineRenameForNavigation(reason)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 6

        Button {
            id: appMenuButton

            Layout.preferredWidth: 40
            Layout.preferredHeight: 36
            padding: 0
            hoverEnabled: true
            focusPolicy: Qt.NoFocus
            onClicked: root.openAppMenu()
            ToolTip.visible: hovered
            ToolTip.text: "Menu"

            contentItem: Item {
                implicitWidth: 22
                implicitHeight: 18

                Column {
                    anchors.centerIn: parent
                    spacing: 4

                    Repeater {
                        model: 3

                        Rectangle {
                            width: 22
                            height: 2
                            radius: 1
                            color: appMenuButton.enabled
                                   ? Theme.actionIconColor("default")
                                   : Theme.textSecondary
                            opacity: appMenuButton.enabled ? 0.95 : 0.45
                        }
                    }
                }
            }

            background: Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: Theme.radiusMd
                color: appMenuButton.pressed || appMenu.opened
                       ? Theme.surfaceActive
                       : (appMenuButton.hovered
                          ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.18 : 0.12)
                          : Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.82 : 0.96))
                border.color: Theme.withAlpha(Theme.accent,
                                              appMenuButton.hovered || appMenu.opened ? 0.68 : 0.38)
                border.width: 1
            }

            ThemedContextMenu {
                id: appMenu
                implicitWidth: 220

                ThemedMenuItem {
                    text: "Settings"
                    shortcut: "Ctrl+,"
                    icon.source: "qrc:/qt/qml/FM/qml/assets/icons/settings.svg"
                    iconColor: Theme.accent
                    onTriggered: {
                        if (root.appRoot && root.appRoot.openSettingsDialog) {
                            root.appRoot.openSettingsDialog()
                        }
                    }
                }

                ThemedMenuItem {
                    text: "Help"
                    shortcut: "F1"
                    icon.source: "qrc:/qt/qml/FM/qml/assets/toolbar-next/info.svg"
                    iconColor: Theme.categoryInfo
                    onTriggered: {
                        if (root.appRoot && root.appRoot.openHelpDialog) {
                            root.appRoot.openHelpDialog()
                        }
                    }
                }

                ThemedMenuSeparator {}

                ThemedMenuItem {
                    text: "Quit"
                    shortcut: "Ctrl+Q"
                    destructive: true
                    icon.source: "qrc:/qt/qml/FM/qml/assets/icons/exit.svg"
                    onTriggered: {
                        if (root.appRoot && root.appRoot.quitApplication) {
                            root.appRoot.quitApplication()
                        }
                    }
                }
            }
        }

        // --- LEFT: Navigation & Core ---
        RowLayout {
            spacing: 6

            NavigationControls {
                controller: root.activeController
                panelView: root.activePanelView
                searchReturnVisible: root.searchReturnVisible
                onSearchReturnRequested: root.searchReturnRequested()
            }

            ViewControls {
                controller: root.activeController
                workspaceController: root.workspaceController
                visible: !root.activeIsFavoritesRoot
            }
        }

        // --- CENTER: Path Bar Island (Expanded) ---
        ToolbarPathEditor {
            id: toolbarPathEditor
            Layout.fillWidth: true
            controller: root.activeController
            workspaceController: root.workspaceController
            activePath: root.activePath
            openPathHandler: function(path) { return root.openActivePath(path) }
            prepareNavigationHandler: function(reason) { root.prepareActiveNavigation(reason) }
        }

        ToolbarActions {
            id: toolbarActions
            controller: root.activeController
            workspaceController: root.workspaceController
            appRoot: root.appRoot
            previewVisible: root.previewVisible
            onPreviewToggleRequested: (visible) => root.previewToggleRequested(visible)
            onHelpRequested: root.appRoot ? root.appRoot.openHelpDialog() : undefined
        }

        ToolbarSearch {
            id: toolbarSearch
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight
            visible: !root.activeIsFavoritesRoot
            enabled: visible
            controller: root.activeController
            workspaceController: root.workspaceController
        }
    }

    // --- Active panel edge indicator (split mode only) ---
    readonly property bool splitActive: root.workspaceController && root.workspaceController.splitEnabled
    readonly property bool activePanelRight: root.workspaceController && root.workspaceController.activePanel === 1

    Rectangle {
        id: activePanelStrip
        visible: root.splitActive && Theme.useGradientColors
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        x: root.activePanelRight ? parent.width - width - 4 : 4
        width: 4
        radius: 2
        color: Theme.activeAccent
        z: 10

        // Fade out → snap position → fade in; no horizontal travel across toolbar
        opacity: root.splitActive ? 0.90 : 0.0
        Behavior on opacity {
            NumberAnimation { duration: Theme.motionFast; easing.type: Easing.InOutQuad }
        }

        onXChanged: {
            if (root.splitActive) stripFadeAnim.restart()
        }

        SequentialAnimation {
            id: stripFadeAnim
            NumberAnimation { target: activePanelStrip; property: "opacity"; to: 0.0; duration: Theme.motionFast; easing.type: Easing.OutQuad }
            NumberAnimation { target: activePanelStrip; property: "opacity"; to: 0.90; duration: Theme.motionFast; easing.type: Easing.InQuad }
        }
    }

    Rectangle {
        id: activePanelGlow
        visible: root.splitActive
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 80
        x: root.activePanelRight ? parent.width - width : 0
        z: 9

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: root.activePanelRight ? 0.0 : 1.0
                color: "transparent"
            }
            GradientStop {
                position: root.activePanelRight ? 1.0 : 0.0
                color: Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.12 : 0.08)
            }
        }

        // Fade out → snap position + flip gradient → fade in
        opacity: root.splitActive ? 1.0 : 0.0
        Behavior on opacity {
            NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.InOutQuad }
        }

        onXChanged: {
            if (root.splitActive) glowFadeAnim.restart()
        }

        SequentialAnimation {
            id: glowFadeAnim
            NumberAnimation { target: activePanelGlow; property: "opacity"; to: 0.0; duration: Theme.motionFast; easing.type: Easing.OutQuad }
            NumberAnimation { target: activePanelGlow; property: "opacity"; to: 1.0; duration: Theme.motionFast; easing.type: Easing.InQuad }
        }
    }
}
