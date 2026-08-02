import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "common"
import "framework"
import "dialogs"

Dialog {
    id: root

    title: "Debug Information"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: 640
    height: 520
    padding: 0

    property var appRoot: null
    property string workingDirectory: ""

    function refreshWorkingDirectory() {
        if (typeof workspaceController !== "undefined" && workspaceController && workspaceController.applicationDirectory !== undefined) {
            root.workingDirectory = workspaceController.applicationDirectory
        } else {
            root.workingDirectory = "Unavailable"
        }
    }

    onAboutToShow: root.refreshWorkingDirectory()

    background: DialogShell {
        accentColor: Theme.accent
        shellBorderColor: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.28 : 0.20)
    }

    header: DialogHeader {
        iconSource: "../assets/icons-classic/info.svg"
        iconTint: Theme.accent
        accentColor: Theme.accent
        title: "Debug Information"
        subtitle: "Hidden diagnostics for runtime state"
        onCloseRequested: root.close()
    }

    contentItem: ColumnLayout {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        spacing: 0
        clip: true
        focus: true

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: FmScrollBar {
                id: debugScrollBar
                parent: scrollView.contentItem
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                policy: ScrollBar.AsNeeded
            }

            Pane {
                width: debugScrollBar.scrollNeeded
                       ? Math.max(0, debugScrollBar.x - 6)
                       : scrollView.availableWidth
                padding: 20
                background: null

                ColumnLayout {
                    width: parent.width
                    spacing: 18

                    // Section 1: RUNTIME PATHS & ENVIRONMENT
                    DialogSection {
                        title: "APPLICATION & SYSTEM"
                        accentColor: Theme.accent
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            rowSpacing: 6
                            columnSpacing: 16

                            Label { text: "Binary directory:"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                            Label { text: root.workingDirectory; font.pixelSize: Theme.fontSizeCaption; color: Theme.textPrimary; font.weight: Font.Medium; elide: Text.ElideMiddle; Layout.fillWidth: true }

                            Label { text: "OS Name:"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                            Label { text: (typeof systemInfoProvider !== "undefined") ? systemInfoProvider.osName : "Unknown"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textPrimary; font.weight: Font.Medium }

                            Label { text: "Qt Version:"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                            Label { text: (typeof workspaceController !== "undefined") ? workspaceController.qtVersion() : "Unknown"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textPrimary; font.weight: Font.Medium }

                            Label { text: "Display Details:"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                            Label { text: (root.appRoot ? (root.appRoot.width + "x" + root.appRoot.height) : "Unknown") + " (DPI Scaling: " + Screen.devicePixelRatio.toFixed(1) + ")"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textPrimary; font.weight: Font.Medium }

                            Label { text: "Process Memory RSS:"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                            Label { text: (typeof workspaceController !== "undefined") ? ((workspaceController.processMemoryUsage() / (1024.0 * 1024.0)).toFixed(1) + " MB") : "Unknown"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textPrimary; font.weight: Font.Medium }
                        }
                    }

                    // Section 2: CLIPBOARD DIAGNOSTICS
                    DialogSection {
                        title: "CLIPBOARD DIAGNOSTICS"
                        accentColor: Theme.accent
                        Layout.fillWidth: true

                        Label {
                            text: (typeof workspaceController !== "undefined" && workspaceController.clipboardSummary) ? workspaceController.clipboardSummary : "No clipboard summary"
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            visible: typeof workspaceController !== "undefined" && workspaceController.clipboardPaths().length > 0

                            Repeater {
                                model: (typeof workspaceController !== "undefined") ? workspaceController.clipboardPaths() : []
                                delegate: Label {
                                    text: modelData
                                    font.family: "Consolas"
                                    font.pixelSize: Theme.fontSizeMicro
                                    color: Theme.textSecondary
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Label {
                            text: "Clipboard is empty."
                            font.pixelSize: Theme.fontSizeCaption
                            font.italic: true
                            color: Theme.textSecondary
                            visible: typeof workspaceController === "undefined" || workspaceController.clipboardPaths().length === 0
                        }
                    }

                    // Section 3: PANELS & HISTORY
                    DialogSection {
                        title: "PANELS & HISTORY"
                        accentColor: Theme.accent
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            Layout.fillWidth: true
                            rowSpacing: 12
                            columnSpacing: 20

                            // Headers
                            Label { text: "Left Panel"; font.bold: true; font.pixelSize: Theme.fontSizeLabel; color: Theme.textPrimary; Layout.fillWidth: true }
                            Label { text: "Right Panel"; font.bold: true; font.pixelSize: Theme.fontSizeLabel; color: Theme.textPrimary; Layout.fillWidth: true }

                            // Paths
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.leftPanel) ? workspaceController.leftPanel.currentPath : "N/A"
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.textSecondary
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.rightPanel) ? workspaceController.rightPanel.currentPath : "N/A"
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.textSecondary
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            // History stack sizes
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.leftPanel) ? ("History: " + workspaceController.leftPanel.backStackCount + " back, " + workspaceController.leftPanel.forwardStackCount + " forward") : "N/A"
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.textSecondary
                            }
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.rightPanel) ? ("History: " + workspaceController.rightPanel.backStackCount + " back, " + workspaceController.rightPanel.forwardStackCount + " forward") : "N/A"
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.textSecondary
                            }

                            // Selection
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.leftPanel) ? ("Selection: " + workspaceController.leftPanel.directoryModel.selectedCount + " items selected") : "N/A"
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.textSecondary
                            }
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.rightPanel) ? ("Selection: " + workspaceController.rightPanel.directoryModel.selectedCount + " items selected") : "N/A"
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.textSecondary
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.panelBorder
                            opacity: 0.5
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                        }

                        Label {
                            text: (typeof workspaceController !== "undefined" && workspaceController.historyManager) ? ("Undo stack count: " + workspaceController.historyManager.undoCount + " | Redo stack count: " + workspaceController.historyManager.redoCount) : "History manager unavailable"
                            font.pixelSize: Theme.fontSizeCaption
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                        }
                    }

                    // Section 4: PLUGINS & PROVIDERS
                    DialogSection {
                        title: "PLUGINS & PROVIDERS"
                        accentColor: Theme.accent
                        Layout.fillWidth: true

                        Label {
                            text: "Built-in providers: local (file://), archive (archive://)"
                            font.pixelSize: Theme.fontSizeCaption
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                        }

                        Label {
                            text: "Loaded custom plugins:"
                            font.pixelSize: Theme.fontSizeCaption
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            Layout.topMargin: 4
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: typeof workspaceController !== "undefined" && workspaceController.loadedPlugins().length > 0

                            Repeater {
                                model: (typeof workspaceController !== "undefined") ? workspaceController.loadedPlugins() : []
                                delegate: ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Label {
                                        text: modelData.displayName + " (" + modelData.pluginId + ")"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.weight: Font.Medium
                                        color: Theme.textPrimary
                                    }
                                    Label {
                                        text: "Path: " + modelData.filePath
                                        font.pixelSize: Theme.fontSizeMicro
                                        color: Theme.textSecondary
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: "Schemes: " + (modelData.schemes.length > 0 ? modelData.schemes.join(", ") : "none")
                                        font.pixelSize: Theme.fontSizeMicro
                                        color: Theme.textSecondary
                                    }
                                }
                            }
                        }

                        Label {
                            text: "No custom plugins loaded."
                            font.pixelSize: Theme.fontSizeCaption
                            font.italic: true
                            color: Theme.textSecondary
                            visible: typeof workspaceController === "undefined" || workspaceController.loadedPlugins().length === 0
                        }
                    }

                    DialogSection {
                        title: "FRAMEWORK COMPONENTS"
                        accentColor: Theme.accent
                        Layout.fillWidth: true

                        Label {
                            text: "FmSlider"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        GridLayout {
                            columns: 2
                            Layout.fillWidth: true
                            columnSpacing: 16
                            rowSpacing: 10

                            Label { text: "Interactive"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            RowLayout {
                                Layout.fillWidth: true
                                FmSlider {
                                    id: debugInteractiveSlider
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    value: 38
                                }
                                Label {
                                    text: Math.round(debugInteractiveSlider.value) + "%"
                                    color: Theme.accent
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.weight: Font.DemiBold
                                    Layout.preferredWidth: 38
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            Label { text: "Stepped"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            RowLayout {
                                Layout.fillWidth: true
                                FmSlider {
                                    id: debugSteppedSlider
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 10
                                    stepSize: 1
                                    snapMode: Slider.SnapAlways
                                    value: 7
                                }
                                Label {
                                    text: Math.round(debugSteppedSlider.value) + " / 10"
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeCaption
                                    Layout.preferredWidth: 38
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            Label { text: "Disabled"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmSlider {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: 58
                                enabled: false
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                            color: Theme.withAlpha(Theme.panelBorder, 0.5)
                        }

                        Label {
                            text: "FmProgressBar"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        GridLayout {
                            columns: 2
                            Layout.fillWidth: true
                            columnSpacing: 16
                            rowSpacing: 10

                            Label { text: "Progress"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmProgressBar { Layout.fillWidth: true; value: 0.63 }

                            Label { text: "Indeterminate"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmProgressBar { Layout.fillWidth: true; indeterminate: true }

                            Label { text: "Error"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmProgressBar { Layout.fillWidth: true; value: 0.78; fillColor: Theme.danger }

                            Label { text: "Disabled"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmProgressBar { Layout.fillWidth: true; value: 0.46; enabled: false }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                            color: Theme.withAlpha(Theme.panelBorder, 0.5)
                        }

                        Label {
                            text: "FmSwitch"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 22

                            FmSwitch { text: "Off" }
                            FmSwitch { text: "On"; checked: true }
                            FmSwitch { text: "Disabled"; checked: true; enabled: false }
                            Item { Layout.fillWidth: true }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                            color: Theme.withAlpha(Theme.panelBorder, 0.5)
                        }

                        Label {
                            text: "FmCheckBox"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 18

                            FmCheckBox { text: "Off" }
                            FmCheckBox { text: "On"; checked: true }
                            FmCheckBox { text: "Mixed"; tristate: true; checkState: Qt.PartiallyChecked }
                            FmCheckBox { text: "Disabled"; checked: true; enabled: false }
                            Item { Layout.fillWidth: true }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                            color: Theme.withAlpha(Theme.panelBorder, 0.5)
                        }

                        Label {
                            text: "FmComboBox"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        GridLayout {
                            columns: 2
                            Layout.fillWidth: true
                            columnSpacing: 16
                            rowSpacing: 10

                            Label { text: "Standard"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmComboBox { Layout.fillWidth: true; model: ["Grid", "Details", "Brief"] }

                            Label { text: "Editable"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmComboBox { Layout.fillWidth: true; editable: true; model: ["Name", "Extension", "Date", "Size"] }

                            Label { text: "Long list"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmComboBox {
                                Layout.fillWidth: true
                                model: ["Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel", "India", "Juliet", "Kilo", "Lima"]
                            }

                            Label { text: "Disabled"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmComboBox { Layout.fillWidth: true; model: ["Unavailable"]; enabled: false }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                            color: Theme.withAlpha(Theme.panelBorder, 0.5)
                        }

                        Label {
                            text: "FmTabBar"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        FmTabBar {
                            Layout.fillWidth: true
                            model: [{ text: "General", value: 0 },
                                    { text: "Details", value: 1 },
                                    { text: "Advanced", value: 2 }]
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                            color: Theme.withAlpha(Theme.panelBorder, 0.5)
                        }

                        Label {
                            text: "FmTextField"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        GridLayout {
                            columns: 2
                            Layout.fillWidth: true
                            columnSpacing: 16
                            rowSpacing: 10

                            Label { text: "Standard"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmTextField { Layout.fillWidth: true; placeholderText: "Enter text" }

                            Label { text: "With value"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmTextField { Layout.fillWidth: true; text: "File Manager" }

                            Label { text: "Error"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmTextField { Layout.fillWidth: true; text: "Invalid value"; error: true }

                            Label { text: "Disabled"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmTextField { Layout.fillWidth: true; text: "Unavailable"; enabled: false }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                            color: Theme.withAlpha(Theme.panelBorder, 0.5)
                        }

                        Label {
                            text: "FmTextArea"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        GridLayout {
                            columns: 2
                            Layout.fillWidth: true
                            columnSpacing: 16
                            rowSpacing: 10

                            Label { text: "Standard"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmTextArea {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 96
                                placeholderText: "Enter multiple lines"
                            }

                            Label { text: "Error"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmTextArea {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 96
                                text: "The first line\nThe second line"
                                error: true
                            }

                            Label { text: "Disabled"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                            FmTextArea {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 72
                                text: "Unavailable"
                                enabled: false
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                            color: Theme.withAlpha(Theme.panelBorder, 0.5)
                        }

                        Label {
                            text: "FmSpinBox"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 18

                            FmSpinBox { from: 0; to: 100; value: 42 }
                            FmSpinBox { from: 1; to: 10; value: 2 }
                            FmSpinBox { from: -50; to: 50; value: -7 }
                            FmSpinBox { from: 0; to: 10; value: 5; enabled: false }
                            Item { Layout.fillWidth: true }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                            color: Theme.withAlpha(Theme.panelBorder, 0.5)
                        }

                        Label {
                            text: "FmButton"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            FmButton { text: "Standard" }
                            FmButton { text: "Primary"; highlighted: true }
                            FmButton { text: "Danger"; highlighted: true; destructive: true; primaryColor: Theme.danger }
                            FmButton { text: "Flat"; flat: true }
                            FmButton { text: "Disabled"; enabled: false }
                            Item { Layout.fillWidth: true }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                            color: Theme.withAlpha(Theme.panelBorder, 0.5)
                        }

                        Label {
                            text: "FmIconButton"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/refresh.svg"; iconTone: "refresh" }
                            FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg"; iconTone: "folder"; isHighlighted: true }
                            FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/delete.svg"; iconTone: "danger" }
                            FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/eye.svg"; iconTone: "preview"; enabled: false }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }

    footer: DialogFooter {
        Item { Layout.fillWidth: true }
        DialogActionButton {
            text: "Close"
            onClicked: root.close()
        }
    }
}
