import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"
import "dialogs"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 600
    height: 680

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 150; easing.type: Easing.OutBack }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.97; duration: 120; easing.type: Easing.InCubic }
    }

    background: DialogShell {
        accentColor: Theme.accent
        shellBorderColor: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.28 : 0.20)
    }

    contentItem: ColumnLayout {
        spacing: 0
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.close()
                event.accepted = true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 12

                RecolorSvgIcon {
                    sourcePath: "../assets/icons/info.svg"
                    recolorColor: Theme.categoryInfo
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignVCenter
                }

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    Label {
                        text: "Keyboard Help"
                        font.pixelSize: Theme.scaledSize(15)
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Label {
                        text: "Workspace, preview, themes, and file actions"
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textPrimary
                        opacity: 0.72
                    }
                }

                Button {
                    id: closeBtn
                    flat: true
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: root.close()

                    contentItem: Label {
                        text: "x"
                        font.pixelSize: Theme.fontSizeSubtitle
                        color: Theme.textPrimary
                        opacity: closeBtn.hovered ? 1.0 : 0.72
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: closeBtn.pressed ? Theme.surfaceActive : (closeBtn.hovered ? Theme.panelSurfaceSoft : "transparent")
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.26 : 0.18)
            }
        }

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

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

                    SurfaceCard {
                        Layout.fillWidth: true
                        cornerRadius: Theme.radiusLg
                        surfaceColor: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.09 : 0.11)
                        strokeColor: Theme.panelBorder

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Rectangle {
                                    Layout.preferredWidth: 36
                                    Layout.preferredHeight: 36
                                    radius: Theme.radiusMd
                                    color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.18 : 0.12)
                                    border.color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.28 : 0.18)
                                    border.width: 1

                                    RecolorSvgIcon {
                                        anchors.centerIn: parent
                                        sourcePath: "../assets/icons/info.svg"
                                        recolorColor: Theme.categoryInfo
                                        width: 18
                                        height: 18
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: "What is already available"
                                        font.pixelSize: Theme.fontSizeBody
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: "Fast panel switching, quick look, preview pane, theme commands, and file operations are reachable from the keyboard."
                                        font.pixelSize: Theme.fontSizeCaption
                                        wrapMode: Text.WordWrap
                                        color: Theme.textPrimary
                                        opacity: 0.74
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            Flow {
                                Layout.fillWidth: true
                                spacing: 8

                                InlineBadge {
                                    text: "Split view"
                                    textColor: Theme.textPrimary
                                    fillColor: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.12 : 0.08)
                                    strokeColor: Theme.panelBorder
                                }
                                InlineBadge {
                                    text: "Quick look"
                                    textColor: Theme.textPrimary
                                    fillColor: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.12 : 0.08)
                                    strokeColor: Theme.panelBorder
                                }
                                InlineBadge {
                                    text: "Theme commands"
                                    textColor: Theme.textPrimary
                                    fillColor: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.12 : 0.08)
                                    strokeColor: Theme.panelBorder
                                }
                                InlineBadge {
                                    text: "Preview pane"
                                    textColor: Theme.textPrimary
                                    fillColor: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.12 : 0.08)
                                    strokeColor: Theme.panelBorder
                                }
                            }
                        }
                    }

                    HelpSection {
                        title: "WORKSPACE"
                        accentColor: Theme.categoryInfo
                        items: [
                            { key: "F1", desc: "Open this help screen" },
                            { key: "Ctrl + K / Ctrl + Shift + P", desc: "Open the command palette" },
                            { key: "F3", desc: "Toggle split view" },
                            { key: "F9", desc: "Focus the sidebar" },
                            { key: "Tab", desc: "Switch focus between panels" },
                            { key: "Ctrl + P", desc: "Toggle the preview pane" }
                        ]
                    }

                    HelpSection {
                        title: "NAVIGATION"
                        accentColor: Theme.categoryAction
                        items: [
                            { key: "Enter", desc: "Open the selected file, folder, or drive" },
                            { key: "Space", desc: "Quick look for files or properties for folders" },
                            { key: "Alt + Up", desc: "Go to the parent folder" },
                            { key: "Alt + Left", desc: "Go back in folder history" },
                            { key: "Alt + Right", desc: "Go forward in folder history" },
                            { key: "Ctrl + L", desc: "Focus the path bar" },
                            { key: "Ctrl + F", desc: "Focus search" },
                            { key: "Ctrl + Shift + F", desc: "Search files under the current folder" },
                            { key: "Ctrl + R", desc: "Refresh the active panel" },
                            { key: "Esc", desc: "Clear selection or close the current overlay" }
                        ]
                    }

                    HelpSection {
                        title: "FILE ACTIONS"
                        accentColor: Theme.categoryInfo
                        items: [
                            { key: "F2", desc: "Rename the selected item or batch rename multiple items" },
                            { key: "F7 / Ctrl + Shift + N", desc: "Create a new folder" },
                            { key: "Ctrl + C", desc: "Copy selected items" },
                            { key: "Ctrl + X", desc: "Cut selected items" },
                            { key: "Ctrl + V", desc: "Paste from clipboard" },
                            { key: "F5", desc: "Copy the active selection to the opposite panel" },
                            { key: "Shift + F5", desc: "Move the active selection to the opposite panel" },
                            { key: "Del", desc: "Delete selected items" },
                            { key: "Ctrl + Z", desc: "Undo the last file operation" },
                            { key: "Ctrl + Y", desc: "Redo the last undone operation" },
                            { key: "Ctrl + A", desc: "Select everything in the current view" }
                        ]
                    }

                    HelpSection {
                        title: "VIEWS & PREVIEW"
                        accentColor: Theme.accent
                        items: [
                            { key: "Ctrl + 1", desc: "Switch the active panel to details view" },
                            { key: "Ctrl + 2", desc: "Switch the active panel to grid view" },
                            { key: "Ctrl + 3", desc: "Switch the active panel to brief view" },
                            { key: "Ctrl + H", desc: "Show or hide hidden files" }
                        ]
                    }

                    HelpSection {
                        title: "THEMES & COMMANDS"
                        accentColor: Theme.categoryInfo
                        items: [
                            { key: "Palette", desc: "Use the command palette to switch built-in schemes or open the theme selector" },
                            { key: "Settings", desc: "Export or import one settings file for workspace, panels, theme, and preferences" },
                            { key: "Theme aware UI", desc: "Dialogs, splash, preview, and panels adapt to the current scheme" },
                            { key: "Context actions", desc: "Properties, checksums, and quick look are available from the active file actions" }
                        ]
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "transparent"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.26 : 0.18)
            }

            Label {
                anchors.centerIn: parent
                text: "Shortcuts, previews, and theme controls stay aligned with the current UI."
                font.pixelSize: Theme.fontSizeMicro
                color: Theme.textPrimary
                opacity: 0.5
                font.italic: true
            }
        }
    }

    component HelpSection: ColumnLayout {
        property string title
        property color accentColor: Theme.accent
        property var items: []
        property real keyColumnWidth: 120
        Layout.fillWidth: true
        spacing: 12

        function recomputeKeyWidth() {
            var maxLen = 0
            for (var i = 0; i < items.length; ++i) {
                var item = items[i]
                if (item && item.key) {
                    maxLen = Math.max(maxLen, String(item.key).length)
                }
            }
            keyColumnWidth = Math.max(120, Math.min(260, maxLen * 7 + 24))
        }

        Component.onCompleted: recomputeKeyWidth()
        onItemsChanged: recomputeKeyWidth()

        RowLayout {
            spacing: 8
            Rectangle {
                width: 3
                height: 12
                radius: 1.5
                color: accentColor
            }
            Label {
                text: title
                font.bold: true
                font.pixelSize: Theme.fontSizeCaption
                font.letterSpacing: 1.0
                color: Theme.textPrimary
                Layout.fillWidth: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: items
                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Rectangle {
                        id: keycapRect
                        Layout.preferredWidth: keyColumnWidth
                        Layout.minimumWidth: keyColumnWidth
                        Layout.preferredHeight: 24
                        color: Theme.panelSurfaceSoft
                        radius: Theme.radiusSm
                        border.color: Theme.panelBorder
                        border.width: 1

                        Label {
                            id: keycapText
                            anchors.centerIn: parent
                            text: modelData.key
                            font.family: "Segoe UI", "Inter", "sans-serif"
                            font.pixelSize: Theme.fontSizeMicro
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                    }

                    Label {
                        text: modelData.desc
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeLabel
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
