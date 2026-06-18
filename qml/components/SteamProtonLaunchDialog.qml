import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import "../style"
import "dialogs"
import "common"

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
    readonly property color rowFill: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.30 : 0.52)
    readonly property color rowFillHover: Theme.withAlpha(Theme.surfaceHover, themeController.isDark ? 0.42 : 0.58)
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
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/open.svg"
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

                DialogActionButton {
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

                ProtonToggleRow {
                    Layout.fillWidth: true
                    title: "vkBasalt"
                    subtitle: root.options.vkBasaltMessage || "vkBasalt was not found on this system."
                    checked: root.vkBasaltEnabled
                    toggleEnabled: root.options.vkBasaltAvailable === true
                    accentColor: root.dialogAccent
                    warning: root.options.vkBasaltAvailable !== true
                    onToggled: (checked) => root.vkBasaltEnabled = checked
                }

                ProtonToggleRow {
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

                ProtonToggleRow {
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

            DialogActionButton {
                text: "Close"
                Layout.fillWidth: true
                highlighted: false
                onClicked: root.close()
            }

            DialogActionButton {
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

    component ProtonRuntimeComboBox: ComboBox {
        id: combo

        delegate: ItemDelegate {
            width: combo.width
            height: Math.max(36, Theme.controlHeight - 2)
            highlighted: combo.highlightedIndex === index

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

            background: Rectangle {
                radius: Theme.radiusSm
                color: highlighted || hovered ? Theme.menuItemHover : "transparent"
            }
        }

        indicator: RecolorSvgIcon {
            x: combo.width - width - 10
            y: Math.round((combo.height - height) / 2)
            width: 10
            height: 10
            sourcePath: "../assets/icons/arrow-up.svg"
            sourceSize: Qt.size(16, 16)
            recolorEnabled: true
            recolorColor: combo.enabled ? Theme.textSecondary : Theme.withAlpha(Theme.textSecondary, 0.42)
            rotation: combo.opened ? 0 : 180
            opacity: 0.78
        }

        contentItem: Label {
            leftPadding: 10
            rightPadding: 26
            text: combo.displayText
            color: combo.enabled ? Theme.textPrimary : Theme.withAlpha(Theme.textSecondary, 0.58)
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.Medium
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            implicitHeight: Theme.controlHeight
            radius: Theme.radiusSm
            color: combo.enabled ? Theme.panelSurfaceSoft : Theme.withAlpha(Theme.panelSurfaceSoft, 0.46)
            border.color: combo.opened ? root.dialogAccent : Theme.panelBorder
            border.width: combo.opened ? 2 : 1
        }

        popup: Popup {
            y: combo.height + 4
            width: combo.width
            padding: 4
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }

            background: Rectangle {
                color: Theme.menuSurface
                radius: Theme.radiusSm
                border.color: Theme.menuBorder
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Theme.glassShadow
                    shadowBlur: 16
                    shadowVerticalOffset: 6
                }
            }
        }
    }

    component ProtonToggleRow: Rectangle {
        id: row

        property string title: ""
        property string subtitle: ""
        property bool checked: false
        property bool toggleEnabled: true
        property bool warning: false
        property color accentColor: Theme.accent
        signal toggled(bool checked)

        implicitHeight: Math.max(52, rowLayout.implicitHeight + 12)
        radius: Theme.radiusSm
        color: rowMouse.containsMouse && row.toggleEnabled ? root.rowFillHover : root.rowFill
        border.color: row.warning && !row.toggleEnabled
                      ? Theme.withAlpha(Theme.warning, themeController.isDark ? 0.46 : 0.34)
                      : Theme.withAlpha(row.accentColor,
                                        row.checked
                                        ? (themeController.isDark ? 0.40 : 0.34)
                                        : (themeController.isDark ? 0.26 : 0.24))
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 7
            width: 2
            radius: 1
            opacity: row.checked ? 1.0 : 0.58
            color: row.warning && !row.toggleEnabled
                   ? Theme.warning
                   : Theme.withAlpha(row.accentColor, themeController.isDark ? 0.86 : 0.72)
        }

        RowLayout {
            id: rowLayout
            anchors.fill: parent
            anchors.leftMargin: row.checked ? 14 : 10
            anchors.rightMargin: 10
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: row.title
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                    color: row.toggleEnabled ? Theme.textPrimary : Theme.withAlpha(Theme.textPrimary, 0.66)
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: row.subtitle
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    color: row.warning && !row.toggleEnabled
                           ? Theme.warning
                           : Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.74 : 0.82)
                }
            }

            Switch {
                id: switchControl
                checked: row.checked
                enabled: row.toggleEnabled
                Layout.preferredWidth: 46
                Layout.preferredHeight: 26

                indicator: Rectangle {
                    implicitWidth: 40
                    implicitHeight: 22
                    x: switchControl.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: height / 2
                    color: switchControl.checked
                           ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.34 : 0.22)
                           : Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.82 : 0.92)
                    border.color: switchControl.checked
                                  ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.62 : 0.44)
                                  : root.rowBorder
                    border.width: 1
                    opacity: row.toggleEnabled ? 1.0 : 0.62

                    Rectangle {
                        x: switchControl.checked ? parent.width - width - 3 : 3
                        anchors.verticalCenter: parent.verticalCenter
                        width: 15
                        height: 15
                        radius: 7.5
                        color: switchControl.checked ? row.accentColor : Theme.withAlpha(Theme.textSecondary, 0.78)

                        Behavior on x {
                            NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
                        }
                    }
                }

                contentItem: Item {}
            }
        }

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            enabled: row.toggleEnabled
            cursorShape: row.toggleEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: row.toggled(!row.checked)
        }
    }
}
