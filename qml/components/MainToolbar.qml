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
    property bool previewVisible: false
    signal previewToggleRequested(bool visible)
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

    readonly property var activeController: workspaceController.activePanel === 0
                                            ? workspaceController.leftPanel
                                            : workspaceController.rightPanel
    readonly property string activePath: workspaceController.activePanel === 0
                                         ? workspaceController.leftPanel.currentPath
                                         : workspaceController.rightPanel.currentPath

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
        toolbarSearch.focusSearch()
    }

    function openThemeSelector() {
        toolbarActions.openThemeSelector()
    }

    function openThemeImportDialog() {
        toolbarActions.openThemeImportDialog()
    }

    function openThemeExportDialog() {
        toolbarActions.openThemeExportDialog()
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
            }

            ViewControls {
                controller: root.activeController
                workspaceController: workspaceController
            }
        }

        // --- CENTER: Path Bar Island (Expanded) ---
        ToolbarPathEditor {
            id: toolbarPathEditor
            Layout.fillWidth: true
            controller: root.activeController
            workspaceController: workspaceController
            activePath: root.activePath
        }

        ToolbarActions {
            id: toolbarActions
            controller: root.activeController
            workspaceController: workspaceController
            previewVisible: root.previewVisible
            onPreviewToggleRequested: (visible) => root.previewToggleRequested(visible)
            onHelpRequested: helpDialog.open()
        }

        ToolbarSearch {
            id: toolbarSearch
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight
            controller: root.activeController
            workspaceController: workspaceController
        }
    }
}
