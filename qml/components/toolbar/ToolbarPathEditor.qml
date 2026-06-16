import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import ".."
import "../../style"

Item {
    id: root

    property var controller
    property var workspaceController
    property string activePath: ""
    property alias pathEditorField: pathEditor
    property bool pathEditing: false
    property string pathEditError: ""
    property real pathEditProgress: 0.0
    property bool ignoreTextChange: false
    property int suggestionRequestId: 0
    property bool suggestionsLoading: false
    property bool applyFirstSuggestionOnReady: false
    property bool pendingSuggestionAllowTrailingSeparator: false
    property var openPathHandler: null
    property var prepareNavigationHandler: null
    readonly property bool autocompleteUnavailable: (root.pathEditing || root.pathEditProgress > 0.0)
                                                   && root.pathEditError.length === 0
                                                   && !root.localAutocompleteAvailable(pathEditor.text)
    readonly property bool autocompleteHintVisible: (root.pathEditing || root.pathEditProgress > 0.0)
                                                   && root.pathEditError.length === 0
                                                   && (root.autocompleteUnavailable || suggestionsPopup.visible || root.suggestionsLoading)
    signal pathAccepted()
    signal pathCancelled()

    Layout.fillWidth: true
    Layout.preferredHeight: 40

    Behavior on pathEditProgress {
        NumberAnimation {
            duration: 150
            easing.type: Easing.InOutCubic
        }
    }

    function focusPath() {
        root.pathEditError = ""
        const displayValue = root.displayPath(root.activePath)
        pathEditor.compactSuggestionTextActive = false
        pathEditor.text = displayValue
        pathEditor.originalText = displayValue
        root.pathEditing = true
        pathEditor.forceActiveFocus()
        pathEditor.selectAll()
        root.pathEditProgress = 1.0
    }

    function displayPath(path) {
        if (!path || String(path).length === 0) {
            return ""
        }
        if (root.workspaceController && root.workspaceController.displayPath) {
            return root.workspaceController.displayPath(String(path))
        }
        const value = String(path)
        if (value.indexOf("://") > 0) {
            return value
        }
        return Qt.platform.os === "windows" ? value.replace(/\//g, "\\") : value
    }

    function openPath(path) {
        if (root.openPathHandler) {
            return root.openPathHandler(path)
        }
        return root.controller ? root.controller.openPath(path) : false
    }

    function prepareNavigation(reason) {
        if (root.prepareNavigationHandler) {
            root.prepareNavigationHandler(reason)
        }
    }

    function hasExplicitNonLocalScheme(path) {
        const value = String(path || "").trim().toLowerCase()
        const schemeIndex = value.indexOf("://")
        return schemeIndex > 0 && value.indexOf("file://") !== 0
    }

    function localAutocompleteAvailable(path) {
        if (!root.controller || !root.controller.pathKindFor) {
            return true
        }
        const currentKind = root.controller.currentPath
                            ? root.controller.pathKindFor(root.controller.currentPath)
                            : "local"
        if (currentKind !== "local") {
            return false
        }
        return !root.hasExplicitNonLocalScheme(path)
    }

    function acceptPathEdit() {
        pathEditor.compactSuggestionTextActive = false
        pathEditor.cancelPendingSuggestions()
        root.suggestionRequestId += 1
        root.suggestionsLoading = false
        const path = pathEditor.text.trim()
        if (path.length > 0) {
            if (root.openPath(path)) {
                root.pathEditError = ""
                suggestionsPopup.close()
                root.pathEditing = false
                root.pathEditProgress = 0.0
                if (root.workspaceController) {
                    root.workspaceController.focusActivePanel()
                }
                root.pathAccepted()
                return
            }
            root.pathEditError = "Path not found"
        } else {
            root.pathEditError = "Enter a valid path"
        }
        pathEditor.forceActiveFocus()
        pathEditor.selectAll()
    }

    function cancelPathEdit() {
        root.pathEditError = ""
        pathEditor.compactSuggestionTextActive = false
        suggestionsPopup.close()
        if (root.pathEditing || root.pathEditProgress > 0.0) {
            root.pathEditing = false
            root.pathEditProgress = 0.0
        }
        if (root.workspaceController) {
            root.workspaceController.focusActivePanel()
        }
        root.pathCancelled()
    }

    onPathEditingChanged: {
        if (!root.pathEditing) {
            pathEditor.cancelPendingSuggestions()
            root.suggestionRequestId += 1
            root.suggestionsLoading = false
            suggestionsPopup.close()
        }
    }

    Rectangle {
        id: pathIsland
        anchors.centerIn: parent
        width: parent.width - 8
        height: 40
        radius: Theme.controlRadius
        readonly property real autocompleteHintWidth: root.autocompleteUnavailable ? Math.min(292, Math.max(210, width - 110)) : 128

        color: root.pathEditing
               ? Theme.panelSurfaceStrong
               : (islandHover.hovered
                  ? Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.92 : 0.98)
                  : Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.74 : 0.90))

        border.color: root.pathEditing
                      ? Theme.withAlpha(root.pathEditError.length > 0 ? Theme.danger : Theme.focusRing,
                                        themeController.isDark ? 0.86 : 0.76)
                      : (islandHover.hovered
                         ? Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.58 : 0.50)
                         : Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.42 : 0.36))
        border.width: 1

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        HoverHandler {
            id: islandHover
        }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.glassShadow
            shadowBlur: 10 + (root.pathEditProgress * 4)
            shadowVerticalOffset: 2 + (root.pathEditProgress * 2)
        }

        Rectangle {
            id: editGlow
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            visible: root.pathEditing
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.withAlpha(Theme.categoryInfo, 0.14) }
                GradientStop { position: 0.42; color: "transparent" }
                GradientStop { position: 1.0; color: Theme.withAlpha(Theme.warmAccent, 0.06) }
            }
        }

        Rectangle {
            id: inlineProviderKind
            anchors.left: parent.left
            anchors.leftMargin: 16 + (10 * root.pathEditProgress)
            anchors.verticalCenter: parent.verticalCenter
            width: 24
            height: 24
            radius: Theme.radiusForSide(height)
            visible: root.pathEditing || root.pathEditProgress > 0.0
            color: Theme.withAlpha(kindColor, themeController.isDark ? 0.16 : 0.11)
            border.color: Theme.withAlpha(kindColor, themeController.isDark ? 0.40 : 0.30)
            border.width: 1

            readonly property string kindValue: {
                if (root.controller && root.controller.pathKindFor) {
                    return root.controller.pathKindFor(root.controller.currentPath)
                }
                return "local"
            }
            readonly property color kindColor: {
                if (kindValue === "archive") {
                    return Theme.warmAccent
                }
                if (kindValue === "devices") {
                    return Theme.categorySystem
                }
                if (kindValue === "favorites") {
                    return Theme.actionIconColor("favorite")
                }
                if (kindValue === "ftp") {
                    return Theme.categoryNavigation
                }
                if (kindValue === "remote") {
                    return Theme.categoryNavigation
                }
                return Theme.categoryInfo
            }
            readonly property string iconSource: {
                if (kindValue === "archive") {
                    return "../../assets/icons/archive.svg"
                }
                if (kindValue === "devices") {
                    return "../../assets/icons/hard-drive.svg"
                }
                if (kindValue === "favorites") {
                    return "../../assets/icons/star.svg"
                }
                if (kindValue === "ftp") {
                    return "../../assets/icons/ftp.svg"
                }
                if (kindValue === "remote") {
                    return "../../assets/icons/computer.svg"
                }
                return "../../assets/icons/folder.svg"
            }
            readonly property string tooltipText: {
                if (kindValue === "archive") return "Archive"
                if (kindValue === "devices") return "This PC"
                if (kindValue === "favorites") return "Favorites"
                if (kindValue === "ftp") return "FTP"
                if (kindValue === "remote") return "Remote provider"
                return "Local folder"
            }
            opacity: 0.92

            HoverHandler {
                id: providerKindHover
            }

            ToolTip.visible: providerKindHover.hovered
            ToolTip.delay: 450
            ToolTip.text: inlineProviderKind.tooltipText

            Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                source: inlineProviderKind.iconSource
                sourceSize: Qt.size(28, 28)
                layer.enabled: true
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: inlineProviderKind.kindColor
                }
            }
        }

        PathBar {
            id: pathBar
            anchors.fill: parent
            anchors.leftMargin: 8 + (10 * root.pathEditProgress)
            anchors.rightMargin: 8
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            controller: root.controller
            path: root.activePath
            readOnly: false
            openPathHandler: function(path) { return root.openPath(path) }
            prepareNavigationHandler: function(reason) { root.prepareNavigation(reason) }
            onEditRequested: root.focusPath()
            opacity: 1.0 - root.pathEditProgress
            visible: root.pathEditProgress < 0.99
            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
        }

        PremiumTextField {
            id: pathEditor
            property string originalText: ""
            property bool compactSuggestionTextActive: false
            anchors.fill: parent
            anchors.leftMargin: 58 + (18 * root.pathEditProgress)
            anchors.rightMargin: root.autocompleteHintVisible ? pathIsland.autocompleteHintWidth + 20 : 42
            opacity: root.pathEditProgress
            visible: root.pathEditing || root.pathEditProgress > 0.01
            text: root.displayPath(root.activePath)
            placeholderText: "Type folder path..."
            background: null
            leftPadding: 0
            rightPadding: 0
            font.family: "Cascadia Code, Consolas, monospace"
            font.pixelSize: 13
            font.weight: Font.Medium
            font.letterSpacing: -0.15
            selectByMouse: true

            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.72)
            color: compactSuggestionTextActive ? "transparent" : Theme.textPrimary
            selectionColor: Theme.withAlpha(Theme.categoryInfo, 0.30)
            selectedTextColor: Theme.textPrimary
            cursorDelegate: Rectangle {
                width: 2
                radius: 1
                color: root.pathEditError.length > 0 ? Theme.danger : Theme.categoryInfo
            }

            onActiveFocusChanged: {
                if (!activeFocus && root.pathEditing) {
                    Qt.callLater(() => {
                        if (root.pathEditing && !pathEditor.activeFocus) {
                            root.cancelPathEdit()
                        }
                    })
                }
            }

            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
            Behavior on anchors.leftMargin { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
            Behavior on anchors.rightMargin { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

            Label {
                anchors.fill: parent
                visible: pathEditor.compactSuggestionTextActive && pathEditor.text.length > 0
                text: pathEditor.text
                color: Theme.textPrimary
                font.family: pathEditor.font.family
                font.pixelSize: pathEditor.font.pixelSize
                font.weight: pathEditor.font.weight
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideMiddle
                clip: true
            }

            onTextChanged: {
                if (root.pathEditing && activeFocus && !root.ignoreTextChange) {
                    compactSuggestionTextActive = false
                    originalText = text
                    updateSuggestions(false)
                }
            }

            Timer {
                id: suggestionRequestTimer
                interval: 90
                repeat: false
                onTriggered: pathEditor.requestSuggestionsNow(root.suggestionRequestId, root.pendingSuggestionAllowTrailingSeparator)
            }

            function cancelPendingSuggestions() {
                suggestionRequestTimer.stop()
                root.applyFirstSuggestionOnReady = false
                if (root.controller && root.controller.cancelDirectorySuggestions) {
                    root.controller.cancelDirectorySuggestions()
                }
            }

            function requestSuggestionsNow(requestId, allowTrailingSeparator) {
                const text = pathEditor.text.trim()
                if (requestId !== root.suggestionRequestId || !root.pathEditing || !root.controller) {
                    return
                }
                if (!root.localAutocompleteAvailable(text)) {
                    root.suggestionsLoading = false
                    root.applyFirstSuggestionOnReady = false
                    suggestionsPopup.close()
                    return
                }
                if ((text.endsWith("/") || text.endsWith("\\")) && allowTrailingSeparator !== true) {
                    root.suggestionsLoading = false
                    root.applyFirstSuggestionOnReady = false
                    suggestionsPopup.close()
                    return
                }
                if (text.length === 0) {
                    root.suggestionsLoading = false
                    root.applyFirstSuggestionOnReady = false
                    suggestionsPopup.close()
                    return
                }
                root.controller.requestDirectorySuggestionEntries(text, requestId, 160)
            }

            function applySuggestedPath(path) {
                root.ignoreTextChange = true
                pathEditor.text = path
                pathEditor.cursorPosition = path.length
                pathEditor.originalText = path
                pathEditor.compactSuggestionTextActive = true
                root.ignoreTextChange = false
            }

            function updateSuggestions(allowTrailingSeparator, immediate) {
                suggestionsModel.clear()
                root.applyFirstSuggestionOnReady = false
                const text = pathEditor.text.trim()
                root.suggestionRequestId += 1
                root.pendingSuggestionAllowTrailingSeparator = allowTrailingSeparator === true
                suggestionRequestTimer.stop()
                if (root.controller && root.controller.cancelDirectorySuggestions) {
                    root.controller.cancelDirectorySuggestions()
                }
                if (!root.localAutocompleteAvailable(text)) {
                    root.suggestionsLoading = false
                    suggestionsPopup.close()
                    return
                }
                if ((text.endsWith("/") || text.endsWith("\\")) && allowTrailingSeparator !== true) {
                    root.suggestionsLoading = false
                    suggestionsPopup.close()
                    return
                }
                if (text.length > 0 && root.controller) {
                    root.suggestionsLoading = true
                    suggestionsList.currentIndex = -1
                    suggestionsPopup.open()
                    if (immediate === true) {
                        requestSuggestionsNow(root.suggestionRequestId, root.pendingSuggestionAllowTrailingSeparator)
                    } else {
                        suggestionRequestTimer.restart()
                    }
                } else {
                    root.suggestionsLoading = false
                    suggestionsPopup.close()
                }
            }

            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    if (suggestionsPopup.visible) {
                        pathEditor.cancelPendingSuggestions()
                        suggestionsPopup.close()
                    }
                    root.acceptPathEdit()
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    if (suggestionsPopup.visible) {
                        pathEditor.cancelPendingSuggestions()
                        root.suggestionRequestId += 1
                        root.suggestionsLoading = false
                        suggestionsPopup.close()
                    } else {
                        root.cancelPathEdit()
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Down && suggestionsPopup.visible) {
                    let nextIndex = suggestionsList.currentIndex + 1
                    if (nextIndex >= suggestionsModel.count) nextIndex = -1
                    suggestionsList.currentIndex = nextIndex

                    if (nextIndex === -1) {
                        root.ignoreTextChange = true
                        pathEditor.text = pathEditor.originalText
                        pathEditor.compactSuggestionTextActive = false
                        pathEditor.cursorPosition = pathEditor.text.length
                        root.ignoreTextChange = false
                    } else {
                        pathEditor.applySuggestedPath(suggestionsModel.get(nextIndex).path)
                    }

                    event.accepted = true
                } else if (event.key === Qt.Key_Up && suggestionsPopup.visible) {
                    let nextIndex = suggestionsList.currentIndex - 1
                    if (nextIndex < -1) nextIndex = suggestionsModel.count - 1
                    suggestionsList.currentIndex = nextIndex

                    if (nextIndex === -1) {
                        root.ignoreTextChange = true
                        pathEditor.text = pathEditor.originalText
                        pathEditor.compactSuggestionTextActive = false
                        pathEditor.cursorPosition = pathEditor.text.length
                        root.ignoreTextChange = false
                    } else {
                        pathEditor.applySuggestedPath(suggestionsModel.get(nextIndex).path)
                    }

                    event.accepted = true
                } else if (event.key === Qt.Key_Tab) {
                    if (root.autocompleteUnavailable) {
                        event.accepted = true
                    } else if (suggestionsPopup.visible && suggestionsModel.count > 0) {
                        let index = suggestionsList.currentIndex >= 0 ? suggestionsList.currentIndex : 0
                        let selectedPath = suggestionsModel.get(index).path

                        pathEditor.applySuggestedPath(selectedPath)

                        suggestionsPopup.close()
                        event.accepted = true
                    } else if (pathEditor.text.trim().length > 0) {
                        updateSuggestions(pathEditor.text.trim().endsWith("/") || pathEditor.text.trim().endsWith("\\"), true)
                        root.applyFirstSuggestionOnReady = true
                        event.accepted = true
                    }
                }
            }
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            visible: root.autocompleteHintVisible
            width: pathIsland.autocompleteHintWidth
            height: 22
            radius: Theme.radiusForSide(height)
            color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.14 : 0.10)
            border.color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.42 : 0.34)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                BusyIndicator {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    running: root.suggestionsLoading
                    visible: root.suggestionsLoading && !root.autocompleteUnavailable
                }

                Rectangle {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    radius: Theme.radiusForSide(Math.min(width, height))
                    color: Theme.categoryInfo
                    visible: !root.suggestionsLoading && !root.autocompleteUnavailable

                    Label {
                        anchors.centerIn: parent
                        text: "Tab"
                        color: Theme.accentText
                        font.pixelSize: 8
                        font.weight: Font.Bold
                    }
                }

                Label {
                    text: root.autocompleteUnavailable
                          ? "autocomplete unavailable for remote providers"
                          : (root.suggestionsLoading ? "loading" : "autocomplete")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        Label {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: root.pathEditError
            visible: opacity > 0
            opacity: (root.pathEditError.length > 0 && (root.pathEditing || root.pathEditProgress > 0.0)) ? 1.0 : 0.0
            color: Theme.danger
            font.pixelSize: Theme.fontSizeCaption
            font.weight: Font.Medium

            background: Rectangle {
                color: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.15 : 0.10)
                border.color: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.30 : 0.40)
                border.width: 1
                radius: Theme.radiusForSide(parent ? parent.height : 24)
            }
            padding: 3
            leftPadding: 10
            rightPadding: 10
            topPadding: 4
            bottomPadding: 4

            Behavior on opacity { NumberAnimation { duration: 150 } }
        }
    }

    Popup {
        id: suggestionsPopup
        property var toolbarRoot: root
        x: pathIsland.x
        y: pathIsland.y + pathIsland.height + 4
        width: pathIsland.width
        height: root.suggestionsLoading && suggestionsModel.count === 0
                ? 64
                : Math.min(suggestionsList.contentHeight + 10, 200)
        padding: 5
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        onClosed: {
            pathEditor.cancelPendingSuggestions()
            if (root.suggestionsLoading) {
                root.suggestionRequestId += 1
                root.suggestionsLoading = false
            }
        }

        background: Rectangle {
            color: Theme.glassSurfaceStrong
            border.color: Theme.withAlpha(Theme.border, 0.85)
            border.width: 1
            radius: Theme.controlRadius

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Theme.glassShadow
                shadowBlur: 10
                shadowVerticalOffset: 4
            }
        }

        contentItem: Item {
            ListView {
                id: suggestionsList
                property var popup: suggestionsPopup
                property var editor: root.pathEditorField
                anchors.fill: parent
                model: ListModel { id: suggestionsModel }
                clip: true
                visible: suggestionsModel.count > 0

                delegate: ItemDelegate {
                    width: ListView.view ? ListView.view.width : 0
                    height: 32
                    hoverEnabled: true

                    onHoveredChanged: {
                        if (hovered && ListView.view) {
                            ListView.view.currentIndex = index
                        }
                    }

                    background: Rectangle {
                        color: (ListView.view && ListView.view.currentIndex === index)
                               ? Theme.itemHoverFill
                               : "transparent"
                        radius: Theme.radiusForSide(height)
                    }

                    contentItem: RowLayout {
                        spacing: 8
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        Image {
                            source: "../../assets/icons/folder.svg"
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                            sourceSize: Qt.size(28, 28)
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                colorization: 1.0
                                colorizationColor: Theme.textSecondary
                            }
                        }

                        Label {
                            text: model.path
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            font.family: "Consolas"
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                        }
                    }

                    onClicked: {
                        let view = ListView.view
                        if (view) {
                            let ed = view.editor
                            if (ed) {
                                let selectedPath = view.model.get(index).path
                                ed.applySuggestedPath(selectedPath)
                            }
                            if (view.popup && view.popup.toolbarRoot) {
                                view.popup.toolbarRoot.acceptPathEdit()
                            }
                        }
                    }
                }
            }

            RowLayout {
                anchors.centerIn: parent
                visible: root.suggestionsLoading && suggestionsModel.count === 0
                spacing: 8

                BusyIndicator {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    running: visible
                }

                Label {
                    text: "Loading folders..."
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
            }
        }
    }

    Connections {
        target: root.controller ? root.controller : null
        function onDirectorySuggestionEntriesReady(requestId, suggestions) {
            if (requestId !== root.suggestionRequestId || !root.pathEditing) {
                return
            }

            root.suggestionsLoading = false
            suggestionsModel.clear()

            if (suggestions.length > 0) {
                for (let i = 0; i < suggestions.length; ++i) {
                    const suggestion = suggestions[i]
                    const path = suggestion && suggestion.path !== undefined ? String(suggestion.path || "") : ""
                    if (path.length > 0) {
                        suggestionsModel.append({
                            "path": path,
                            "label": suggestion && suggestion.label !== undefined ? String(suggestion.label || "") : ""
                        })
                    }
                }
            }

            if (suggestionsModel.count > 0) {
                suggestionsList.currentIndex = -1
                if (root.applyFirstSuggestionOnReady) {
                    root.applyFirstSuggestionOnReady = false
                    pathEditor.applySuggestedPath(suggestionsModel.get(0).path)
                    suggestionsPopup.close()
                } else {
                    suggestionsPopup.open()
                }
            } else {
                root.applyFirstSuggestionOnReady = false
                suggestionsPopup.close()
            }
        }
    }

    SequentialAnimation {
        id: shakeAnimation
        loops: 1

        NumberAnimation { target: pathIsland; property: "anchors.horizontalCenterOffset"; to: -8; duration: 50; easing.type: Easing.OutQuad }
        NumberAnimation { target: pathIsland; property: "anchors.horizontalCenterOffset"; to: 8; duration: 50; easing.type: Easing.InOutQuad }
        NumberAnimation { target: pathIsland; property: "anchors.horizontalCenterOffset"; to: -5; duration: 50; easing.type: Easing.InOutQuad }
        NumberAnimation { target: pathIsland; property: "anchors.horizontalCenterOffset"; to: 5; duration: 50; easing.type: Easing.InOutQuad }
        NumberAnimation { target: pathIsland; property: "anchors.horizontalCenterOffset"; to: 0; duration: 50; easing.type: Easing.InQuad }
    }

    Connections {
        target: root
        function onPathEditErrorChanged() {
            if (root.pathEditError.length > 0) {
                shakeAnimation.start()
            }
        }
    }
}
