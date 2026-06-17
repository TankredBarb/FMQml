import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "common"
import "dialogs"
import "../style"

Dialog {
    id: root

    title: "Plugin Manager"
    modal: true
    focus: true
    parent: Overlay.overlay
    width: Math.min(parent ? parent.width - 48 : 760, 760)
    height: Math.min(parent ? parent.height - 48 : 560, 560)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    padding: 0

    property var plugins: []
    property var loadErrors: []
    property string statusText: ""
    readonly property int loadedPluginCount: root.countLoadedPlugins(root.plugins)
    readonly property color dialogAccent: Theme.accent
    readonly property color sectionFill: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.30 : 0.56)
    readonly property color sectionBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.24)
    readonly property color rowFill: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.30 : 0.52)
    readonly property color rowFillHover: Theme.withAlpha(Theme.surfaceHover, themeController.isDark ? 0.42 : 0.58)
    readonly property color rowBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.26 : 0.20)
    readonly property color detailText: Theme.readableOn(Theme.panelSurface, Theme.textSecondary)

    onOpened: {
        root.refresh()
        Qt.callLater(() => contentItem.forceActiveFocus())
    }

    function refresh() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            root.plugins = []
            root.loadErrors = []
            root.statusText = "Plugin controller is unavailable."
            return
        }
        root.plugins = pluginActionController.plugins()
        root.loadErrors = pluginActionController.loadErrors()
    }

    function applyResult(result) {
        root.statusText = String(result.message || "")
        root.refresh()
    }

    function rescanDefaults() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) return
        root.applyResult(pluginActionController.rescanDefaultPluginDirectories())
    }

    function unloadPlugin(pluginId) {
        if (typeof pluginActionController === "undefined" || !pluginActionController) return
        root.applyResult(pluginActionController.unloadPlugin(pluginId))
    }

    function loadPlugin(path) {
        if (typeof pluginActionController === "undefined" || !pluginActionController) return
        root.applyResult(pluginActionController.loadPluginFile(path))
    }

    function countLoadedPlugins(items) {
        let count = 0
        const list = items || []
        for (let i = 0; i < list.length; ++i) {
            if (list[i].loaded) {
                ++count
            }
        }
        return count
    }

    function displayPath(path) {
        if (!path || String(path).length === 0) {
            return ""
        }
        return Qt.platform.os === "windows" ? String(path).replace(/\//g, "\\") : String(path)
    }

    background: DialogShell {
        accentColor: root.dialogAccent
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
        shadowBlur: 16
        shadowVerticalOffset: 5
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/plugin.svg"
        iconTint: root.dialogAccent
        accentColor: root.dialogAccent
        title: root.title
        subtitle: root.loadedPluginCount + " loaded / " + root.plugins.length + " known"
        closeText: "x"
        onCloseRequested: root.accept()
    }

    footer: DialogFooter {
        Label {
            Layout.fillWidth: true
            text: root.statusText
            font.pixelSize: Theme.fontSizeCaption
            color: root.detailText
            elide: Text.ElideRight
        }

        DialogActionButton {
            text: "Close"
            highlighted: true
            primaryColor: root.dialogAccent
            onClicked: root.accept()
        }
    }

    contentItem: ColumnLayout {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        spacing: 0
        clip: true
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                root.accept()
                event.accepted = true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            spacing: 10

            DialogActionButton {
                text: "Rescan"
                highlighted: false
                secondaryTextColor: root.dialogAccent
                onClicked: root.rescanDefaults()
            }

            DialogActionButton {
                text: "Load file"
                highlighted: false
                secondaryTextColor: root.dialogAccent
                onClicked: pluginFileDialog.open()
            }

            DialogActionButton {
                text: "Load folder"
                highlighted: false
                secondaryTextColor: root.dialogAccent
                onClicked: pluginFolderDialog.open()
            }

            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.withAlpha(Theme.border, 0.45)
        }

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Pane {
                width: scrollView.availableWidth
                padding: 16
                background: null

                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    Repeater {
                        model: root.plugins

                        SurfaceCard {
                            Layout.fillWidth: true
                            implicitHeight: pluginRow.implicitHeight + 22
                            surfaceColor: pluginMouse.containsMouse ? root.rowFillHover : root.rowFill
                            strokeColor: pluginMouse.containsMouse
                                         ? Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.30 : 0.22)
                                         : root.rowBorder
                            cornerRadius: Theme.radiusMd

                            MouseArea {
                                id: pluginMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }

                            ColumnLayout {
                                id: pluginRow
                                anchors.fill: parent
                                anchors.margins: 11
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Rectangle {
                                        Layout.preferredWidth: 10
                                        Layout.preferredHeight: 10
                                        radius: 5
                                        color: modelData.loaded ? Theme.success : Theme.textSecondary
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Label {
                                            Layout.fillWidth: true
                                            text: modelData.displayName || modelData.pluginId
                                            font.pixelSize: Theme.fontSizeBody
                                            font.weight: Font.DemiBold
                                            color: Theme.textPrimary
                                            elide: Text.ElideRight
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: modelData.pluginId
                                            font.pixelSize: Theme.fontSizeMicro
                                            color: root.detailText
                                            elide: Text.ElideMiddle
                                        }
                                    }

                                    DialogActionButton {
                                        text: modelData.loaded ? "Unload" : "Load"
                                        highlighted: false
                                        secondaryTextColor: modelData.loaded ? Theme.warning : root.dialogAccent
                                        onClicked: modelData.loaded ? root.unloadPlugin(modelData.pluginId) : root.loadPlugin(modelData.filePath)
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    TagPill {
                                        text: modelData.loaded ? "Loaded" : "Unloaded"
                                        accentColor: modelData.loaded ? Theme.success : Theme.textSecondary
                                    }

                                    TagPill {
                                        visible: modelData.capabilitiesText && modelData.capabilitiesText.length > 0
                                        text: modelData.capabilitiesText || ""
                                        accentColor: Theme.categoryInfo
                                    }

                                    TagPill {
                                        visible: modelData.schemesText && modelData.schemesText.length > 0
                                        text: modelData.schemesText || ""
                                        accentColor: Theme.categoryAction
                                    }

                                    Item { Layout.fillWidth: true }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.displayPath(modelData.filePath)
                                    font.pixelSize: Theme.fontSizeMicro
                                    color: root.detailText
                                    elide: Text.ElideMiddle
                                }
                            }
                        }
                    }

                    SurfaceCard {
                        Layout.fillWidth: true
                        visible: root.plugins.length === 0
                        implicitHeight: emptyLayout.implicitHeight + 24
                        surfaceColor: root.sectionFill
                        strokeColor: root.sectionBorder
                        cornerRadius: Theme.radiusMd

                        ColumnLayout {
                            id: emptyLayout
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: "No plugins loaded"
                                font.pixelSize: Theme.fontSizeBody
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "Default plugin directories can be rescanned from this screen."
                                wrapMode: Text.WordWrap
                                font.pixelSize: Theme.fontSizeCaption
                                color: root.detailText
                            }
                        }
                    }

                    DialogSection {
                        visible: root.loadErrors.length > 0
                        title: "LOAD ERRORS"
                        accentColor: Theme.warning
                        fillColor: Theme.withAlpha(Theme.warning, themeController.isDark ? 0.08 : 0.05)
                        borderColor: Theme.withAlpha(Theme.warning, themeController.isDark ? 0.28 : 0.20)
                        radiusSize: Theme.radiusMd

                        Repeater {
                            model: root.loadErrors

                            Label {
                                Layout.fillWidth: true
                                text: modelData
                                wrapMode: Text.WrapAnywhere
                                font.pixelSize: Theme.fontSizeCaption
                                color: root.detailText
                            }
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: pluginFileDialog
        title: "Load Plugin"
        nameFilters: ["Plugin libraries (*.dll *.so *.dylib)", "All files (*)"]
        onAccepted: {
            if (typeof pluginActionController === "undefined" || !pluginActionController) return
            root.applyResult(pluginActionController.loadPluginFile(selectedFile.toString()))
        }
    }

    FolderDialog {
        id: pluginFolderDialog
        title: "Load Plugin Folder"
        onAccepted: {
            if (typeof pluginActionController === "undefined" || !pluginActionController) return
            root.applyResult(pluginActionController.loadPluginDirectory(selectedFolder.toString()))
        }
    }

    component TagPill: Rectangle {
        property string text: ""
        property color accentColor: Theme.accent

        Layout.preferredWidth: pillLabel.implicitWidth + 14
        Layout.preferredHeight: 22
        radius: Theme.radiusSm
        color: Theme.withAlpha(accentColor, themeController.isDark ? 0.15 : 0.10)
        border.color: Theme.withAlpha(accentColor, themeController.isDark ? 0.34 : 0.24)
        border.width: 1

        Label {
            id: pillLabel
            anchors.centerIn: parent
            text: parent.text
            font.pixelSize: Theme.fontSizeMicro
            font.weight: Font.DemiBold
            color: parent.accentColor
        }
    }
}
