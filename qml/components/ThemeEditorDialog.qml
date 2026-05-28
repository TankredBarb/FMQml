import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../style"
import "dialogs"

Dialog {
    id: root

    title: "Theme Editor"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 48 : 1040, 1040)
    height: Math.min(parent ? parent.height - 48 : 760, 760)
    padding: 0

    property var initialState: ({})
    property var defaultDraftState: ({})
    property var workingState: ({})
    property string statusMessage: ""
    property bool statusIsError: false
    property bool dirty: false
    property string pickerTokenKey: ""
    property string pickerTokenTitle: ""
    property string hoveredTokenKey: ""
    readonly property bool compactLayout: root.width < 900
    readonly property color dialogAccent: Theme.warmAccent
    readonly property color sectionFill: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.72 : 0.88)
    readonly property color sectionBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.92 : 0.78)
    readonly property var foundationTokens: [
        { key: "bg", title: "Background", hint: "Main app backdrop" },
        { key: "surface", title: "Surface", hint: "Primary cards and panes" },
        { key: "surfaceHover", title: "Surface Hover", hint: "Hover state on surfaces" },
        { key: "surfaceActive", title: "Surface Active", hint: "Pressed or active state" },
        { key: "border", title: "Border", hint: "Base outlines and dividers" }
    ]
    readonly property var textTokens: [
        { key: "textPrimary", title: "Primary Text", hint: "Headings and key labels" },
        { key: "textSecondary", title: "Secondary Text", hint: "Hints and metadata" },
        { key: "accent", title: "Accent", hint: "Primary brand/action color" },
        { key: "accentText", title: "Accent Text", hint: "Text on accent surfaces" },
        { key: "focusRing", title: "Focus Ring", hint: "Keyboard focus highlight" }
    ]
    readonly property var interfaceTokens: [
        { key: "panelSurface", title: "Panel Surface", hint: "Panel body tone" },
        { key: "panelBorder", title: "Panel Border", hint: "Panel outlines" },
        { key: "controlSurface", title: "Control Surface", hint: "Inputs and buttons" },
        { key: "controlSurfaceActive", title: "Control Active", hint: "Pressed controls" },
        { key: "controlBorder", title: "Control Border", hint: "Control outline" }
    ]
    readonly property var stateTokens: [
        { key: "itemSelectedFill", title: "Selection Fill", hint: "Selected rows and chips" },
        { key: "itemSelectedBorder", title: "Selection Border", hint: "Selection outline" },
        { key: "danger", title: "Danger", hint: "Destructive actions" },
        { key: "success", title: "Success", hint: "Positive states" },
        { key: "warning", title: "Warning", hint: "Caution and alerts" }
    ]

    onOpened: {
        loadDefaultDraft()
        Qt.callLater(() => contentItem.forceActiveFocus())
    }

    function cloneState(value) {
        return JSON.parse(JSON.stringify(value || {}))
    }

    function ensureStateShape(state) {
        const next = cloneState(state)
        next.id = next.id ? next.id.toString() : ""
        next.name = next.name ? next.name.toString() : ""
        if (!next.mode || (next.mode !== "light" && next.mode !== "dark")) {
            next.mode = "dark"
        }
        if (!next.colors) {
            next.colors = {}
        }
        return next
    }

    function syncDirtyState() {
        dirty = JSON.stringify(workingState) !== JSON.stringify(initialState)
    }

    function withUpdatedState(mutator) {
        const next = ensureStateShape(workingState)
        mutator(next)
        workingState = next
        syncDirtyState()
    }

    function setStatus(message, isError) {
        statusMessage = message
        statusIsError = !!isError
    }

    function loadDefaultDraft() {
        const state = ensureStateShape(themeController.defaultThemeDraft())
        defaultDraftState = cloneState(state)
        initialState = cloneState(state)
        workingState = cloneState(state)
        setStatus("", false)
        dirty = false
    }

    function normalizedThemeId(value) {
        const compact = (value || "").toString().trim().toLowerCase()
        if (compact.length === 0) {
            return ""
        }
        const slug = compact
            .replace(/[^a-z0-9]+/g, "-")
            .replace(/^-+|-+$/g, "")
        return slug
    }

    function themeName() {
        return workingState && typeof workingState.name === "string" ? workingState.name : ""
    }

    function themeId() {
        return workingState && typeof workingState.id === "string" ? workingState.id : ""
    }

    function colorValue(key) {
        if (!workingState || !workingState.colors) {
            return ""
        }
        const value = workingState.colors[key]
        return value ? value.toString() : ""
    }

    function previewColor(key, fallback) {
        const value = colorValue(key)
        return value && value.length > 0 ? value : fallback
    }

    function tokenChanged(key) {
        return colorValue(key) !== previewColorFromState(initialState, key)
    }

    function previewColorFromState(state, key) {
        if (!state || !state.colors) {
            return ""
        }
        const value = state.colors[key]
        return value ? value.toString() : ""
    }

    function changedTokenModels() {
        const groups = []
            .concat(foundationTokens)
            .concat(textTokens)
            .concat(interfaceTokens)
            .concat(stateTokens)
        const changed = []
        for (let i = 0; i < groups.length; ++i) {
            const token = groups[i]
            if (tokenChanged(token.key)) {
                changed.push(token)
            }
        }
        return changed
    }

    function tokenArea(key) {
        switch (key) {
        case "bg":
        case "surface":
        case "border":
        case "surfaceHover":
        case "surfaceActive":
            return "background"
        case "panelSurface":
        case "panelBorder":
            return "chrome"
        case "controlSurface":
        case "controlSurfaceActive":
        case "controlBorder":
        case "accent":
        case "accentText":
        case "focusRing":
            return "controls"
        case "itemSelectedFill":
        case "itemSelectedBorder":
        case "textPrimary":
        case "textSecondary":
            return "list"
        case "danger":
        case "success":
        case "warning":
            return "status"
        default:
            return "background"
        }
    }

    function tokenAreaTitle(key) {
        switch (tokenArea(key)) {
        case "background":
            return "Background"
        case "chrome":
            return "Panel Chrome"
        case "controls":
            return "Controls"
        case "list":
            return "Content List"
        case "status":
            return "Status Badges"
        default:
            return "Preview"
        }
    }

    function areaHighlighted(area) {
        return hoveredTokenKey.length > 0 && tokenArea(hoveredTokenKey) === area
    }

    function areaChanged(area) {
        const groups = []
            .concat(foundationTokens)
            .concat(textTokens)
            .concat(interfaceTokens)
            .concat(stateTokens)
        for (let i = 0; i < groups.length; ++i) {
            const token = groups[i]
            if (tokenArea(token.key) === area && tokenChanged(token.key)) {
                return true
            }
        }
        return false
    }

    function setThemeName(value) {
        withUpdatedState(function(next) {
            next.name = value ? value.trim() : ""
        })
    }

    function setThemeId(value) {
        withUpdatedState(function(next) {
            next.id = normalizedThemeId(value)
        })
    }

    function setThemeMode(value) {
        withUpdatedState(function(next) {
            next.mode = value === "light" ? "light" : "dark"
        })
    }

    function setColorValue(key, value) {
        withUpdatedState(function(next) {
            next.colors[key] = (value || "").toString().trim()
        })
    }

    function resetTokenToDefault(key) {
        const fallback = previewColorFromState(defaultDraftState, key)
        withUpdatedState(function(next) {
            next.colors[key] = fallback
        })
    }

    function resetDraft() {
        workingState = cloneState(initialState)
        setStatus("Draft reset to the current editor baseline.", false)
        dirty = false
    }

    function loadDraftFromFile(fileUrl) {
        const state = themeController.readThemeStateFromFile(fileUrl.toString())
        if (!state || !state.colors) {
            setStatus("Theme file could not be loaded into the draft editor.", true)
            return
        }
        initialState = ensureStateShape(state)
        workingState = cloneState(initialState)
        setStatus("Theme draft loaded from file. Active app theme was not changed.", false)
        dirty = false
    }

    function defaultSaveFileUrl() {
        const directory = themeController.customThemeDirectory()
        const draftId = normalizedThemeId(themeId())
        const fileName = (draftId.length > 0 ? draftId : "custom-theme") + ".json"
        const nativePath = directory.length > 0 ? (directory + "/" + fileName) : fileName
        const normalized = nativePath.replace(/\\/g, "/")
        if (/^[A-Za-z]:/.test(normalized)) {
            return "file:///" + normalized
        }
        return normalized === fileName ? normalized : "file:///" + normalized
    }

    function validateDraftForSave(fileUrl) {
        const trimmedName = themeName().trim()
        const trimmedId = normalizedThemeId(themeId())
        if (trimmedName.length === 0) {
            setStatus("Theme name is required before saving.", true)
            return false
        }
        if (trimmedId.length === 0) {
            setStatus("Theme id is required before saving.", true)
            return false
        }
        if (!themeController.isThemeIdAvailable(trimmedId, fileUrl.toString())) {
            setStatus("Theme id is already used by a built-in or saved custom theme.", true)
            return false
        }
        return true
    }

    function saveDraftToFile(fileUrl) {
        if (!validateDraftForSave(fileUrl)) {
            return
        }
        const stateToSave = ensureStateShape(workingState)
        stateToSave.id = normalizedThemeId(themeId())
        stateToSave.name = themeName().trim()
        const saved = themeController.writeThemeStateToFile(stateToSave, fileUrl.toString())
        if (!saved) {
            setStatus("Theme file could not be saved. Check the target path and draft values.", true)
            return
        }
        setStatus("Theme saved. Choose it later from the theme picker.", false)
        initialState = cloneState(stateToSave)
        workingState = cloneState(stateToSave)
        dirty = false
    }

    function openPickerForToken(key, title) {
        pickerTokenKey = key
        pickerTokenTitle = title
        colorPicker.selectedColor = previewColor(key, Theme.accent)
        colorPicker.open()
    }

    background: DialogShell {
        accentColor: root.dialogAccent
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.panelBorder
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/moon.svg"
        iconTint: root.dialogAccent
        accentColor: root.dialogAccent
        title: root.title
        subtitle: "Edit a draft, preview it locally, save it as a separate theme file"
        closeText: "x"
        onCloseRequested: root.accept()
    }

    footer: DialogFooter {
        DialogActionButton {
            text: "Reset Draft"
            highlighted: false
            enabled: root.dirty
            secondaryTextColor: root.dialogAccent
            onClicked: root.resetDraft()
        }

        DialogActionButton {
            text: "Load Theme File"
            highlighted: false
            secondaryTextColor: root.dialogAccent
            onClicked: importDialog.open()
        }

        DialogActionButton {
            text: "Save Theme As..."
            highlighted: true
            primaryColor: root.dialogAccent
            primaryHoverColor: Qt.lighter(root.dialogAccent, 1.08)
            primaryPressedColor: Qt.darker(root.dialogAccent, 1.08)
            onClicked: {
                exportDialog.selectedFile = root.defaultSaveFileUrl()
                exportDialog.open()
            }
        }

        DialogActionButton {
            text: "Close"
            highlighted: false
            secondaryTextColor: Theme.textSecondary
            onClicked: root.accept()
        }
    }

    contentItem: ColumnLayout {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        spacing: 0
        clip: true
        focus: true

        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 20
            Layout.bottomMargin: 12
            implicitHeight: statusColumn.implicitHeight + 16
            radius: Theme.radiusSm
            color: root.statusMessage.length > 0
                   ? Theme.withAlpha(root.statusIsError ? Theme.danger : Theme.categoryInfo,
                                     themeController.isDark ? 0.14 : 0.10)
                   : Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.62 : 0.84)
            border.color: root.statusMessage.length > 0
                          ? Theme.withAlpha(root.statusIsError ? Theme.danger : Theme.categoryInfo, 0.45)
                          : Theme.panelBorder
            border.width: 1

            ColumnLayout {
                id: statusColumn
                anchors.fill: parent
                anchors.margins: 8
                spacing: 2

                Label {
                    text: root.statusMessage.length > 0
                          ? root.statusMessage
                          : "This editor starts from a neutral blank draft. It never edits built-in themes or recolors the active app theme."
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                    color: root.statusMessage.length > 0
                           ? (root.statusIsError ? Theme.danger : Theme.categoryInfo)
                           : Theme.textSecondary
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 20
            spacing: 16

            ScrollView {
                id: editorScrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                Pane {
                    width: editorScrollView.availableWidth
                    padding: 0
                    background: null

                    ColumnLayout {
                        width: parent.width
                        spacing: 14

                        DialogSection {
                            title: "IDENTITY"
                            accentColor: root.dialogAccent
                            fillColor: root.sectionFill
                            borderColor: root.sectionBorder

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "Theme name"
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                }

                                PremiumTextField {
                                    Layout.fillWidth: true
                                    text: root.themeName()
                                    placeholderText: "Enter a theme name"
                                    onTextEdited: root.setThemeName(text)
                                }

                                Label {
                                    text: "Theme id"
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                }

                                PremiumTextField {
                                    Layout.fillWidth: true
                                    text: root.themeId()
                                    placeholderText: "Enter a unique theme id"
                                    onTextEdited: root.setThemeId(text)
                                }

                                Label {
                                    text: "Tone mode"
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                }

                                RowLayout {
                                    spacing: 8

                                    ThemeModePill {
                                        title: "Dark"
                                        selected: root.workingState.mode !== "light"
                                        accentColor: root.dialogAccent
                                        onClicked: root.setThemeMode("dark")
                                    }

                                    ThemeModePill {
                                        title: "Light"
                                        selected: root.workingState.mode === "light"
                                        accentColor: root.dialogAccent
                                        onClicked: root.setThemeMode("light")
                                    }
                                }
                            }
                        }

                        DialogSection {
                            title: "FOUNDATION"
                            accentColor: Theme.categoryInfo
                            fillColor: root.sectionFill
                            borderColor: root.sectionBorder

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Repeater {
                                    model: root.foundationTokens
                                    delegate: ThemeTokenRow { token: modelData }
                                }
                            }
                        }

                        DialogSection {
                            title: "TEXT AND ACCENT"
                            accentColor: Theme.categoryNavigation
                            fillColor: root.sectionFill
                            borderColor: root.sectionBorder

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Repeater {
                                    model: root.textTokens
                                    delegate: ThemeTokenRow { token: modelData }
                                }
                            }
                        }

                        DialogSection {
                            title: "INTERFACE"
                            accentColor: Theme.categoryUtility
                            fillColor: root.sectionFill
                            borderColor: root.sectionBorder

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Repeater {
                                    model: root.interfaceTokens
                                    delegate: ThemeTokenRow { token: modelData }
                                }
                            }
                        }

                        DialogSection {
                            title: "STATE COLORS"
                            accentColor: Theme.categoryAction
                            fillColor: root.sectionFill
                            borderColor: root.sectionBorder

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Repeater {
                                    model: root.stateTokens
                                    delegate: ThemeTokenRow { token: modelData }
                                }
                            }
                        }

                        ColumnLayout {
                            visible: root.compactLayout
                            Layout.fillWidth: true
                            spacing: 14

                            PreviewSection {}
                            SaveTargetSection {}
                        }
                    }
                }
            }

            ColumnLayout {
                visible: !root.compactLayout
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                spacing: 14

                PreviewSection {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                SaveTargetSection {
                    Layout.fillWidth: true
                }
            }
        }
    }

    FileDialog {
        id: importDialog
        title: "Load Theme Draft"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Theme files (*.json)", "JSON files (*.json)"]
        onAccepted: root.loadDraftFromFile(selectedFile)
    }

    FileDialog {
        id: exportDialog
        title: "Save Theme"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["Theme files (*.json)", "JSON files (*.json)"]
        onAccepted: root.saveDraftToFile(selectedFile)
    }

    ColorDialog {
        id: colorPicker
        title: root.pickerTokenTitle.length > 0 ? ("Choose " + root.pickerTokenTitle) : "Choose Color"
        onAccepted: {
            if (root.pickerTokenKey.length > 0) {
                root.setColorValue(root.pickerTokenKey, selectedColor.toString())
            }
        }
    }

    component ThemeModePill: Button {
        id: modeButton
        property string title: ""
        property bool selected: false
        property color accentColor: Theme.accent

        text: title
        implicitHeight: 34
        implicitWidth: 88

        contentItem: Label {
            text: modeButton.text
            color: modeButton.selected ? Theme.accentText : Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 12
            font.weight: modeButton.selected ? Font.DemiBold : Font.Medium
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: modeButton.selected
                   ? modeButton.accentColor
                   : (modeButton.hovered ? Theme.controlSurfaceActive : Theme.controlSurface)
            border.color: modeButton.selected
                          ? modeButton.accentColor
                          : Theme.controlBorder
            border.width: 1
        }
    }

    component ThemeTokenRow: Rectangle {
        id: tokenRow

        property var token

        Layout.fillWidth: true
        implicitHeight: tokenLayout.implicitHeight + 12
        radius: Theme.radiusSm
        color: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.78 : 0.92)
        border.color: root.tokenChanged(token.key)
                      ? Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.60 : 0.42)
                      : Theme.panelBorder
        border.width: 1

        HoverHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onHoveredChanged: {
                if (hovered) {
                    root.hoveredTokenKey = token.key
                } else if (root.hoveredTokenKey === token.key) {
                    root.hoveredTokenKey = ""
                }
            }
        }

        RowLayout {
            id: tokenLayout
            anchors.fill: parent
            anchors.margins: 8
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 8
                color: root.previewColor(token.key, Theme.accent)
                border.color: Theme.withAlpha(Theme.textPrimary, 0.18)
                border.width: 1
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Label {
                    text: token.title
                    Layout.fillWidth: true
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    text: token.hint
                    Layout.fillWidth: true
                    font.pixelSize: 10
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                Label {
                    visible: root.tokenChanged(token.key)
                    text: "Changed from " + root.previewColorFromState(root.initialState, token.key)
                    Layout.fillWidth: true
                    font.pixelSize: 10
                    color: root.dialogAccent
                    elide: Text.ElideRight
                }

                Label {
                    text: "Affects: " + root.tokenAreaTitle(token.key)
                    Layout.fillWidth: true
                    font.pixelSize: 10
                    color: root.areaHighlighted(root.tokenArea(token.key)) ? root.dialogAccent : Theme.textSecondary
                    elide: Text.ElideRight
                }
            }

            PremiumTextField {
                Layout.preferredWidth: 138
                text: root.colorValue(token.key)
                placeholderText: "#FFFFFFFF"
                onTextEdited: root.setColorValue(token.key, text)
            }

            DialogActionButton {
                text: "Pick"
                highlighted: false
                secondaryTextColor: root.dialogAccent
                onClicked: root.openPickerForToken(token.key, token.title)
            }

            Button {
                id: resetTokenButton
                visible: root.tokenChanged(token.key)
                flat: true
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                onClicked: root.resetTokenToDefault(token.key)

                contentItem: Label {
                    text: "↺"
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 8
                    color: resetTokenButton.pressed
                           ? Theme.surfaceActive
                           : (resetTokenButton.hovered ? Theme.panelSurfaceSoft : "transparent")
                    border.color: Theme.withAlpha(Theme.panelBorder, 0.75)
                    border.width: 1
                }

                ToolTip.visible: hovered
                ToolTip.text: "Reset to default"
            }
        }
    }

    component ThemePreviewCard: Rectangle {
        readonly property color previewBg: root.previewColor("bg", Theme.bg)
        readonly property color previewSurface: root.previewColor("surface", Theme.surface)
        readonly property color previewSurfaceHover: root.previewColor("surfaceHover", Theme.surfaceHover)
        readonly property color previewSurfaceActive: root.previewColor("surfaceActive", Theme.surfaceActive)
        readonly property color previewTextPrimary: root.previewColor("textPrimary", Theme.textPrimary)
        readonly property color previewTextSecondary: root.previewColor("textSecondary", Theme.textSecondary)
        readonly property color previewBorder: root.previewColor("border", Theme.border)
        readonly property color previewAccent: root.previewColor("accent", Theme.accent)
        readonly property color previewAccentText: root.previewColor("accentText", Theme.accentText)
        readonly property color previewDanger: root.previewColor("danger", Theme.danger)
        readonly property color previewSuccess: root.previewColor("success", Theme.success)
        readonly property color previewWarning: root.previewColor("warning", Theme.warning)
        readonly property color previewPanelSurface: root.previewColor("panelSurface", Theme.panelSurface)
        readonly property color previewPanelBorder: root.previewColor("panelBorder", Theme.panelBorder)
        readonly property color previewControlSurface: root.previewColor("controlSurface", Theme.controlSurface)
        readonly property color previewControlActive: root.previewColor("controlSurfaceActive", Theme.controlSurfaceActive)
        readonly property color previewControlBorder: root.previewColor("controlBorder", Theme.controlBorder)
        readonly property color previewSelectionFill: root.previewColor("itemSelectedFill", Theme.itemSelectedFill)
        readonly property color previewSelectionBorder: root.previewColor("itemSelectedBorder", Theme.itemSelectedBorder)
        readonly property bool backgroundChanged: root.areaChanged("background")
        readonly property bool chromeChanged: root.areaChanged("chrome")
        readonly property bool controlsChanged: root.areaChanged("controls")
        readonly property bool listChanged: root.areaChanged("list")
        readonly property bool statusChanged: root.areaChanged("status")

        radius: Theme.radiusLg
        color: previewBg
        border.color: Theme.withAlpha(previewBorder, 0.88)
        border.width: 1
        clip: true

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.withAlpha(previewAccent, root.workingState.mode === "light" ? 0.08 : 0.12) }
                GradientStop { position: 1.0; color: Theme.withAlpha(previewBg, 1.0) }
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: root.areaHighlighted("background") ? Theme.withAlpha(root.dialogAccent, 0.95) : "transparent"
            border.width: root.areaHighlighted("background") ? 2 : 0
            radius: Theme.radiusLg
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: 12
                color: previewPanelSurface
                border.color: root.areaHighlighted("chrome")
                              ? Theme.withAlpha(root.dialogAccent, 0.95)
                              : (chromeChanged ? Theme.withAlpha(root.dialogAccent, 0.7) : previewPanelBorder)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        radius: 5
                        color: previewAccent
                    }

                    Label {
                        text: root.themeName()
                        visible: text.trim().length > 0
                        color: previewTextPrimary
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: root.themeName().trim().length === 0
                        text: "Untitled Theme"
                        color: previewTextSecondary
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        Layout.preferredWidth: 56
                        Layout.preferredHeight: 22
                        radius: 11
                        color: previewAccent

                        Label {
                            anchors.centerIn: parent
                            text: root.workingState.mode === "light" ? "LIGHT" : "DARK"
                            color: previewAccentText
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 88
                    Layout.fillHeight: true
                    radius: 14
                    color: previewPanelSurface
                    border.color: root.areaHighlighted("chrome")
                                  ? Theme.withAlpha(root.dialogAccent, 0.95)
                                  : (chromeChanged ? Theme.withAlpha(root.dialogAccent, 0.7) : previewPanelBorder)
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Repeater {
                            model: [
                                { text: "Home", tone: previewAccent },
                                { text: "Files", tone: previewTextSecondary },
                                { text: "This PC", tone: previewTextSecondary },
                                { text: "Pinned", tone: previewTextSecondary }
                            ]

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                radius: 9
                                color: index === 0 ? Theme.withAlpha(modelData.tone, 0.18) : "transparent"
                                border.color: index === 0 ? Theme.withAlpha(modelData.tone, 0.34) : "transparent"
                                border.width: 1

                                Label {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.right: parent.right
                                    anchors.rightMargin: 6
                                    text: modelData.text
                                    color: index === 0 ? previewTextPrimary : modelData.tone
                                    elide: Text.ElideRight
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        radius: 12
                        color: previewPanelSurface
                        border.color: root.areaHighlighted("controls")
                                      ? Theme.withAlpha(root.dialogAccent, 0.95)
                                      : (controlsChanged ? Theme.withAlpha(root.dialogAccent, 0.7) : previewPanelBorder)
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 20
                                radius: 10
                                color: previewControlSurface
                                border.color: root.areaHighlighted("controls")
                                              ? Theme.withAlpha(root.dialogAccent, 0.95)
                                              : (controlsChanged ? Theme.withAlpha(root.dialogAccent, 0.7) : previewControlBorder)
                                border.width: 1

                                Label {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    text: "D:/Projects/Preview"
                                    color: previewTextSecondary
                                    font.pixelSize: 10
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 58
                                Layout.preferredHeight: 20
                                radius: 10
                                color: previewAccent

                                Label {
                                    anchors.centerIn: parent
                                    text: "Apply"
                                    color: previewAccentText
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 14
                        color: previewPanelSurface
                        border.color: root.areaHighlighted("list")
                                      ? Theme.withAlpha(root.dialogAccent, 0.95)
                                      : (listChanged ? Theme.withAlpha(root.dialogAccent, 0.7) : previewPanelBorder)
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 6

                            Repeater {
                                model: [
                                    { name: "design-system.fig", meta: "22 KB", selected: false },
                                    { name: "palette-review.png", meta: "1.4 MB", selected: true },
                                    { name: "notes.txt", meta: "4 KB", selected: false },
                                    { name: "theme-preview.json", meta: "2 KB", selected: false }
                                ]

                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    radius: 9
                                    color: modelData.selected ? previewSelectionFill : (hoverHandler.hovered ? previewSurfaceHover : "transparent")
                                    border.color: modelData.selected ? previewSelectionBorder : "transparent"
                                    border.width: 1

                                    HoverHandler {
                                        id: hoverHandler
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 8

                                        Rectangle {
                                            Layout.preferredWidth: 8
                                            Layout.preferredHeight: 8
                                            radius: 4
                                            color: modelData.selected ? previewAccent : previewTextSecondary
                                        }

                                        Label {
                                            text: modelData.name
                                            color: previewTextPrimary
                                            font.pixelSize: 10
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }

                                        Label {
                                            text: modelData.meta
                                            color: previewTextSecondary
                                            font.pixelSize: 9
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                PreviewBadge {
                    title: "Safe"
                    fill: Theme.withAlpha(previewSuccess, 0.16)
                    stroke: Theme.withAlpha(previewSuccess, 0.34)
                    tone: previewSuccess
                }

                PreviewBadge {
                    title: "Warning"
                    fill: Theme.withAlpha(previewWarning, 0.16)
                    stroke: Theme.withAlpha(previewWarning, 0.34)
                    tone: previewWarning
                }

                PreviewBadge {
                    title: "Delete"
                    fill: Theme.withAlpha(previewDanger, 0.16)
                    stroke: Theme.withAlpha(previewDanger, 0.34)
                    tone: previewDanger
                }

                Item {
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.preferredWidth: 78
                    Layout.preferredHeight: 26
                    radius: 13
                    color: previewControlActive
                    border.color: root.areaHighlighted("controls")
                                  ? Theme.withAlpha(root.dialogAccent, 0.95)
                                  : (controlsChanged ? Theme.withAlpha(root.dialogAccent, 0.7) : previewControlBorder)
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        text: "Button"
                        color: previewTextPrimary
                        font.pixelSize: 10
                        font.weight: Font.Medium
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                visible: root.hoveredTokenKey.length > 0
                implicitHeight: 28
                radius: 12
                color: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.14 : 0.10)
                border.color: Theme.withAlpha(root.dialogAccent, 0.30)
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    text: root.hoveredTokenKey.length > 0
                          ? ("Spotlight: " + root.tokenAreaTitle(root.hoveredTokenKey))
                          : ""
                    color: Theme.textPrimary
                    font.pixelSize: 10
                    font.weight: Font.Medium
                }
            }
        }
    }

    component PreviewBadge: Rectangle {
        property string title: ""
        property color fill: "transparent"
        property color stroke: "transparent"
        property color tone: Theme.textPrimary

        Layout.preferredHeight: 24
        Layout.preferredWidth: previewLabel.implicitWidth + 18
        radius: 12
        color: fill
        border.color: stroke
        border.width: 1

        Label {
            id: previewLabel
            anchors.centerIn: parent
            text: parent.title
            color: parent.tone
            font.pixelSize: 10
            font.weight: Font.DemiBold
        }
    }

    component PreviewSection: DialogSection {
        title: "PREVIEW"
        accentColor: Theme.warning
        fillColor: root.sectionFill
        borderColor: root.sectionBorder

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Approximate local preview of the draft. The active app theme stays unchanged until you choose a saved file in the theme picker."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 11
                color: Theme.textSecondary
            }

            ThemePreviewCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 430
            }
        }
    }

    component SaveTargetSection: DialogSection {
        title: "SAVE TARGET"
        accentColor: Theme.categoryInfo
        fillColor: root.sectionFill
        borderColor: root.sectionBorder

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: "Suggested library folder"
                font.pixelSize: 11
                color: Theme.textSecondary
            }

            Label {
                text: themeController.customThemeDirectory().length > 0
                      ? themeController.customThemeDirectory()
                      : "Theme library folder is not available."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 11
                color: Theme.textPrimary
            }

            Label {
                text: "Saved files from this folder will appear in the theme picker."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 11
                color: Theme.textSecondary
            }
        }
    }
}
