import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    property var commands: []
    property var filteredCommands: []
    property var pendingCommand: null
    property string query: ""
    property int selectedIndex: -1
    property var usageStats: ({ counts: {}, timestamps: {} })

    // Argument-aware command state
    property bool argumentMode: false
    property string argumentText: ""
    property string argumentError: ""
    property var selectedArgumentCommand: null
    property var filteredSuggestions: []

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

    function getDisabledReason(command) {
        if (!command || !command.disabledReason) return ""
        if (typeof command.disabledReason === "function") {
            try {
                return String(command.disabledReason())
            } catch(e) {
                return ""
            }
        }
        return String(command.disabledReason)
    }

    function categoryColor(category) {
        if (!category) return Theme.textSecondary
        const cat = String(category).toLowerCase()
        switch (cat) {
            case "navigation":
                return Theme.categoryNavigation
            case "view":
                return Theme.categoryUtility
            case "file":
                return Theme.categoryAction
            case "inspect":
                return Theme.categoryInfo
            case "theme":
                return Theme.categoryUtility
            case "settings":
            case "admin":
                return Theme.categorySystem
            case "help":
                return Theme.categoryInfo
            default:
                return Theme.textSecondary
        }
    }

    function parseQuery(queryText) {
        const q = normalize(queryText)
        let categoryFilter = ""
        let searchText = q

        const match = q.match(/@(\w+)/)
        if (match) {
            categoryFilter = match[1]
            searchText = q.replace(/@\w+/, "").replace(/\s+/g, " ").trim()
        }
        return { categoryFilter: categoryFilter, searchText: searchText }
    }

    function commandText(command) {
        if (!command) return ""
        const parts = []
        if (command.title) parts.push(command.title)
        if (command.subtitle) parts.push(command.subtitle)
        if (command.shortcut) parts.push(command.shortcut)
        if (command.keywords && command.keywords.length > 0) parts.push(command.keywords.join(" "))
        if (command.aliases && command.aliases.length > 0) parts.push(command.aliases.join(" "))
        if (command.category) parts.push(command.category)
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
        
        const parsed = parseQuery(queryText)
        const categoryFilter = parsed.categoryFilter
        const searchText = parsed.searchText
        
        if (categoryFilter.length > 0) {
            if (!command.category || normalize(command.category) !== categoryFilter) {
                return -1
            }
        }
        
        const searchTokens = searchText.length > 0 ? searchText.split(/\s+/).filter(Boolean) : []
        if (searchTokens.length > 0 && !matchesTokens(command, searchTokens)) {
            return -1
        }
        
        if (searchText.length === 0) return 0
        
        const title = normalize(command.title)
        const subtitle = normalize(command.subtitle)
        const shortcut = normalize(command.shortcut)
        const keywords = command.keywords && command.keywords.length > 0 ? normalize(command.keywords.join(" ")) : ""
        
        if (title.indexOf(searchText) === 0) return 0
        if (title.indexOf(searchText) >= 0) return 1
        
        let aliasMatch = false
        if (command.aliases) {
            for (let i = 0; i < command.aliases.length; ++i) {
                if (normalize(command.aliases[i]).indexOf(searchText) >= 0) {
                    aliasMatch = true
                    break
                }
            }
        }
        if (aliasMatch) return 2
        
        if (keywords.indexOf(searchText) >= 0) return 3
        if (shortcut.indexOf(searchText) >= 0) return 4
        if (subtitle.indexOf(searchText) >= 0) return 5
        return 6
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
        const parsed = parseQuery(queryText)
        const searchText = parsed.searchText
        const tokens = searchText.length > 0 ? searchText.split(/\s+/).filter(Boolean) : []
        const next = []

        for (let i = 0; i < root.commands.length; ++i) {
            const command = root.commands[i]
            const enabled = isEnabled(command)
            if (!enabled && queryText.length === 0) {
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
            const enabledA = isEnabled(a.command)
            const enabledB = isEnabled(b.command)
            if (enabledA !== enabledB) {
                return enabledA ? -1 : 1
            }

            if (a.score !== b.score) return a.score - b.score

            const timeA = (usageStats && usageStats.timestamps) ? (usageStats.timestamps[a.command.id] || 0) : 0
            const timeB = (usageStats && usageStats.timestamps) ? (usageStats.timestamps[b.command.id] || 0) : 0
            if (timeA !== timeB) {
                return timeB - timeA
            }

            const countA = (usageStats && usageStats.counts) ? (usageStats.counts[a.command.id] || 0) : 0
            const countB = (usageStats && usageStats.counts) ? (usageStats.counts[b.command.id] || 0) : 0
            if (countA !== countB) {
                return countB - countA
            }

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
        argumentError = ""
        if (typeof searchField !== "undefined" && searchField) {
            searchField.text = ""
        }
        if (typeof appSettings !== "undefined" && appSettings) {
            usageStats = appSettings.commandUsageStats()
        }
        refreshResults()
        open()
    }

    function beginArgumentCommand(command) {
        if (!command || !command.acceptsArgument || !isEnabled(command)) {
            return false
        }
        root.selectedArgumentCommand = command
        root.argumentText = ""
        root.argumentError = ""
        root.argumentMode = true
        root.refreshSuggestions()
        open()
        Qt.callLater(() => {
            if (typeof argumentField !== "undefined" && argumentField) {
                argumentField.forceActiveFocus()
            }
        })
        return true
    }

    function openCommandArgument(commandId) {
        query = ""
        argumentError = ""
        if (typeof searchField !== "undefined" && searchField) {
            searchField.text = ""
        }
        if (typeof appSettings !== "undefined" && appSettings) {
            usageStats = appSettings.commandUsageStats()
        }
        for (let i = 0; i < root.commands.length; ++i) {
            const command = root.commands[i]
            if (command && command.id === commandId) {
                if (root.beginArgumentCommand(command)) {
                    return
                }
                break
            }
        }
        root.openPalette()
    }

    function closePalette() {
        close()
    }

    function executeSelected() {
        if (root.argumentMode) {
            if (!root.selectedArgumentCommand) return
            
            let arg = root.argumentText.trim()
            if (root.selectedIndex >= 0 && root.selectedIndex < root.filteredSuggestions.length) {
                const sugg = root.filteredSuggestions[root.selectedIndex]
                if (sugg && sugg.value !== undefined) {
                    arg = sugg.value
                }
            }

            if (typeof root.selectedArgumentCommand.validateArgument === "function") {
                const validationError = String(root.selectedArgumentCommand.validateArgument(arg) || "")
                if (validationError.length > 0) {
                    root.argumentError = validationError
                    if (typeof argumentField !== "undefined" && argumentField) {
                        argumentField.forceActiveFocus()
                        argumentField.selectAll()
                    }
                    return
                }
            }
            
            const argCmd = root.selectedArgumentCommand
            if (typeof appSettings !== "undefined" && appSettings) {
                appSettings.recordCommandExecuted(argCmd.id)
            }
            
            root.argumentMode = false
            root.selectedArgumentCommand = null
            root.argumentText = ""
            root.argumentError = ""
            root.pendingCommand = null
            close()
            
            if (typeof argCmd.runWithArgument === "function") {
                Qt.callLater(() => argCmd.runWithArgument(arg))
            }
            return
        }

        if (root.selectedIndex < 0 || root.selectedIndex >= root.filteredCommands.length) {
            return
        }
        const entry = root.filteredCommands[root.selectedIndex]
        if (!entry || !entry.command) {
            return
        }
        if (!isEnabled(entry.command)) {
            return
        }

        if (entry.command.acceptsArgument) {
            root.beginArgumentCommand(entry.command)
            return
        }

        root.pendingCommand = entry.command
        if (typeof appSettings !== "undefined" && appSettings) {
            appSettings.recordCommandExecuted(entry.command.id)
        }
        close()
    }

    function moveSelection(delta) {
        const count = root.argumentMode ? root.filteredSuggestions.length : root.filteredCommands.length
        if (count === 0) {
            root.selectedIndex = -1
            return
        }

        let next = root.selectedIndex + delta
        if (next < 0) next = count - 1
        if (next >= count) next = 0
        root.selectedIndex = next
    }

    function refreshSuggestions() {
        if (!root.selectedArgumentCommand) {
            root.filteredSuggestions = []
            root.selectedIndex = -1
            return
        }

        const cmd = root.selectedArgumentCommand
        const text = root.argumentText
        let list = []
        if (typeof cmd.getSuggestions === "function") {
            try {
                list = cmd.getSuggestions(text)
            } catch (e) {
                console.log("Error getting dynamic suggestions: " + e)
            }
        } else if (Array.isArray(cmd.suggestions)) {
            const term = text.toLowerCase().trim()
            for (let i = 0; i < cmd.suggestions.length; ++i) {
                const sugg = cmd.suggestions[i]
                if (term.length === 0 || 
                    sugg.title.toLowerCase().indexOf(term) >= 0 || 
                    (sugg.subtitle && sugg.subtitle.toLowerCase().indexOf(term) >= 0)) {
                    list.push(sugg)
                }
            }
        }

        const processed = []
        for (let j = 0; j < list.length; ++j) {
            const item = list[j]
            processed.push({
                isSuggestion: true,
                title: item.title || item.label || item.value || "",
                subtitle: item.subtitle || "",
                value: item.value || "",
                previewColor: item.previewColor || "",
                category: item.category || ""
            })
        }

        root.filteredSuggestions = processed
        if (processed.length > 0) {
            root.selectedIndex = 0
        } else {
            root.selectedIndex = -1
        }
    }

    onArgumentTextChanged: {
        argumentError = ""
        if (root.argumentMode) {
            root.refreshSuggestions()
        }
    }

    onArgumentModeChanged: {
        if (argumentMode) {
            refreshSuggestions()
        } else {
            filteredSuggestions = []
            argumentError = ""
        }
    }

    onOpened: {
        Qt.callLater(() => {
            if (root.argumentMode && typeof argumentField !== "undefined" && argumentField) {
                argumentField.forceActiveFocus()
            } else {
                searchField.forceActiveFocus()
            }
        })
    }

    onSelectedIndexChanged: {
        if (commandList.currentIndex !== selectedIndex) {
            commandList.currentIndex = selectedIndex
        }
        const count = root.argumentMode ? root.filteredSuggestions.length : root.filteredCommands.length
        if (selectedIndex >= 0 && selectedIndex < count) {
            commandList.positionViewAtIndex(selectedIndex, ListView.Contain)
        }
    }

    onVisibleChanged: {
        if (!visible) {
            if (typeof searchField !== "undefined" && searchField) {
                searchField.text = ""
            }
            if (typeof argumentField !== "undefined" && argumentField) {
                argumentField.text = ""
            }
            query = ""
            filteredCommands = []
            filteredSuggestions = []
            selectedIndex = -1
            argumentMode = false
            argumentText = ""
            argumentError = ""
            selectedArgumentCommand = null
        }
    }

    onClosed: {
        const command = root.pendingCommand
        root.pendingCommand = null
        if (command && typeof command.run === "function") {
            Qt.callLater(() => command.run())
        }
        root.argumentMode = false
        root.selectedArgumentCommand = null
        root.argumentText = ""
        root.argumentError = ""
        root.filteredSuggestions = []
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
                        source: root.argumentMode ? "../assets/lucide-toolbar/arrow-right.svg" : "../assets/lucide-toolbar/search.svg"
                        sourceSize: Qt.size(18, 18)
                        smooth: true
                        opacity: 0.94
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controlHeight

                        PremiumTextField {
                            id: searchField
                            anchors.fill: parent
                            placeholderText: "Type a command or keyword..."
                            visible: opacity > 0.01
                            opacity: root.argumentMode ? 0.0 : 1.0
                            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }

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
                                    const entry = root.selectedIndex >= 0 && root.selectedIndex < root.filteredCommands.length
                                                 ? root.filteredCommands[root.selectedIndex] : null
                                    if (event.key === Qt.Key_Tab && entry && entry.command && entry.command.acceptsArgument) {
                                        root.executeSelected()
                                    } else {
                                        root.moveSelection(1)
                                    }
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

                        PremiumTextField {
                            id: argumentField
                            anchors.fill: parent
                            placeholderText: root.selectedArgumentCommand && root.selectedArgumentCommand.argumentLabel
                                             ? root.selectedArgumentCommand.argumentLabel
                                             : "Enter argument..."
                            text: root.argumentText
                            visible: opacity > 0.01
                            opacity: root.argumentMode ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }

                            onTextEdited: {
                                root.argumentText = text
                            }

                            Keys.onPressed: (event) => {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                    root.executeSelected()
                                    event.accepted = true
                                    return
                                }
                                if (event.key === Qt.Key_Escape) {
                                    root.argumentMode = false
                                    root.selectedArgumentCommand = null
                                    root.argumentText = ""
                                    Qt.callLater(() => searchField.forceActiveFocus())
                                    event.accepted = true
                                    return
                                }
                                if (event.key === Qt.Key_Backtab) {
                                    root.argumentMode = false
                                    root.selectedArgumentCommand = null
                                    root.argumentText = ""
                                    Qt.callLater(() => searchField.forceActiveFocus())
                                    event.accepted = true
                                    return
                                }
                                if (event.key === Qt.Key_Down || event.key === Qt.Key_PageDown) {
                                    root.moveSelection(1)
                                    event.accepted = true
                                    return
                                }
                                if (event.key === Qt.Key_Up || event.key === Qt.Key_PageUp) {
                                    root.moveSelection(-1)
                                    event.accepted = true
                                    return
                                }
                                if (event.key === Qt.Key_Tab) {
                                    if (root.selectedIndex >= 0 && root.selectedIndex < root.filteredSuggestions.length) {
                                        const sugg = root.filteredSuggestions[root.selectedIndex]
                                        if (sugg && sugg.value !== undefined) {
                                            argumentField.text = sugg.value
                                            root.argumentText = sugg.value
                                            argumentField.cursorPosition = argumentField.text.length
                                        }
                                    }
                                    event.accepted = true
                                    return
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: !root.argumentMode

                        KeyBadge {
                            text: "Ctrl+K"
                        }

                        Label {
                            text: root.query.length === 0
                                  ? "Recent and frequent commands first; type @file, @inspect, @theme to filter"
                                  : "Search commands, aliases, categories, and shortcuts"
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

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.argumentMode

                        KeyBadge {
                            text: "Arg"
                            textColor: Theme.categoryUtility
                            fillColor: Theme.withAlpha(Theme.categoryUtility, 0.15)
                        }

                        Label {
                            text: root.selectedArgumentCommand ? "Run: " + root.selectedArgumentCommand.title : ""
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeCaption
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: root.filteredSuggestions.length > 0 
                                  ? (root.filteredSuggestions.length === 1 ? "1 suggestion" : root.filteredSuggestions.length + " suggestions")
                                  : "Type custom and Enter"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeCaption
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: argumentErrorLabel.implicitHeight + 10
                        radius: Theme.radiusSm
                        visible: root.argumentMode && root.argumentError.length > 0
                        color: Theme.withAlpha(Theme.warning, themeController.isDark ? 0.16 : 0.10)
                        border.color: Theme.withAlpha(Theme.warning, 0.42)
                        border.width: 1

                        Label {
                            id: argumentErrorLabel
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            verticalAlignment: Text.AlignVCenter
                            text: root.argumentError
                            color: Theme.warning
                            font.pixelSize: Theme.fontSizeCaption
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
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
                model: root.argumentMode ? root.filteredSuggestions : root.filteredCommands
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

                    readonly property bool isSuggestion: modelData && modelData.isSuggestion ? true : false
                    readonly property var commandEntry: modelData
                    readonly property var command: !isSuggestion && commandEntry ? commandEntry.command : null
                    readonly property bool isCurrent: ListView.view && ListView.view.currentIndex === index
                    readonly property bool isEnabled: isSuggestion ? true : root.isEnabled(command)

                    readonly property string titleText: isSuggestion ? (modelData.title || "") : (command && command.title || "")
                    readonly property string subtitleText: isSuggestion ? (modelData.subtitle || "") : (command && command.subtitle || "")
                    readonly property string categoryText: isSuggestion ? (modelData.category || "") : (command && command.category || "")
                    readonly property string shortcutText: isSuggestion ? "" : (command && command.shortcut || "")

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
                        opacity: isEnabled ? 1.0 : 0.45

                        Rectangle {
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 32
                            radius: 4
                            color: !isEnabled ? Theme.withAlpha(Theme.textSecondary, 0.2)
                                   : (isSuggestion && modelData.previewColor ? modelData.previewColor
                                      : (command && command.danger ? Theme.danger
                                         : (isCurrent ? Theme.accent : Theme.withAlpha(Theme.accent, 0.30))))
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                text: titleText
                                color: isEnabled ? Theme.textPrimary : Theme.textSecondary
                                font.pixelSize: 13
                                font.weight: isCurrent ? Font.DemiBold : Font.Medium
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Label {
                                text: {
                                    if (isSuggestion) return subtitleText
                                    if (!command) return ""
                                    if (!isEnabled) {
                                        const reason = root.getDisabledReason(command)
                                        return reason ? ("Unavailable: " + reason) : (subtitleText || "")
                                    }
                                    return subtitleText || ""
                                }
                                color: !isEnabled ? Theme.warning : Theme.textSecondary
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        KeyBadge {
                            visible: categoryText.length > 0
                            text: categoryText
                            textColor: root.categoryColor(categoryText)
                            fillColor: Theme.withAlpha(textColor, themeController.isDark ? 0.16 : 0.12)
                            borderColor: Theme.withAlpha(textColor, themeController.isDark ? 0.28 : 0.20)
                            Layout.alignment: Qt.AlignVCenter
                        }

                        KeyBadge {
                            id: shortcutLabel
                            visible: shortcutText.length > 0
                            text: shortcutText
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
                visible: root.argumentMode ? root.filteredSuggestions.length === 0 : root.filteredCommands.length === 0

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
                        source: root.argumentMode ? "../assets/lucide-toolbar/arrow-right.svg" : "../assets/lucide-toolbar/search.svg"
                        sourceSize: Qt.size(22, 22)
                        opacity: 0.86
                    }
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.argumentMode ? "No suggestions available" : "No commands match the current query"
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.argumentMode ? "Type a custom value and press Enter to run" : "Try a shorter keyword or remove a filter token"
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
                color: Theme.withAlpha(Theme.border, 0.75)
            }

            Label {
                text: "Esc to close"
                color: Theme.textSecondary
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }

            Item { Layout.fillWidth: true }

            Label {
                text: "@category filters: @file @view @inspect @settings"
                color: Theme.textSecondary
                font.pixelSize: 10
                opacity: 0.8
            }
        }
    }
}
