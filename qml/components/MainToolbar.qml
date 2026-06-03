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
    property bool previewVisible: false
    property bool searchReturnVisible: false
    signal previewToggleRequested(bool visible)
    signal searchReturnRequested()
    readonly property bool textEditingActive: pathEditing || toolbarSearch.editorActiveFocus
    
    height: 64
    
    background: Rectangle {
        color: Theme.panelSurface

        Rectangle {
            anchors.fill: parent
            radius: 0
            color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.08 : 0.04)
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: themeController.isDark
                ? Theme.withAlpha(Theme.accentText, 0.09)
                : Theme.withAlpha(Theme.border, 0.5)
        }
        
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.accentText, themeController.isDark ? 0.08 : 0.06) }
            GradientStop { position: 0.52; color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.04 : 0.02) }
            GradientStop { position: 1.0; color: "transparent" }
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

    function focusSearch() {
        if (root.activeIsFavoritesRoot) {
            return
        }
        toolbarSearch.focusSearch()
    }

    function openThemeSelector() {
        toolbarActions.openThemeSelector()
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
