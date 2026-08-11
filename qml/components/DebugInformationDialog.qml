import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "framework"
import "dialogs"

Dialog {
    id: root

    title: "Debug Information"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: 720
    height: 600
    padding: 0

    property var appRoot: null

    background: DialogShell {
        accentColor: Theme.accent
        shellBorderColor: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.28 : 0.20)
    }

    header: DialogHeader {
        iconSource: "../assets/icons-classic/info.svg"
        iconTint: Theme.accent
        accentColor: Theme.accent
        title: "Debug Information"
        subtitle: debugInformationController.generatedAtText.length > 0
                  ? "Snapshot " + debugInformationController.generatedAtText
                  : "Runtime support report"
        onCloseRequested: root.close()
    }

    contentItem: ScrollView {
        id: scrollView
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

        ColumnLayout {
            width: debugScrollBar.scrollNeeded
                   ? Math.max(0, debugScrollBar.x - 6)
                   : scrollView.availableWidth
            spacing: 18

            Item { Layout.preferredHeight: 2 }

            Repeater {
                model: debugInformationController.snapshot.sections || []

                delegate: DialogSection {
                    required property var modelData
                    title: modelData.title
                    accentColor: Theme.accent
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20

                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        columnSpacing: 18
                        rowSpacing: 7

                        Repeater {
                            model: modelData.rows

                            delegate: Item {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                implicitHeight: diagnosticColumn.implicitHeight

                                ColumnLayout {
                                    id: diagnosticColumn
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    spacing: 2

                                    Label {
                                        id: rowLabel
                                        Layout.fillWidth: true
                                        text: modelData.label + ":"
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSizeCaption
                                    }
                                    Label {
                                        id: rowValue
                                        Layout.fillWidth: true
                                        text: modelData.value
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.weight: Font.Medium
                                        wrapMode: Text.WrapAnywhere
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 4 }
        }
    }

    footer: DialogFooter {
        FmCheckBox {
            id: includePaths
            text: "Include file paths in copied report"
            ToolTip.visible: hovered
            ToolTip.delay: 350
            ToolTip.text: "The dialog always shows local paths. This option includes them in the copied report. Secrets are always removed."
        }
        Item { Layout.fillWidth: true }
        FmButton {
            text: "Refresh"
            onClicked: debugInformationController.refresh()
        }
        FmButton {
            text: "Copy Report"
            highlighted: true
            onClicked: {
                debugInformationController.copyReport(includePaths.checked)
                if (root.appRoot && root.appRoot.showTransientInfo) {
                    root.appRoot.showTransientInfo("Report copied")
                }
            }
        }
        FmButton {
            text: "Close"
            onClicked: root.close()
        }
    }
}
