import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
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

    background: Rectangle {
        radius: Theme.panelRadius
        topLeftRadius: 0
        topRightRadius: 0
        color: Theme.panelSurfaceStrong
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.00; color: Theme.chromeGradientStart }
            GradientStop { position: 0.42; color: Theme.chromeGradientMid }
            GradientStop { position: 0.78; color: Theme.chromeGradientEnd }
            GradientStop { position: 1.00; color: Theme.withAlpha(Theme.panelSurfaceStrong, 0.00) }
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.innerRadius(parent.radius, 1)
            topLeftRadius: 0
            topRightRadius: 0
            color: Theme.withAlpha(Theme.accentText, themeController.isDark ? 0.026 : 0.040)
        }

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: Theme.withAlpha(Theme.accentText, themeController.isDark ? 0.04 : 0.22)
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.panelRadius
            anchors.rightMargin: Theme.panelRadius
            height: 1
            color: themeController.isDark
                ? Theme.withAlpha(Theme.accentText, 0.045)
                : Theme.withAlpha(Theme.border, 0.34)
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
}
