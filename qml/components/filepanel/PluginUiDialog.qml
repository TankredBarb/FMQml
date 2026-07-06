import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../dialogs"
import "../../style"

Dialog {
    id: root

    property string componentUrl: ""
    property string subtitleText: "Plugin UI"
    property string statusText: ""
    property var pluginContext: ({})
    property var appRoot: null
    readonly property var pluginItem: pluginLoader.item
    readonly property bool pluginBusy: root.pluginItem && root.pluginItem.busy === true
    readonly property bool pluginCanApply: root.pluginItem
                                           && root.pluginItem.canApply !== false
                                           && typeof root.pluginItem.apply === "function"
                                           && !root.pluginBusy
    readonly property bool pluginCanApplyAll: root.pluginItem
                                              && root.pluginItem.canApplyAll !== false
                                              && typeof root.pluginItem.applyAll === "function"
                                              && !root.pluginBusy
    readonly property bool pluginApplyDoesNotClose: root.pluginItem
                                                    && root.pluginItem.applyDoesNotClose === true

    function showPluginUi(result) {
        root.title = String(result.title || "Plugin")
        root.subtitleText = String(result.subtitle || "Plugin UI")
        root.componentUrl = String(result.componentUrl || "")
        root.pluginContext = result.context || ({})
        root.statusText = ""

        if (!root.componentUrl.startsWith("qrc:/")) {
            root.statusText = "Plugin UI component is not available."
            pluginLoader.source = ""
            root.open()
            return
        }

        pluginLoader.setSource(root.componentUrl, {
            "pluginContext": root.pluginContext
        })
        root.open()
    }

    function applyPluginUi() {
        if (!root.pluginCanApply) {
            return
        }

        const result = root.pluginItem.apply()
        if (!result) {
            return
        }

        root.statusText = String(result.message || "")
        if (result.refreshCurrentPath === true
                && root.appRoot
                && root.appRoot.activePanelController) {
            const controller = root.appRoot.activePanelController()
            if (controller && controller.refresh) {
                controller.refresh()
            }
        }
        if (result.ok === true && !root.pluginApplyDoesNotClose) {
            root.close()
        }
    }

    function applyAllPluginUi() {
        if (!root.pluginCanApplyAll) {
            return
        }

        const result = root.pluginItem.applyAll()
        if (!result) {
            return
        }

        root.statusText = String(result.message || "")
        if (result.refreshCurrentPath === true
                && root.appRoot
                && root.appRoot.activePanelController) {
            const controller = root.appRoot.activePanelController()
            if (controller && controller.refresh) {
                controller.refresh()
            }
        }
        if (result.ok === true && !root.pluginApplyDoesNotClose) {
            root.close()
        }
    }

    modal: true
    focus: true
    parent: Overlay.overlay
    padding: 0
    width: Math.min(920, Math.max(520, (parent ? parent.width : 920) - 96))
    height: Math.min(680, Math.max(420, (parent ? parent.height : 680) - 96))
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    closePolicy: root.pluginBusy ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: DialogShell {
        accentColor: Theme.categoryInfo
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/plugin.svg"
        iconTint: Theme.categoryInfo
        accentColor: Theme.categoryInfo
        title: root.title
        subtitle: root.subtitleText
        closeText: "x"
        onCloseRequested: if (!root.pluginBusy) root.close()
    }

    footer: DialogFooter {
        Label {
            Layout.fillWidth: true
            text: root.pluginBusy ? "Applying changes..." : root.statusText
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            elide: Text.ElideRight
        }

        DialogActionButton {
            text: "Close"
            enabled: !root.pluginBusy
            onClicked: root.close()
        }

        DialogActionButton {
            text: "Apply to All"
            visible: root.pluginItem && typeof root.pluginItem.applyAll === "function"
            highlighted: true
            enabled: root.pluginCanApplyAll
            primaryColor: Theme.categoryInfo
            primaryHoverColor: Theme.categoryInfo
            primaryPressedColor: Theme.categoryInfo
            onClicked: root.applyAllPluginUi()
        }

        DialogActionButton {
            text: "Apply"
            highlighted: true
            enabled: root.pluginCanApply
            primaryColor: Theme.categoryInfo
            primaryHoverColor: Theme.categoryInfo
            primaryPressedColor: Theme.categoryInfo
            onClicked: root.applyPluginUi()
        }
    }

    contentItem: Item {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        clip: true

        Loader {
            id: pluginLoader
            anchors.fill: parent
            asynchronous: false

            onStatusChanged: {
                if (status === Loader.Error) {
                    root.statusText = "Plugin UI failed to load."
                }
            }
        }

        Label {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 520)
            visible: root.statusText.length > 0 && pluginLoader.status !== Loader.Ready
            text: root.statusText
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }
}
