import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "common"
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
        iconSource: "../assets/icons/info.svg"
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
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            Pane {
                width: scrollView.availableWidth
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

                            Label { text: "Binary directory:"; font.pixelSize: 11; color: Theme.textSecondary }
                            Label { text: root.workingDirectory; font.pixelSize: 11; color: Theme.textPrimary; font.weight: Font.Medium; elide: Text.ElideMiddle; Layout.fillWidth: true }

                            Label { text: "OS Name:"; font.pixelSize: 11; color: Theme.textSecondary }
                            Label { text: (typeof systemInfoProvider !== "undefined") ? systemInfoProvider.osName : "Unknown"; font.pixelSize: 11; color: Theme.textPrimary; font.weight: Font.Medium }

                            Label { text: "Qt Version:"; font.pixelSize: 11; color: Theme.textSecondary }
                            Label { text: (typeof workspaceController !== "undefined") ? workspaceController.qtVersion() : "Unknown"; font.pixelSize: 11; color: Theme.textPrimary; font.weight: Font.Medium }

                            Label { text: "Display Details:"; font.pixelSize: 11; color: Theme.textSecondary }
                            Label { text: (root.appRoot ? (root.appRoot.width + "x" + root.appRoot.height) : "Unknown") + " (DPI Scaling: " + Screen.devicePixelRatio.toFixed(1) + ")"; font.pixelSize: 11; color: Theme.textPrimary; font.weight: Font.Medium }

                            Label { text: "Process Memory RSS:"; font.pixelSize: 11; color: Theme.textSecondary }
                            Label { text: (typeof workspaceController !== "undefined") ? ((workspaceController.processMemoryUsage() / (1024.0 * 1024.0)).toFixed(1) + " MB") : "Unknown"; font.pixelSize: 11; color: Theme.textPrimary; font.weight: Font.Medium }
                        }
                    }

                    // Section 2: CLIPBOARD DIAGNOSTICS
                    DialogSection {
                        title: "CLIPBOARD DIAGNOSTICS"
                        accentColor: Theme.accent
                        Layout.fillWidth: true

                        Label {
                            text: (typeof workspaceController !== "undefined" && workspaceController.clipboardSummary) ? workspaceController.clipboardSummary : "No clipboard summary"
                            font.pixelSize: 12
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
                                    font.pixelSize: 10
                                    color: Theme.textSecondary
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Label {
                            text: "Clipboard is empty."
                            font.pixelSize: 11
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
                            Label { text: "Left Panel"; font.bold: true; font.pixelSize: 12; color: Theme.textPrimary; Layout.preferredWidth: parent.width / 2 - 10 }
                            Label { text: "Right Panel"; font.bold: true; font.pixelSize: 12; color: Theme.textPrimary; Layout.preferredWidth: parent.width / 2 - 10 }

                            // Paths
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.leftPanel) ? workspaceController.leftPanel.currentPath : "N/A"
                                font.pixelSize: 11
                                color: Theme.textSecondary
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.rightPanel) ? workspaceController.rightPanel.currentPath : "N/A"
                                font.pixelSize: 11
                                color: Theme.textSecondary
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            // History stack sizes
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.leftPanel) ? ("History: " + workspaceController.leftPanel.backStackCount + " back, " + workspaceController.leftPanel.forwardStackCount + " forward") : "N/A"
                                font.pixelSize: 11
                                color: Theme.textSecondary
                            }
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.rightPanel) ? ("History: " + workspaceController.rightPanel.backStackCount + " back, " + workspaceController.rightPanel.forwardStackCount + " forward") : "N/A"
                                font.pixelSize: 11
                                color: Theme.textSecondary
                            }

                            // Selection
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.leftPanel) ? ("Selection: " + workspaceController.leftPanel.directoryModel.selectedCount + " items selected") : "N/A"
                                font.pixelSize: 11
                                color: Theme.textSecondary
                            }
                            Label {
                                text: (typeof workspaceController !== "undefined" && workspaceController.rightPanel) ? ("Selection: " + workspaceController.rightPanel.directoryModel.selectedCount + " items selected") : "N/A"
                                font.pixelSize: 11
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
                            font.pixelSize: 11
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
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                        }

                        Label {
                            text: "Loaded custom plugins:"
                            font.pixelSize: 11
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
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        color: Theme.textPrimary
                                    }
                                    Label {
                                        text: "Path: " + modelData.filePath
                                        font.pixelSize: 10
                                        color: Theme.textSecondary
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: "Schemes: " + (modelData.schemes.length > 0 ? modelData.schemes.join(", ") : "none")
                                        font.pixelSize: 10
                                        color: Theme.textSecondary
                                    }
                                }
                            }
                        }

                        Label {
                            text: "No custom plugins loaded."
                            font.pixelSize: 11
                            font.italic: true
                            color: Theme.textSecondary
                            visible: typeof workspaceController === "undefined" || workspaceController.loadedPlugins().length === 0
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
