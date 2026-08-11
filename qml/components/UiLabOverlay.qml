import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "framework"
import "ui_lab"

Popup {
    id: root

    x: 8
    y: 8
    width: parent ? Math.max(0, parent.width - 16) : 1200
    height: parent ? Math.max(0, parent.height - 16) : 760
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose

    property string currentPageId: "overview"
    property string currentScenarioId: "matrix"
    property int viewportPreset: typeof appSettings !== "undefined" && appSettings ? appSettings.uiLabViewportPreset : 1
    property int backgroundPreset: typeof appSettings !== "undefined" && appSettings ? appSettings.uiLabBackgroundPreset : 0
    property bool stateMatrix: typeof appSettings !== "undefined" && appSettings ? appSettings.uiLabStateMatrix : true
    readonly property var pages: [
        { id: "overview", title: "Overview", component: overviewPage },
        { id: "buttons", title: "Buttons and Actions", component: buttonsPage },
        { id: "inputs", title: "Text Inputs", component: inputsPage },
        { id: "selection", title: "Selection Controls", component: selectionPage },
        { id: "menus", title: "Combo, Menu, and Popup", component: menusPage },
        { id: "progress", title: "Progress and Activity", component: progressPage },
        { id: "scrollbars", title: "Scrollbars and Scrolling", component: scrollbarsPage },
        { id: "tabs", title: "Tabs and Navigation", component: tabsPage },
        { id: "typography", title: "Typography", component: typographyPage },
        { id: "colors", title: "Colors and Surfaces", component: colorsPage },
        { id: "icons", title: "Icons", component: iconsPage },
        { id: "lists", title: "Lists, Rows, and Delegates", component: listsPage },
        { id: "dialogs", title: "Dialog Surfaces", component: dialogsPage },
        { id: "folder-preview", title: "Hover Previews and Peek", component: folderPreviewPage },
        { id: "composite", title: "Composite Patterns", component: compositePage }
    ]
    readonly property int sceneWidth: viewportPreset === 0 ? 520 : (viewportPreset === 1 ? 760 : 1040)
    readonly property string sceneWidthLabel: viewportPreset === 3 ? "Full desktop" : sceneWidth + " px"
    readonly property string activeThemeName: themeController.schemeName && themeController.schemeName.length > 0
                                               ? themeController.schemeName : "Unnamed theme"
    readonly property color sceneColor: backgroundPreset === 0 ? Theme.panelSurface
                                                               : (backgroundPreset === 1 ? Theme.surface
                                                                                         : Theme.panelSurfaceStrong)

    function pageIndex(pageId) {
        for (let i = 0; i < pages.length; ++i) {
            if (pages[i].id === pageId) return i
        }
        return 0
    }

    function openPage(pageId, scenarioId) {
        const index = pageIndex(pageId)
        currentPageId = pages[index].id
        currentScenarioId = scenarioId || "matrix"
        pageList.currentIndex = index
        pageLoader.sourceComponent = pages[index].component
        if (!opened) open()
        else forceActiveFocus()
    }

    function resetScene() {
        viewportPreset = 1
        backgroundPreset = 0
        stateMatrix = true
        currentScenarioId = "matrix"
        pageLoader.active = false
        pageLoader.active = true
    }

    onViewportPresetChanged: {
        if (typeof appSettings !== "undefined" && appSettings) appSettings.uiLabViewportPreset = viewportPreset
    }
    onBackgroundPresetChanged: {
        if (typeof appSettings !== "undefined" && appSettings) appSettings.uiLabBackgroundPreset = backgroundPreset
    }
    onStateMatrixChanged: {
        if (typeof appSettings !== "undefined" && appSettings) appSettings.uiLabStateMatrix = stateMatrix
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.bg
        border.color: Theme.panelBorder
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            color: Theme.panelSurfaceStrong
            radius: Theme.radiusLg

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Theme.radiusLg
                color: parent.color
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 14
                spacing: 12

                ColumnLayout {
                    spacing: 2
                    Label { text: "UI Lab"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
                    Label { text: "Production controls · deterministic scenes · " + root.currentPageId + "/" + root.currentScenarioId; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                }
                Item { Layout.fillWidth: true }
                Label { text: "Theme: " + root.activeThemeName + " (" + (themeController.isDark ? "Dark" : "Light") + ")"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                FmIconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/close.svg"
                    showIdleSurface: true
                    Accessible.name: "Close UI Lab"
                    onClicked: root.close()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 224
                Layout.fillHeight: true
                radius: Theme.radiusMd
                color: Theme.panelSurface
                border.color: Theme.panelBorder

                ListView {
                    id: pageList
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4
                    clip: true
                    model: root.pages
                    currentIndex: 0

                    delegate: FmButton {
                        required property int index
                        required property var modelData
                        width: ListView.view.width
                        text: modelData.title
                        highlighted: ListView.isCurrentItem
                        flat: !highlighted
                        onClicked: root.openPage(modelData.id, "matrix")
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    radius: Theme.radiusMd
                    color: Theme.panelSurface
                    border.color: Theme.panelBorder

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8
                        Label { text: "Viewport"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                        FmComboBox { model: ["Narrow", "Medium", "Wide", "Full (Desktop)"]; currentIndex: root.viewportPreset; onActivated: index => root.viewportPreset = index }
                        Label { text: "Background"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                        FmComboBox { model: ["Panel", "Surface", "Strong Surface"]; currentIndex: root.backgroundPreset; onActivated: index => root.backgroundPreset = index }
                        FmSwitch {
                            text: "State Matrix"
                            checked: root.stateMatrix
                            ToolTip.text: "Show additional disabled, error, long-text, and geometry states"
                            ToolTip.visible: hovered
                            onToggled: root.stateMatrix = checked
                        }
                        Item { Layout.fillWidth: true }
                        Label { text: root.sceneWidthLabel; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                        FmButton { text: "Reset Scene"; onClicked: root.resetScene() }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    Rectangle {
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: root.viewportPreset === 3 ? parent.width : Math.min(parent.width, root.sceneWidth)
                        radius: Theme.radiusMd
                        color: root.sceneColor
                        border.color: Theme.panelBorder
                        clip: true

                        ScrollView {
                            id: sceneScrollView
                            anchors.fill: parent
                            anchors.margins: 12
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical: FmScrollBar {
                                parent: sceneScrollView
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                policy: ScrollBar.AsNeeded
                                wheelTarget: sceneScrollView.contentItem
                            }

                            Loader {
                                id: pageLoader
                                width: parent.width
                                sourceComponent: overviewPage
                                onLoaded: {
                                    item.stateMatrix = Qt.binding(() => root.stateMatrix)
                                    item.labVisible = Qt.binding(() => root.opened)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; enabled: root.opened; onActivated: root.close() }

    Component {
        id: overviewPage
        UiLabOverviewPage {
            pageRegistry: root.pages
            onPageRequested: pageId => root.openPage(pageId, "matrix")
        }
    }
    Component { id: buttonsPage; UiLabButtonsPage {} }
    Component { id: inputsPage; UiLabInputsPage {} }
    Component { id: selectionPage; UiLabSelectionPage {} }
    Component { id: menusPage; UiLabMenusPage {} }
    Component { id: progressPage; UiLabProgressPage {} }
    Component { id: scrollbarsPage; UiLabScrollbarsPage {} }
    Component { id: tabsPage; UiLabTabsPage {} }
    Component { id: typographyPage; UiLabTypographyPage {} }
    Component { id: colorsPage; UiLabColorsPage {} }
    Component { id: iconsPage; UiLabIconsPage {} }
    Component { id: listsPage; UiLabListsPage {} }
    Component { id: dialogsPage; UiLabDialogsPage {} }
    Component { id: folderPreviewPage; UiLabFolderPreviewPage {} }
    Component { id: compositePage; UiLabCompositePage {} }
}
