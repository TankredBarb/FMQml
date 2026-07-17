import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "common"
import "dialogs"

Popup {
    id: root

    property var commands: []
    property string searchText: ""

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(720, parent.width - 40)
    height: Math.min(760, parent.height - 40)

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: {
        searchText = ""
        Qt.callLater(() => searchField.forceActiveFocus())
    }

    function commandShortcutItems(categories) {
        const result = []
        const source = commands || []
        for (let i = 0; i < source.length; ++i) {
            const command = source[i]
            if (!command || !command.shortcut || categories.indexOf(command.category) < 0) continue
            result.push({ key: command.shortcut.replace(/\+/g, " + "), desc: command.title })
        }
        return result
    }

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
                        text: "FM Help"
                        font.pixelSize: Theme.scaledSize(15)
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Label {
                        text: "Workflows, tools, and keyboard reference"
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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            color: "transparent"

            TextField {
                id: searchField
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                placeholderText: "Search actions or shortcuts..."
                text: root.searchText
                onTextChanged: root.searchText = text
                selectByMouse: true
                color: Theme.textPrimary
                placeholderTextColor: Theme.withAlpha(Theme.textPrimary, 0.5)

                background: Rectangle {
                    radius: Theme.radiusMd
                    color: Theme.panelSurfaceSoft
                    border.color: searchField.activeFocus ? Theme.accent : Theme.panelBorder
                    border.width: 1
                }
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
                                        text: "Start with the command palette"
                                        font.pixelSize: Theme.fontSizeBody
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: "Press Ctrl+K to find commands by name, including tools without dedicated shortcuts. Disabled commands explain what the current selection or location is missing."
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
                                    text: "F3: two panels"
                                    textColor: Theme.textPrimary
                                    fillColor: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.12 : 0.08)
                                    strokeColor: Theme.panelBorder
                                }
                                InlineBadge {
                                    text: "Space: Quick Look"
                                    textColor: Theme.textPrimary
                                    fillColor: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.12 : 0.08)
                                    strokeColor: Theme.panelBorder
                                }
                                InlineBadge {
                                    text: "F5: copy across"
                                    textColor: Theme.textPrimary
                                    fillColor: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.12 : 0.08)
                                    strokeColor: Theme.panelBorder
                                }
                                InlineBadge {
                                    text: "Ctrl+Shift+F: deep search"
                                    textColor: Theme.textPrimary
                                    fillColor: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.12 : 0.08)
                                    strokeColor: Theme.panelBorder
                                }
                            }
                        }
                    }

                    HelpSection {
                        title: "QUICK WORKFLOWS"
                        accentColor: Theme.categoryInfo
                        items: [
                            { key: "Two-panel transfer", desc: "Press F3, choose the destination in the other panel, then use F5 to copy or Shift+F5 to move." },
                            { key: "Inspect before opening", desc: "Press Space for Quick Look, or Ctrl+P to keep the preview pane visible while navigating." },
                            { key: "Find content", desc: "Ctrl+F filters the current listing; Ctrl+Shift+F searches recursively under the current folder." },
                            { key: "Compare folders", desc: "Open both folders in split view, then run Compare panel folders from Ctrl+K." },
                            { key: "Archives", desc: "Open an archive like a folder. Copy from it to extract selected content, or use context actions for full extraction." },
                            { key: "Remote locations", desc: "Open provider places from the sidebar. Transfers use the same operation drawer as local files." }
                        ]
                    }

                    HelpSection {
                        title: "WORKSPACE"
                        accentColor: Theme.categoryAction
                        items: [
                            { key: "F1", desc: "Open this help screen" },
                            { key: "Ctrl + K / Ctrl + Shift + P", desc: "Open the command palette" },
                            { key: "F3", desc: "Toggle split view" },
                            { key: "F4", desc: "Mirror the active panel path, view, sort, and filters" },
                            { key: "F9", desc: "Move focus between the sidebar and file panels" },
                            { key: "Tab", desc: "Switch focus between panels" }
                        ]
                    }

                    HelpSection {
                        title: "NAVIGATION & VIEW"
                        accentColor: Theme.categoryAction
                        items: root.commandShortcutItems(["Navigation", "View"])
                    }

                    HelpSection {
                        title: "OPEN & SELECT"
                        accentColor: Theme.accent
                        items: [
                            { key: "Enter", desc: "Open the selected file, folder, or drive" },
                            { key: "Space", desc: "Quick look for files or properties for folders" },
                            { key: "Ctrl + A", desc: "Select everything in the current file view" },
                            { key: "Ctrl + I", desc: "Invert the current selection" },
                            { key: "Esc", desc: "Clear selection or close the current overlay" }
                        ]
                    }

                    HelpSection {
                        title: "FILE ACTIONS"
                        accentColor: Theme.categoryInfo
                        items: root.commandShortcutItems(["File"])
                    }

                    HelpSection {
                        title: "INSPECT & TOOLS"
                        accentColor: Theme.accent
                        items: root.commandShortcutItems(["Inspect", "Tools"])
                    }

                    HelpSection {
                        title: "TOOLS WITHOUT SHORTCUTS"
                        accentColor: Theme.categoryInfo
                        items: [
                            { key: "Ctrl + K", desc: "Folder compare, disk usage, properties, checksums, archive creation, settings, themes, and administrator actions." },
                            { key: "Context menu", desc: "Provider-specific actions, Open With, archive commands, Favorites, and storage actions appear when applicable." },
                            { key: "Operation drawer", desc: "Monitor progress, speed and ETA, or cancel a running copy, move, delete, or extraction." }
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
                text: "Tip: Ctrl+K also shows why an unavailable command is disabled."
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
        readonly property var filteredItems: {
            const query = root.searchText.trim().toLowerCase()
            if (query.length === 0) return items
            const result = []
            for (let i = 0; i < items.length; ++i) {
                const item = items[i]
                const haystack = (String(item.key || "") + " " + String(item.desc || "")).toLowerCase()
                if (haystack.indexOf(query) >= 0) result.push(item)
            }
            return result
        }
        property real keyColumnWidth: 120
        Layout.fillWidth: true
        visible: filteredItems.length > 0
        spacing: 12

        function recomputeKeyWidth() {
            var maxLen = 0
            for (var i = 0; i < filteredItems.length; ++i) {
                var item = filteredItems[i]
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
                Layout.preferredWidth: 3
                Layout.preferredHeight: 12
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
                model: filteredItems
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
                            font.family: Theme.fontFamily
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
