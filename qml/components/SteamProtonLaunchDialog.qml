import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import "../style"
import "dialogs"
import "common"
import "framework"

Popup {
    id: root

    property var controller: null
    property string targetPath: ""
    property var options: ({})
    property var runtimeChoices: []
    property string selectedRuntimeId: "auto"
    property bool vkBasaltEnabled: false
    property bool captureLog: false
    property bool clearXModifiers: false
    property string launchStatus: ""
    readonly property color dialogAccent: Theme.categoryAction
    readonly property color rowBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.26 : 0.20)
    readonly property bool hasRuntime: root.options.available === true && root.runtimeChoices.length > 1

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.92, 560)
    padding: 20

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openFor(targetController, path) {
        root.controller = targetController
        root.targetPath = path || ""
        root.launchStatus = ""
        root.refreshOptions()
        root.open()
    }

    function refreshOptions() {
        root.options = root.controller && root.controller.steamProtonLaunchOptionsForPath
                ? root.controller.steamProtonLaunchOptionsForPath(root.targetPath)
                : ({})
        const runtimes = root.options.runtimes || []
        const choices = [{ id: "auto", name: "Auto", path: "", source: "" }]
        for (let i = 0; i < runtimes.length; ++i) {
            choices.push(runtimes[i])
        }
        root.runtimeChoices = choices
        root.selectedRuntimeId = root.options.selectedRuntimeId || "auto"
        root.vkBasaltEnabled = root.options.vkBasaltEnabled === true
        root.captureLog = root.options.captureLog === true
        root.clearXModifiers = root.options.clearXModifiers === true
        runtimeCombo.currentIndex = root.indexForRuntime(root.selectedRuntimeId)
    }

    function indexForRuntime(runtimeId) {
        for (let i = 0; i < root.runtimeChoices.length; ++i) {
            if (root.runtimeChoices[i].id === runtimeId) {
                return i
            }
        }
        return 0
    }

    function selectedRuntime() {
        const index = runtimeCombo.currentIndex >= 0 ? runtimeCombo.currentIndex : 0
        return root.runtimeChoices[Math.min(index, root.runtimeChoices.length - 1)] || root.runtimeChoices[0]
    }

    function launch() {
        if (!root.controller || !root.hasRuntime) {
            return
        }
        const runtime = root.selectedRuntime()
        const result = root.controller.launchPathWithSteamProton(
                    root.targetPath,
                    runtime.id || "auto",
                    root.vkBasaltEnabled,
                    root.captureLog,
                    root.clearXModifiers)
        if (result && result.ok === true) {
            root.launchStatus = root.captureLog
                    ? "Launched. Proton log: " + (result.details || root.options.logDirectory || "")
                    : "Launched."
        } else {
            root.launchStatus = result && result.message ? result.message : "Could not start Steam Proton."
        }
    }

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    background: DialogShell {
        accentColor: root.dialogAccent
        shellBorderColor: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.30 : 0.22)
    }

    contentItem: ColumnLayout {
        spacing: 16
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                root.close()
                event.accepted = true
            } else if ((event.key === Qt.Key_Enter || event.key === Qt.Key_Return) && root.hasRuntime) {
                root.launch()
                event.accepted = true
            }
        }

        DialogHeader {
            Layout.fillWidth: true
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg"
            iconTint: root.dialogAccent
            accentColor: root.dialogAccent
            title: "Steam Proton"
            subtitle: root.options.targetName || "Windows application"
            showCloseButton: true
            onCloseRequested: root.close()
        }

        SurfaceCard {
            Layout.fillWidth: true
            implicitHeight: targetLayout.implicitHeight + 18
            surfaceColor: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.08 : 0.045)
            strokeColor: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.24 : 0.18)

            ColumnLayout {
                id: targetLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: root.options.targetName || "Selected application"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                }

                Label {
                    Layout.fillWidth: true
                    text: root.options.targetPath || root.targetPath
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    elide: Text.ElideMiddle
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Label {
                    text: "Proton"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLabel
                    Layout.preferredWidth: 92
                }

                ProtonRuntimeComboBox {
                    id: runtimeCombo
                    Layout.fillWidth: true
                    enabled: root.hasRuntime
                    model: root.runtimeChoices
                    textRole: "name"
                    valueRole: "id"
                    onActivated: root.selectedRuntimeId = currentValue || "auto"
                }

                FmButton {
                    text: "Refresh"
                    highlighted: false
                    onClicked: root.refreshOptions()
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.hasRuntime
                text: {
                    const runtime = root.selectedRuntime()
                    return runtime && runtime.path ? runtime.path : "FMQml will use the newest discovered Steam Proton."
                }
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
                elide: Text.ElideMiddle
            }

            Label {
                Layout.fillWidth: true
                visible: !root.hasRuntime
                text: root.options.errorMessage || "Install Steam and a Proton compatibility tool, then try again."
                color: Theme.warning
                font.pixelSize: Theme.fontSizeCaption
                wrapMode: Text.Wrap
            }
        }

        SurfaceCard {
            Layout.fillWidth: true
            implicitHeight: settingsLayout.implicitHeight + 18
            surfaceColor: Theme.panelSurfaceSoft
            strokeColor: Theme.panelBorder

            ColumnLayout {
                id: settingsLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                FmToggleRow {
                    Layout.fillWidth: true
                    title: "vkBasalt"
                    subtitle: root.options.vkBasaltMessage || "vkBasalt was not found on this system."
                    checked: root.vkBasaltEnabled
                    toggleEnabled: root.options.vkBasaltAvailable === true
                    accentColor: root.options.vkBasaltAvailable === true ? root.dialogAccent : Theme.warning
                    titleColor: root.options.vkBasaltAvailable === true
                                ? Theme.textPrimary
                                : Theme.withAlpha(Theme.textPrimary, 0.66)
                    subtitleColor: root.options.vkBasaltAvailable === true
                                   ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.74 : 0.82)
                                   : Theme.warning
                    onToggled: (checked) => root.vkBasaltEnabled = checked
                }

                FmToggleRow {
                    Layout.fillWidth: true
                    title: "Capture Proton log"
                    subtitle: root.captureLog
                              ? (root.options.logFile || root.options.logDirectory || "")
                              : "Write Proton logs for this launch."
                    checked: root.captureLog
                    toggleEnabled: true
                    accentColor: root.dialogAccent
                    onToggled: (checked) => root.captureLog = checked
                }

                FmToggleRow {
                    Layout.fillWidth: true
                    title: "Clear XMODIFIERS"
                    subtitle: "Launch with XMODIFIERS set to an empty value."
                    checked: root.clearXModifiers
                    toggleEnabled: true
                    accentColor: root.dialogAccent
                    onToggled: (checked) => root.clearXModifiers = checked
                }

            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.launchStatus.length > 0
            text: root.launchStatus
            color: root.launchStatus === "Launched." || root.launchStatus.startsWith("Launched.")
                   ? Theme.textSecondary
                   : Theme.warning
            font.pixelSize: Theme.fontSizeCaption
            wrapMode: Text.Wrap
        }

        DialogFooter {
            Layout.fillWidth: true

            FmButton {
                text: "Close"
                Layout.fillWidth: true
                highlighted: false
                onClicked: root.close()
            }

            FmButton {
                text: "Launch"
                Layout.fillWidth: true
                highlighted: true
                enabled: root.hasRuntime
                primaryColor: root.dialogAccent
                primaryHoverColor: root.dialogAccent
                primaryPressedColor: root.dialogAccent
                onClicked: root.launch()
            }
        }
    }

    component ProtonRuntimeComboBox: FmComboBox {
        id: combo
        accentColor: root.dialogAccent
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeLabel
        font.weight: Font.Medium

        delegate: FmMenuItem {
            width: combo.width
            height: Math.max(36, Theme.controlHeight - 2)
            useHighlightedState: true
            highlighted: combo.highlightedIndex === index
            active: combo.currentIndex === index

            contentItem: ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    text: modelData.name || ""
                    color: highlighted ? Theme.textPrimary : Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: highlighted ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible: modelData.path && modelData.path.length > 0
                    text: modelData.path || ""
                    color: Theme.withAlpha(Theme.textSecondary, 0.72)
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeMicro
                    elide: Text.ElideMiddle
                }
            }

        }

    }

}
