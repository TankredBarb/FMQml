import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"
import "../framework"

DialogSection {
    id: section

    required property bool splitViewEnabled
    required property bool previewPaneEnabled
    required property var setSplitViewEnabled
    required property var setPreviewPaneEnabled
    required property var sidebarPanelOrder
    required property bool sidebarPlacesEnabled
    required property bool sidebarRecentEnabled
    required property bool sidebarFoldersEnabled
    required property var setSidebarPanelEnabled
    required property var moveSidebarPanel
    required property var resetSidebarPanels

    function panelTitle(panelId) {
        return panelId === "places" ? "Places"
             : panelId === "recent" ? "Recent folders" : "Folder tree"
    }

    function panelSubtitle(panelId) {
        return panelId === "places" ? "Locations, drives, devices, and providers"
             : panelId === "recent" ? "Frequently visited local folders"
             : "Browse the local folder hierarchy"
    }

    function panelEnabled(panelId) {
        return panelId === "places" ? section.sidebarPlacesEnabled
             : panelId === "recent" ? section.sidebarRecentEnabled
             : section.sidebarFoldersEnabled
    }

    title: "WORKSPACE"
    accentColor: Theme.accent
    fillColor: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.30 : 0.56)
    borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.24)
    radiusSize: Theme.radiusMd

    FmToggleRow {
        title: "Split view"
        subtitle: "Show the second file panel"
        checked: section.splitViewEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setSplitViewEnabled(checked)
    }

    FmToggleRow {
        title: "Preview pane"
        subtitle: "Keep the file preview pane visible"
        checked: section.previewPaneEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setPreviewPaneEnabled(checked)
    }

    Label {
        text: "SIDEBAR PANELS"
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeMicro
        font.weight: Font.DemiBold
        color: Theme.textSecondary
        topPadding: 6
        bottomPadding: 2
    }

    Repeater {
        model: section.sidebarPanelOrder

        RowLayout {
            required property string modelData
            required property int index

            Layout.fillWidth: true
            spacing: 6

            FmToggleRow {
                Layout.fillWidth: true
                title: section.panelTitle(modelData)
                subtitle: section.panelSubtitle(modelData)
                checked: section.panelEnabled(modelData)
                accentColor: Theme.accent
                onToggled: checked => section.setSidebarPanelEnabled(modelData, checked)
            }

            ColumnLayout {
                spacing: 4

                FmIconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-up.svg"
                    showIdleSurface: true
                    enabled: index > 0
                    Accessible.name: "Move " + section.panelTitle(modelData) + " up"
                    ToolTip.text: Accessible.name
                    ToolTip.visible: hovered
                    onClicked: section.moveSidebarPanel(modelData, -1)
                }

                FmIconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-down.svg"
                    showIdleSurface: true
                    enabled: index < section.sidebarPanelOrder.length - 1
                    Accessible.name: "Move " + section.panelTitle(modelData) + " down"
                    ToolTip.text: Accessible.name
                    ToolTip.visible: hovered
                    onClicked: section.moveSidebarPanel(modelData, 1)
                }
            }
        }
    }

    FmButton {
        text: "Restore sidebar defaults"
        flat: true
        Layout.alignment: Qt.AlignLeft
        onClicked: section.resetSidebarPanels()
    }
}
