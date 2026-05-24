import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    property var commands: []
    property var filteredCommands: []
    property string query: ""
    property int selectedIndex: -1

    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(parent ? parent.width * 0.72 : 720, 720)
    height: Math.min(parent ? parent.height * 0.72 : 520, 520)
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: Theme.motionNormal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1.0; duration: Theme.motionNormal; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.98; duration: 120; easing.type: Easing.InCubic }
    }

    Overlay.modal: Rectangle {
        color: Theme.overlayScrim
    }

    onAboutToShow: {
        opacity = 0.0
        scale = 0.96
    }

    function normalize(value) {
        return String(value || "").toLowerCase().trim()
    }

    function commandText(command) {
        if (!command) return ""
        const parts = []
        if (command.title) parts.push(command.title)
        if (command.subtitle) parts.push(command.subtitle)
        if (command.shortcut) parts.push(command.shortcut)
        if (command.keywords && command.keywords.length > 0) parts.push(command.keywords.join(" "))
        return normalize(parts.join(" "))
    }

    function matchesTokens(command, tokens) {
        const haystack = commandText(command)
        if (haystack.length === 0) return false
        for (let i = 0; i < tokens.length; ++i) {
            if (haystack.indexOf(tokens[i]) < 0) {
                return false
            }
        }
        return true
    }

    function scoreCommand(command, queryText, tokens) {
        if (!command) return -1
        if (tokens.length > 0 && !matchesTokens(command, tokens)) {
            return -1
        }

        const title = normalize(command.title)
        const subtitle = normalize(command.subtitle)
        const shortcut = normalize(command.shortcut)
        const keywords = command.keywords && command.keywords.length > 0 ? normalize(command.keywords.join(" ")) : ""
        const q = normalize(queryText)

        if (q.length === 0) return 0
        if (title.indexOf(q) === 0) return 0
        if (title.indexOf(q) >= 0) return 1
        if (shortcut.indexOf(q) >= 0) return 2
        if (keywords.indexOf(q) >= 0) return 3
        if (subtitle.indexOf(q) >= 0) return 4
        return 5
    }

    function isEnabled(command) {
        if (!command) return false
        if (typeof command.enabled !== "function") return true
        try {
            return !!command.enabled()
        } catch (e) {
            return false
        }
    }

    function resultCountText() {
        const count = root.filteredCommands.length
        if (count === 0) {
            return "No matches"
        }
        if (count === 1) {
            return "1 command"
        }
        return count + " commands"
    }

    function refreshResults() {
        const queryText = normalize((typeof searchField !== "undefined" && searchField) ? searchField.text : root.query)
        const tokens = queryText.length > 0 ? queryText.split(/\s+/).filter(Boolean) : []
        const next = []

        for (let i = 0; i < root.commands.length; ++i) {
            const command = root.commands[i]
            if (!isEnabled(command)) {
                continue
            }

            const score = scoreCommand(command, queryText, tokens)
            if (score < 0) {
                continue
            }

            next.push({
                command: command,
                score: score,
                originalIndex: i
            })
        }

        next.sort((a, b) => {
            if (a.score !== b.score) return a.score - b.score
            return a.originalIndex - b.originalIndex
        })

        root.filteredCommands = next
        if (next.length > 0) {
            root.selectedIndex = 0
        } else {
            root.selectedIndex = -1
        }
    }

    function openPalette() {
        query = ""
        if (typeof searchField !== "undefined" && searchField) {
            searchField.text = ""
        }
        refreshResults()
        open()
    }

    function closePalette() {
        close()
    }

    function executeSelected() {
        if (root.selectedIndex < 0 || root.selectedIndex >= root.filteredCommands.length) {
            return
        }
        const entry = root.filteredCommands[root.selectedIndex]
        if (!entry || !entry.command) {
            return
        }

        const command = entry.command
        close()
        Qt.callLater(() => {
            if (typeof command.run === "function") {
                command.run()
            }
        })
    }

    function moveSelection(delta) {
        if (root.filteredCommands.length === 0) {
            root.selectedIndex = -1
            return
        }

        let next = root.selectedIndex + delta
        if (next < 0) next = root.filteredCommands.length - 1
        if (next >= root.filteredCommands.length) next = 0
        root.selectedIndex = next
    }

    onOpened: {
        Qt.callLater(() => searchField.forceActiveFocus())
    }

    onSelectedIndexChanged: {
        if (commandList.currentIndex !== selectedIndex) {
            commandList.currentIndex = selectedIndex
        }
        if (selectedIndex >= 0 && selectedIndex < root.filteredCommands.length) {
            commandList.positionViewAtIndex(selectedIndex, ListView.Contain)
        }
    }

    onVisibleChanged: {
        if (!visible) {
            if (typeof searchField !== "undefined" && searchField) {
                searchField.text = ""
            }
            query = ""
            filteredCommands = []
            selectedIndex = -1
        }
    }

    onCommandsChanged: refreshResults()

    background: Rectangle {
        radius: Theme.panelRadius
        color: Theme.panelSurfaceStrong
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.panelSurface }
            GradientStop { position: 1.0; color: Theme.panelSurfaceStrong }
        }
        border.color: Theme.withAlpha(Theme.accent, 0.18)
        border.width: 1

        Rectangle {
            x: -120
            y: -90
            width: 260
            height: 220
            radius: 130
            rotation: -16
            color: Theme.withAlpha(Theme.accent, 0.08)
            opacity: 0.8
        }

        Rectangle {
            x: parent.width - 150
            y: parent.height - 130
            width: 220
            height: 180
            radius: 110
            rotation: 18
            color: Theme.withAlpha(Theme.accent, 0.05)
            opacity: 0.75
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Theme.withAlpha(Theme.surface, 0.35)
            opacity: 0.65
        }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.glassShadow
            shadowBlur: 28
            shadowVerticalOffset: 12
        }
    }

    contentItem: ColumnLayout {
        spacing: 0
        focus: true

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.withAlpha(Theme.accent, 0.12)
            opacity: 0.9
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.panelHeaderHeight
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                anchors.topMargin: 14
                anchors.bottomMargin: 10
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    radius: Theme.radiusLg
                    color: Theme.withAlpha(Theme.accent, 0.18)
                    border.color: Theme.withAlpha(Theme.accent, 0.30)
                    border.width: 1

                    Image {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: "../assets/lucide-toolbar/search.svg"
                        sourceSize: Qt.size(18, 18)
                        smooth: true
                        opacity: 0.94
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    PremiumTextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: "Type a command or keyword..."

                        onTextEdited: {
                            root.query = text
                            root.refreshResults()
                        }

                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_Escape) {
                                root.closePalette()
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                root.executeSelected()
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Down || event.key === Qt.Key_Tab || event.key === Qt.Key_PageDown) {
                                root.moveSelection(1)
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Up || event.key === Qt.Key_Backtab || event.key === Qt.Key_PageUp) {
                                root.moveSelection(-1)
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Home) {
                                if (root.filteredCommands.length > 0) {
                                    root.selectedIndex = 0
                                }
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_End) {
                                if (root.filteredCommands.length > 0) {
                                    root.selectedIndex = root.filteredCommands.length - 1
                                }
                                event.accepted = true
                                return
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        KeyBadge {
                            text: "Ctrl+K"
                        }

                        Label {
                            text: "Search commands, actions, and shortcuts"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeCaption
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: root.resultCountText()
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeCaption
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.withAlpha(Theme.border, 0.55)
            opacity: 0.7
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 10
            Layout.bottomMargin: 10

            ListView {
                id: commandList
                anchors.fill: parent
                clip: true
                model: root.filteredCommands
                currentIndex: -1
                interactive: contentHeight > height
                highlightFollowsCurrentItem: false

                onCurrentIndexChanged: {
                    if (root.selectedIndex !== currentIndex) {
                        root.selectedIndex = currentIndex
                    }
                }

                delegate: ItemDelegate {
                    width: ListView.view ? ListView.view.width : 0
                    height: 60
                    padding: 0
                    hoverEnabled: true

                    readonly property var commandEntry: modelData
                    readonly property var command: commandEntry ? commandEntry.command : null
                    readonly property bool isCurrent: ListView.view && ListView.view.currentIndex === index

                    onHoveredChanged: {
                        if (hovered && ListView.view) {
                            root.selectedIndex = index
                        }
                    }

                    onClicked: {
                        root.selectedIndex = index
                        root.executeSelected()
                    }

                    background: Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusLg
                        color: isCurrent
                                ? Theme.withAlpha(Theme.accent, 0.16)
                                : (hovered ? Theme.withAlpha(Theme.surfaceHover, 0.92) : "transparent")
                        border.color: isCurrent ? Theme.withAlpha(Theme.accent, 0.80) : "transparent"
                        border.width: isCurrent ? 1 : 0
                    }

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 32
                            radius: 4
                            color: isCurrent ? Theme.accent : Theme.withAlpha(Theme.accent, 0.30)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                text: command ? command.title : ""
                                color: Theme.textPrimary
                                font.pixelSize: 13
                                font.weight: isCurrent ? Font.DemiBold : Font.Medium
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Label {
                                text: command ? command.subtitle : ""
                                color: Theme.textSecondary
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        KeyBadge {
                            id: shortcutLabel
                            visible: command && command.shortcut && command.shortcut.length > 0
                            text: command ? command.shortcut : ""
                            fillColor: Theme.withAlpha(Theme.surface, 0.62)
                            borderColor: Theme.withAlpha(Theme.border, 0.80)
                            textColor: Theme.textSecondary
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    width: 10
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 10
                visible: root.filteredCommands.length === 0

                Rectangle {
                    width: 56
                    height: 56
                    radius: 18
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.withAlpha(Theme.accent, 0.10)
                    border.color: Theme.withAlpha(Theme.accent, 0.18)
                    border.width: 1

                    Image {
                        anchors.centerIn: parent
                        width: 22
                        height: 22
                        source: "../assets/lucide-toolbar/search.svg"
                        sourceSize: Qt.size(22, 22)
                        opacity: 0.86
                    }
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "No commands match the current query"
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Try a shorter keyword or remove a filter token"
                    color: Theme.textSecondary
                    font.pixelSize: 10
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.withAlpha(Theme.border, 0.55)
            opacity: 0.7
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            spacing: 10

            Label {
                text: "Enter to run"
                color: Theme.textSecondary
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 12
                color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.75)
            }

            Label {
                text: "Esc to close"
                color: Theme.textSecondary
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }

            Item { Layout.fillWidth: true }

            Label {
                text: "Filtered commands only"
                color: Theme.textSecondary
                font.pixelSize: 10
                opacity: 0.8
            }
        }
    }
}
