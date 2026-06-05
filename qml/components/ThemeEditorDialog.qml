import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Effects
import QtQuick.Layouts
import "../style"
import "dialogs"

Dialog {
    id: root

    title: "Theme Editor"
    modal: true
    closePolicy: Popup.NoAutoClose
    focus: true
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 1600, 1600)
    height: Math.min(parent ? parent.height - 32 : 980, 980)
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
    property string baselineKind: "neutral"
    property var builtInDrafts: []
    property int builtInDraftIndex: 0
    readonly property bool compactLayout: root.width < 1180
    readonly property bool wideTokenLayout: root.width >= 1460
    readonly property color dialogAccent: Theme.accent
    readonly property color sectionFill: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.72 : 0.88)
    readonly property color sectionBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.92 : 0.78)
    readonly property color tokenRowFill: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.50 : 0.70)
    readonly property color tokenRowFillHover: Theme.withAlpha(Theme.surfaceHover, themeController.isDark ? 0.46 : 0.60)
    readonly property color tokenRowBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.32 : 0.24)
    readonly property color tokenHintText: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.68 : 0.74)
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
        { key: "panelSurfaceSoft", title: "Panel Surface Soft", hint: "Subtle panel tint and soft overlays" },
        { key: "panelSurfaceStrong", title: "Panel Surface Strong", hint: "Elevated cards and menus" },
        { key: "panelBorder", title: "Panel Border", hint: "Panel outlines" },
        { key: "controlSurface", title: "Control Surface", hint: "Inputs and buttons" },
        { key: "controlSurfaceActive", title: "Control Active", hint: "Pressed controls" },
        { key: "controlBorder", title: "Control Border", hint: "Control outline" }
    ]
    readonly property var stateTokens: [
        { key: "itemHoverFill", title: "Hover Fill", hint: "Hovered rows and crumbs" },
        { key: "itemCurrentFill", title: "Current Fill", hint: "Focused current row" },
        { key: "itemCurrentBorder", title: "Current Border", hint: "Focused current row outline" },
        { key: "itemSelectedFill", title: "Selection Fill", hint: "Selected rows and chips" },
        { key: "itemSelectedFillInactive", title: "Selection Fill Inactive", hint: "Selection in inactive panels" },
        { key: "itemSelectedBorder", title: "Selection Border", hint: "Selection outline" },
        { key: "itemSelectedBorderInactive", title: "Selection Border Inactive", hint: "Selection outline in inactive panels" },
        { key: "danger", title: "Danger", hint: "Destructive actions" },
        { key: "success", title: "Success", hint: "Positive states" },
        { key: "warning", title: "Warning", hint: "Caution and alerts" },
        { key: "activeAccent", title: "Active Panel Accent", hint: "Bright frame for the active panel" },
        { key: "activeGlow", title: "Active Panel Glow", hint: "Outer glow around the active panel" },
        { key: "statusRailFill", title: "Status Rail Fill", hint: "Operation rail and footer strip" }
    ]
    readonly property var utilityTokens: [
        { key: "secondaryAccent", title: "Secondary Accent", hint: "Secondary semantic accent" },
        { key: "warmAccent", title: "Warm Accent", hint: "Warm utility and theme affordances" },
        { key: "categoryInfo", title: "Category Info", hint: "Info and navigation accents" },
        { key: "categoryNavigation", title: "Category Navigation", hint: "Navigation and route accents" },
        { key: "categoryAction", title: "Category Action", hint: "Action and operation accents" },
        { key: "categoryUtility", title: "Category Utility", hint: "Utility and view accents" },
        { key: "categorySystem", title: "Category System", hint: "System and admin accents" }
    ]
    readonly property var overlayTokens: [
        { key: "overlayScrim", title: "Overlay Scrim", hint: "Modal backdrop dimming" },
        { key: "menuBorder", title: "Menu Border", hint: "Menu and popup outlines" },
        { key: "menuSeparator", title: "Menu Separator", hint: "Menu dividers" },
        { key: "menuItemPressed", title: "Menu Item Pressed", hint: "Pressed menu row state" },
        { key: "chromeGradientStart", title: "Chrome Gradient Start", hint: "Window and toolbar gradient start" },
        { key: "chromeGradientMid", title: "Chrome Gradient Mid", hint: "Window and toolbar gradient middle" },
        { key: "chromeGradientEnd", title: "Chrome Gradient End", hint: "Window and toolbar gradient end" },
        { key: "glassShadow", title: "Glass Shadow", hint: "Elevated popup shadow" },
        { key: "shadow", title: "Base Shadow", hint: "Generic soft shadow tone" }
    ]

    onOpened: {
        refreshBuiltInDrafts()
        loadDefaultDraft()
        Qt.callLater(() => contentItem.forceActiveFocus())
    }

    function cloneState(value) {
        return JSON.parse(JSON.stringify(value || {}))
    }

    function childDialogOpen() {
        return importDialog.visible || exportDialog.visible || colorPicker.visible
    }

    function closeEditor() {
        if (!childDialogOpen()) {
            close()
        }
    }

    function refreshBuiltInDrafts() {
        builtInDrafts = themeController.builtInThemeDrafts()
        if (builtInDraftIndex < 0 || builtInDraftIndex >= builtInDrafts.length) {
            builtInDraftIndex = 0
        }
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
        baselineKind = "neutral"
        setStatus("", false)
        dirty = false
    }

    function neutralDraftStateForMode(mode) {
        const nextMode = mode === "light" ? "light" : "dark"
        return ensureStateShape(themeController.defaultThemeDraftForMode(nextMode))
    }

    function loadNeutralDraftForMode(mode, preserveIdentity) {
        const nextMode = mode === "light" ? "light" : "dark"
        const state = neutralDraftStateForMode(nextMode)
        if (preserveIdentity && workingState) {
            state.id = themeId()
            state.name = themeName()
        }
        defaultDraftState = cloneState(state)
        initialState = cloneState(state)
        workingState = cloneState(state)
        baselineKind = "neutral"
        setStatus("Switched to the neutral " + (nextMode === "light" ? "light" : "dark") + " draft preset.", false)
        dirty = false
    }

    function loadBuiltInDraft(index) {
        if (!builtInDrafts || index < 0 || index >= builtInDrafts.length) {
            setStatus("Built-in theme preset is not available.", true)
            return
        }

        const source = ensureStateShape(builtInDrafts[index])
        const editable = cloneState(source)
        const sourceId = normalizedThemeId(source.id)
        editable.id = sourceId.length > 0 ? (sourceId + "-custom") : "custom-theme"
        editable.name = (source.name && source.name.length > 0 ? source.name : "Built-in Theme") + " Custom"

        builtInDraftIndex = index
        defaultDraftState = cloneState(source)
        initialState = cloneState(editable)
        workingState = cloneState(editable)
        baselineKind = "builtin"
        setStatus("Loaded built-in colors from " + source.name + ". Save as a separate custom theme to keep edits.", false)
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
        const groups = allEditableTokens()
        const changed = []
        for (let i = 0; i < groups.length; ++i) {
            const token = groups[i]
            if (tokenChanged(token.key)) {
                changed.push(token)
            }
        }
        return changed
    }

    function changedTokenCount() {
        return changedTokenModels().length
    }

    function allEditableTokens() {
        return []
            .concat(foundationTokens)
            .concat(textTokens)
            .concat(interfaceTokens)
            .concat(stateTokens)
            .concat(utilityTokens)
            .concat(overlayTokens)
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
        case "panelSurfaceSoft":
        case "panelSurfaceStrong":
        case "panelBorder":
        case "activeAccent":
        case "activeGlow":
        case "menuBorder":
        case "menuSeparator":
        case "menuItemPressed":
        case "chromeGradientStart":
        case "chromeGradientMid":
        case "chromeGradientEnd":
        case "glassShadow":
        case "shadow":
            return "chrome"
        case "controlSurface":
        case "controlSurfaceActive":
        case "controlBorder":
        case "accent":
        case "accentText":
        case "focusRing":
        case "secondaryAccent":
        case "warmAccent":
        case "categoryInfo":
        case "categoryNavigation":
        case "categoryAction":
        case "categoryUtility":
        case "categorySystem":
            return "controls"
        case "itemHoverFill":
        case "itemCurrentFill":
        case "itemCurrentBorder":
        case "itemSelectedFill":
        case "itemSelectedFillInactive":
        case "itemSelectedBorder":
        case "itemSelectedBorderInactive":
        case "textPrimary":
        case "textSecondary":
            return "list"
        case "overlayScrim":
            return "background"
        case "statusRailFill":
            return "status"
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

    function tokenAreaAccent(key) {
        switch (tokenArea(key)) {
        case "background":
            return Theme.categoryInfo
        case "chrome":
            return Theme.categorySystem
        case "controls":
            return Theme.categoryUtility
        case "list":
            return Theme.categoryNavigation
        case "status":
            return Theme.categoryAction
        default:
            return root.dialogAccent
        }
    }

    function tokenModelForKey(key) {
        const groups = allEditableTokens()
        for (let i = 0; i < groups.length; ++i) {
            if (groups[i].key === key) {
                return groups[i]
            }
        }
        return null
    }

    function hoveredTokenTitle() {
        const token = tokenModelForKey(hoveredTokenKey)
        return token ? token.title : ""
    }

    function hoveredTokenDetail() {
        const token = tokenModelForKey(hoveredTokenKey)
        if (!token) {
            return "No token focused"
        }
        return token.key + " - " + token.hint
    }

    function areaHighlighted(area) {
        return hoveredTokenKey.length > 0 && tokenArea(hoveredTokenKey) === area
    }

    function areaChanged(area) {
        const groups = allEditableTokens()
        for (let i = 0; i < groups.length; ++i) {
            const token = groups[i]
            if (tokenArea(token.key) === area && tokenChanged(token.key)) {
                return true
            }
        }
        return false
    }

    function changedTokensForArea(area) {
        const groups = allEditableTokens()
        const changed = []
        for (let i = 0; i < groups.length; ++i) {
            const token = groups[i]
            if (tokenArea(token.key) === area && tokenChanged(token.key)) {
                changed.push(token)
            }
        }
        return changed
    }

    function areaMarkerLabel(area) {
        const changed = changedTokensForArea(area)
        if (changed.length === 0) {
            return ""
        }
        if (changed.length === 1) {
            return changed[0].title
        }
        return changed[0].title + " +" + (changed.length - 1)
    }

    function areaMarkerDetails(area) {
        const changed = changedTokensForArea(area)
        if (changed.length === 0) {
            switch (area) {
            case "background":
                return "Window backdrop and base surfaces"
            case "chrome":
                return "Panels, sidebar and frame"
            case "controls":
                return "Toolbar, inputs and actions"
            case "list":
                return "Content rows and preview card"
            case "status":
                return "Badges and semantic states"
            default:
                return "Preview region"
            }
        }

        let labels = ""
        for (let i = 0; i < changed.length; ++i) {
            labels += (i === 0 ? "" : ", ") + changed[i].title
        }
        return labels
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
        const nextMode = value === "light" ? "light" : "dark"
        if (baselineKind === "builtin") {
            setStatus("Built-in based drafts keep their original tone. Load a neutral draft to switch Light/Dark.", false)
            return
        }
        if (workingState && workingState.mode === nextMode) {
            return
        }
        loadNeutralDraftForMode(nextMode, true)
    }

    function setColorValue(key, value) {
        withUpdatedState(function(next) {
            next.colors[key] = (value || "").toString().trim()
        })
    }

    function resetTokenToDefault(key) {
        const baselineDefaults = baselineKind === "builtin"
                               ? defaultDraftState
                               : neutralDraftStateForMode(workingState && workingState.mode === "light" ? "light" : "dark")
        const fallback = previewColorFromState(baselineDefaults, key)
                         || previewColorFromState(defaultDraftState, key)
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
        defaultDraftState = neutralDraftStateForMode(initialState.mode)
        workingState = cloneState(initialState)
        baselineKind = "file"
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

    function themeLibraryFolderUrl() {
        const directory = themeController.customThemeDirectory()
        if (directory.length === 0) {
            return ""
        }

        const normalized = directory.replace(/\\/g, "/")
        if (/^[A-Za-z]:/.test(normalized)) {
            return "file:///" + normalized
        }
        return "file:///" + normalized
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
        baselineKind = "file"
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
        onCloseRequested: root.close()
    }

    footer: DialogFooter {
        Item {
            Layout.fillWidth: true
        }

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
            secondaryTextColor: Theme.textPrimary
            onClicked: {
                importDialog.currentFolder = root.themeLibraryFolderUrl()
                importDialog.open()
            }
        }

        DialogActionButton {
            text: "Save Theme As..."
            highlighted: true
            primaryColor: root.dialogAccent
            onClicked: {
                exportDialog.currentFolder = root.themeLibraryFolderUrl()
                exportDialog.selectedFile = root.defaultSaveFileUrl()
                exportDialog.open()
            }
        }
    }

    contentItem: ColumnLayout {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        spacing: 0
        clip: true
        focus: true

        Keys.priority: Keys.BeforeItem
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                root.closeEditor()
                event.accepted = true
            }
        }

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

        SplitView {
            id: mainSplitView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 20
            orientation: Qt.Horizontal

            ScrollView {
                id: editorScrollView
                SplitView.preferredWidth: root.compactLayout ? mainSplitView.width : (root.wideTokenLayout ? 460 : 400)
                SplitView.minimumWidth: 260
                SplitView.fillWidth: root.compactLayout
                SplitView.fillHeight: true
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
                                        enabled: root.baselineKind !== "builtin"
                                        accentColor: root.dialogAccent
                                        onClicked: root.setThemeMode("dark")
                                    }

                                    ThemeModePill {
                                        title: "Light"
                                        selected: root.workingState.mode === "light"
                                        enabled: root.baselineKind !== "builtin"
                                        accentColor: root.dialogAccent
                                        onClicked: root.setThemeMode("light")
                                    }
                                }

                                Label {
                                    text: "Built-in base"
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                    Layout.topMargin: 4
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    ComboBox {
                                        id: builtInBaseCombo
                                        Layout.fillWidth: true
                                        implicitHeight: 30
                                        model: root.builtInDrafts
                                        textRole: "name"
                                        valueRole: "id"
                                        currentIndex: root.builtInDraftIndex
                                        onActivated: (index) => root.builtInDraftIndex = index

                                        delegate: ItemDelegate {
                                            width: builtInBaseCombo.width
                                            height: 34
                                            padding: 0
                                            highlighted: builtInBaseCombo.highlightedIndex === index

                                            contentItem: RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 9
                                                anchors.rightMargin: 9
                                                spacing: 8

                                                Rectangle {
                                                    Layout.preferredWidth: 10
                                                    Layout.preferredHeight: 10
                                                    radius: 5
                                                    color: modelData && modelData.colors && modelData.colors.accent
                                                           ? modelData.colors.accent
                                                           : Theme.accent
                                                    border.color: Theme.withAlpha(Theme.textPrimary, 0.18)
                                                    border.width: 1
                                                }

                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 0

                                                    Label {
                                                        text: modelData && modelData.name ? modelData.name : ""
                                                        color: builtInBaseCombo.highlightedIndex === index
                                                               ? Theme.accent
                                                               : Theme.textPrimary
                                                        font.pixelSize: 11
                                                        font.weight: builtInBaseCombo.currentIndex === index ? Font.DemiBold : Font.Normal
                                                        Layout.fillWidth: true
                                                        elide: Text.ElideRight
                                                    }

                                                    Label {
                                                        text: modelData && modelData.subtitle ? modelData.subtitle : ""
                                                        color: Theme.textSecondary
                                                        font.pixelSize: 9
                                                        Layout.fillWidth: true
                                                        elide: Text.ElideRight
                                                        visible: text.length > 0
                                                    }
                                                }
                                            }

                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: builtInBaseCombo.highlightedIndex === index
                                                       ? Theme.menuItemHover
                                                       : (builtInBaseCombo.currentIndex === index
                                                          ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.13 : 0.09)
                                                          : "transparent")
                                                border.color: builtInBaseCombo.currentIndex === index
                                                              ? Theme.withAlpha(Theme.accent, 0.34)
                                                              : "transparent"
                                                border.width: builtInBaseCombo.currentIndex === index ? 1 : 0
                                            }
                                        }

                                        contentItem: Label {
                                            leftPadding: 10
                                            rightPadding: 28
                                            text: builtInBaseCombo.displayText
                                            color: Theme.textPrimary
                                            font.pixelSize: 11
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideRight
                                        }

                                        indicator: Item {
                                            x: builtInBaseCombo.width - width - 10
                                            y: (builtInBaseCombo.height - height) / 2
                                            width: 10
                                            height: 10

                                            Image {
                                                anchors.fill: parent
                                                source: "../assets/icons/arrow-up.svg"
                                                rotation: builtInBaseCombo.opened ? 0 : 180
                                                opacity: builtInBaseCombo.enabled ? 0.62 : 0.28
                                                sourceSize: Qt.size(10, 10)
                                                layer.enabled: true
                                                layer.effect: MultiEffect {
                                                    colorization: 1.0
                                                    colorizationColor: Theme.textSecondary
                                                }
                                            }
                                        }

                                        background: Rectangle {
                                            radius: Theme.radiusSm
                                            color: builtInBaseCombo.pressed
                                                   ? Theme.controlSurfaceActive
                                                   : (builtInBaseCombo.hovered ? Theme.panelSurfaceSoft : Theme.controlSurface)
                                            border.color: builtInBaseCombo.opened ? Theme.accent : Theme.controlBorder
                                            border.width: 1
                                        }

                                        popup: Popup {
                                            y: builtInBaseCombo.height + 4
                                            width: builtInBaseCombo.width
                                            implicitHeight: Math.min(contentItem.implicitHeight + 8, 248)
                                            padding: 4
                                            dim: false

                                            contentItem: ListView {
                                                clip: true
                                                implicitHeight: contentHeight
                                                model: builtInBaseCombo.popup.visible ? builtInBaseCombo.delegateModel : null
                                                currentIndex: builtInBaseCombo.highlightedIndex
                                                interactive: contentHeight > height
                                                spacing: 1

                                                ScrollIndicator.vertical: ScrollIndicator {}
                                            }

                                            background: Item {
                                                Rectangle {
                                                    anchors.fill: parent
                                                    anchors.topMargin: 3
                                                    anchors.leftMargin: 2
                                                    anchors.rightMargin: 1
                                                    radius: Theme.radius + 2
                                                    color: Theme.shadow
                                                    opacity: themeController.isDark ? 0.90 : 0.70
                                                }

                                                Rectangle {
                                                    anchors.fill: parent
                                                    anchors.topMargin: 1
                                                    anchors.leftMargin: 1
                                                    radius: Theme.radius + 1
                                                    color: Theme.accent
                                                    opacity: themeController.isDark ? 0.14 : 0.06
                                                }

                                                Rectangle {
                                                    anchors.fill: parent
                                                    radius: Theme.radius + 1
                                                    color: Theme.menuSurface
                                                    border.color: Theme.menuBorder
                                                    border.width: 1
                                                    layer.enabled: true
                                                    layer.effect: MultiEffect {
                                                        shadowEnabled: true
                                                        shadowColor: Theme.glassShadow
                                                        shadowBlur: 16
                                                    }
                                                }

                                                Rectangle {
                                                    anchors.top: parent.top
                                                    anchors.left: parent.left
                                                    anchors.right: parent.right
                                                    anchors.topMargin: 1
                                                    anchors.leftMargin: 5
                                                    anchors.rightMargin: 5
                                                    height: 1
                                                    radius: 0.5
                                                    color: Theme.withAlpha(themeController.isDark ? Theme.textPrimary : Theme.bg,
                                                                           themeController.isDark ? 0.13 : 0.55)
                                                }
                                            }
                                        }
                                    }

                                    Button {
                                        id: loadBuiltInButton
                                        text: "Load"
                                        implicitHeight: 30
                                        implicitWidth: 58
                                        enabled: root.builtInDrafts.length > 0
                                        onClicked: root.loadBuiltInDraft(root.builtInDraftIndex)

                                        contentItem: Label {
                                            text: loadBuiltInButton.text
                                            color: loadBuiltInButton.enabled ? Theme.textPrimary : Theme.textSecondary
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        background: Rectangle {
                                            radius: Theme.radiusSm
                                            color: loadBuiltInButton.pressed
                                                   ? Theme.controlSurfaceActive
                                                   : (loadBuiltInButton.hovered ? Theme.panelSurfaceSoft : Theme.controlSurface)
                                            border.color: Theme.controlBorder
                                            border.width: 1
                                        }
                                    }
                                }

                                Label {
                                    visible: root.baselineKind === "builtin"
                                    text: "Tone is locked to the source theme; save creates a separate custom JSON."
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 10
                                    color: Theme.textSecondary
                                }
                            }
                        }

                        RowLayout {
                            visible: root.wideTokenLayout
                            Layout.fillWidth: true
                            spacing: 14

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                spacing: 14

                                DialogSection {
                                    Layout.fillWidth: true
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
                                    Layout.fillWidth: true
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
                                    Layout.fillWidth: true
                                    title: "OVERLAYS AND MENUS"
                                    accentColor: Theme.categorySystem
                                    fillColor: root.sectionFill
                                    borderColor: root.sectionBorder

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Repeater {
                                            model: root.overlayTokens
                                            delegate: ThemeTokenRow { token: modelData }
                                        }
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                spacing: 14

                                DialogSection {
                                    Layout.fillWidth: true
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
                                    Layout.fillWidth: true
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

                                DialogSection {
                                    Layout.fillWidth: true
                                    title: "UTILITY ACCENTS"
                                    accentColor: Theme.categoryUtility
                                    fillColor: root.sectionFill
                                    borderColor: root.sectionBorder

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Repeater {
                                            model: root.utilityTokens
                                            delegate: ThemeTokenRow { token: modelData }
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            visible: !root.wideTokenLayout
                            Layout.fillWidth: true
                            spacing: 14

                            DialogSection {
                                Layout.fillWidth: true
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
                                Layout.fillWidth: true
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
                                Layout.fillWidth: true
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
                                Layout.fillWidth: true
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

                            DialogSection {
                                Layout.fillWidth: true
                                title: "UTILITY ACCENTS"
                                accentColor: Theme.categoryUtility
                                fillColor: root.sectionFill
                                borderColor: root.sectionBorder

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Repeater {
                                        model: root.utilityTokens
                                        delegate: ThemeTokenRow { token: modelData }
                                    }
                                }
                            }

                            DialogSection {
                                Layout.fillWidth: true
                                title: "OVERLAYS AND MENUS"
                                accentColor: Theme.categorySystem
                                fillColor: root.sectionFill
                                borderColor: root.sectionBorder

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Repeater {
                                        model: root.overlayTokens
                                        delegate: ThemeTokenRow { token: modelData }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            visible: root.compactLayout
                            Layout.fillWidth: true
                            spacing: 14

                            PreviewSection {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 500
                            }
                            SaveTargetSection {}
                        }
                    }
                }
            }

            ColumnLayout {
                visible: !root.compactLayout
                SplitView.preferredWidth: root.compactLayout ? 0 : (root.wideTokenLayout ? 860 : 720)
                SplitView.minimumWidth: root.compactLayout ? 0 : 500
                SplitView.maximumWidth: root.compactLayout ? 0 : 16777215
                SplitView.fillHeight: true
                spacing: 14

                PreviewSection {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                SaveTargetSection {
                    Layout.fillWidth: true
                }
            }

            handle: Rectangle {
                implicitWidth: 8
                implicitHeight: 8
                color: "transparent"
                visible: !root.compactLayout

                // Interaction overlay
                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    color: root.dialogAccent
                    opacity: SplitHandle.pressed ? 0.12 : (SplitHandle.hovered ? 0.06 : 0)
                    radius: 4
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                // The actual divider line
                Rectangle {
                    anchors.centerIn: parent
                    width: (SplitHandle.hovered || SplitHandle.pressed) ? 2 : 1
                    height: parent.height - 12
                    radius: 1
                    color: (SplitHandle.hovered || SplitHandle.pressed) ? root.dialogAccent : Theme.border
                    opacity: (SplitHandle.hovered || SplitHandle.pressed) ? 1.0 : 0.4
                    
                    Behavior on width { NumberAnimation { duration: 100 } }
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }
    }

    FileDialog {
        id: importDialog
        title: "Load Theme Draft"
        fileMode: FileDialog.OpenFile
        currentFolder: root.themeLibraryFolderUrl()
        nameFilters: ["Theme files (*.json)", "JSON files (*.json)"]
        onAccepted: root.loadDraftFromFile(selectedFile)
    }

    FileDialog {
        id: exportDialog
        title: "Save Theme"
        fileMode: FileDialog.SaveFile
        currentFolder: root.themeLibraryFolderUrl()
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
            opacity: modeButton.enabled ? 1.0 : 0.55
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
            opacity: modeButton.enabled ? 1.0 : 0.62
            border.width: 1
        }
    }

    component ThemeTokenRow: Rectangle {
        id: tokenRow

        property var token
        readonly property bool isChanged: root.tokenChanged(token.key)
        readonly property color rowAccent: root.tokenAreaAccent(token.key)
        readonly property string areaTitle: root.tokenAreaTitle(token.key)

        Layout.fillWidth: true
        implicitHeight: Math.max(64, tokenLayout.implicitHeight + 14)
        radius: Theme.radiusSm
        color: rowHover.hovered ? root.tokenRowFillHover : root.tokenRowFill
        border.color: tokenRow.isChanged
                      ? Theme.withAlpha(tokenRow.rowAccent, themeController.isDark ? 0.62 : 0.44)
                      : root.tokenRowBorder
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 7
            width: 2
            radius: 1
            color: Theme.withAlpha(tokenRow.rowAccent, tokenRow.isChanged || rowHover.hovered ? 0.90 : 0.56)
        }

        HoverHandler {
            id: rowHover
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onHoveredChanged: {
                if (hovered) {
                    root.hoveredTokenKey = token.key
                } else if (root.hoveredTokenKey === token.key) {
                    root.hoveredTokenKey = ""
                }
            }
        }

        ToolTip {
            visible: rowHover.hovered
            delay: 600
            contentItem: Label {
                text: "<b>" + token.title + "</b> (" + token.key + ")<br/>" +
                      (token.hint ? token.hint + "<br/>" : "") +
                      "<i>Affects: " + tokenRow.areaTitle + "</i>" +
                      (tokenRow.isChanged ? "<br/><font color='" + tokenRow.rowAccent + "'>Changed (Initial: " + root.previewColorFromState(root.initialState, token.key) + ")</font>" : "")
                textFormat: Text.RichText
                font.pixelSize: 11
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            background: Rectangle {
                color: Theme.panelSurface
                border.color: Theme.panelBorder
                border.width: 1
                radius: Theme.radiusSm
            }
        }

        RowLayout {
            id: tokenLayout
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 8
            anchors.topMargin: 7
            anchors.bottomMargin: 7
            spacing: 10

            Rectangle {
                id: colorSwatch
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 7
                color: root.previewColor(token.key, Theme.accent)
                border.color: Theme.withAlpha(Theme.textPrimary, 0.18)
                border.width: 1

                TapHandler {
                    onTapped: root.openPickerForToken(token.key, token.title)
                }
                HoverHandler {
                    id: tokenSwatchHover
                    cursorShape: Qt.PointingHandCursor
                }
                ToolTip {
                    visible: tokenSwatchHover.hovered
                    delay: 400
                    text: "Click to pick color for " + token.title
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: token.title
                        Layout.fillWidth: true
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        visible: tokenRow.isChanged
                        Layout.preferredWidth: changedLabel.implicitWidth + 10
                        Layout.preferredHeight: 18
                        radius: 9
                        color: Theme.withAlpha(tokenRow.rowAccent, themeController.isDark ? 0.18 : 0.12)
                        border.color: Theme.withAlpha(tokenRow.rowAccent, themeController.isDark ? 0.48 : 0.34)
                        border.width: 1

                        Label {
                            id: changedLabel
                            anchors.centerIn: parent
                            text: "Changed"
                            color: tokenRow.rowAccent
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Label {
                    text: token.hint ? token.hint : tokenRow.areaTitle
                    Layout.fillWidth: true
                    font.pixelSize: 10
                    color: root.tokenHintText
                    elide: Text.ElideRight
                }

                Label {
                    text: token.key + " / " + tokenRow.areaTitle
                    Layout.fillWidth: true
                    font.pixelSize: 9
                    font.family: "monospace"
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 104
                spacing: 4

                PremiumTextField {
                    Layout.fillWidth: true
                    implicitHeight: 26
                    leftPadding: 7
                    rightPadding: 7
                    font.pixelSize: 10
                    font.family: "monospace"
                    premiumRadius: 5
                    text: root.colorValue(token.key)
                    placeholderText: "#FFFFFFFF"
                    onTextEdited: root.setColorValue(token.key, text)
                }

                Button {
                    id: resetTokenButton
                    visible: tokenRow.isChanged
                    flat: true
                    Layout.alignment: Qt.AlignRight
                    Layout.preferredWidth: 54
                    Layout.preferredHeight: 22
                    onClicked: root.resetTokenToDefault(token.key)

                    contentItem: Label {
                        text: "Reset"
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 5
                        color: resetTokenButton.pressed
                               ? Theme.surfaceActive
                               : (resetTokenButton.hovered ? Theme.panelSurfaceSoft : "transparent")
                        border.color: Theme.withAlpha(tokenRow.rowAccent, resetTokenButton.hovered ? 0.42 : 0.26)
                        border.width: 1
                    }

                    ToolTip.visible: hovered
                    ToolTip.text: "Reset to default"
                }
            }
        }
    }

    component PreviewMarkerChip: Rectangle {
        property string title: ""
        property string detail: ""
        property color accent: Theme.accent
        property bool emphasized: false
        property bool compact: false

        visible: title.length > 0 || detail.length > 0
        implicitHeight: markerColumn.implicitHeight + 10
        implicitWidth: compact
                       ? Math.max(72, markerTitle.implicitWidth + 14)
                       : Math.max(96, Math.min(220, markerColumn.implicitWidth + 14))
        radius: 10
        color: Theme.withAlpha(accent, emphasized ? 0.20 : 0.12)
        border.color: Theme.withAlpha(accent, emphasized ? 0.74 : 0.40)
        border.width: 1

        Column {
            id: markerColumn
            anchors.fill: parent
            anchors.margins: 5
            spacing: 1

            Label {
                id: markerTitle
                visible: parent.parent.title.length > 0
                text: parent.parent.title
                color: parent.parent.accent
                font.pixelSize: 9
                font.weight: Font.Bold
            }

            Label {
                visible: !parent.parent.compact && parent.parent.detail.length > 0
                text: parent.parent.detail
                color: Theme.textPrimary
                font.pixelSize: 8
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }

    component PreviewSection: DialogSection {
        title: "PREVIEW"
        accentColor: root.dialogAccent
        fillColor: root.sectionFill
        borderColor: root.sectionBorder
        expandContent: true

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Flow {
                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight
                spacing: 6

                PreviewMarkerChip {
                    title: root.hoveredTokenKey.length > 0 ? root.hoveredTokenTitle() : "Focused token"
                    detail: root.hoveredTokenKey.length > 0 ? root.hoveredTokenDetail() : "No token focused"
                    accent: root.hoveredTokenKey.length > 0
                            ? root.tokenAreaAccent(root.hoveredTokenKey)
                            : root.dialogAccent
                    emphasized: root.hoveredTokenKey.length > 0
                    compact: root.compactLayout
                }

                PreviewMarkerChip {
                    title: root.hoveredTokenKey.length > 0 ? root.tokenAreaTitle(root.hoveredTokenKey) : "Preview area"
                    detail: root.hoveredTokenKey.length > 0
                            ? root.areaMarkerDetails(root.tokenArea(root.hoveredTokenKey))
                            : "Token rows map to preview regions"
                    accent: root.hoveredTokenKey.length > 0
                            ? root.tokenAreaAccent(root.hoveredTokenKey)
                            : Theme.categoryInfo
                    emphasized: root.hoveredTokenKey.length > 0
                    compact: root.compactLayout
                }

                PreviewMarkerChip {
                    title: root.changedTokenCount() > 0 ? (root.changedTokenCount() + " changed") : "Clean draft"
                    detail: root.changedTokenCount() > 0 ? "Changed rows expose reset controls" : "No edited colors"
                    accent: root.changedTokenCount() > 0 ? root.dialogAccent : Theme.textSecondary
                    emphasized: root.changedTokenCount() > 0
                    compact: root.compactLayout
                }
            }

            ThemeEditorPreviewCard {
                editor: root
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 320
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
