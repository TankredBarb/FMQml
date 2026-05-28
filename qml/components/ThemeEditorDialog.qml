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
            onClicked: importDialog.open()
        }

        DialogActionButton {
            text: "Save Theme As..."
            highlighted: true
            primaryColor: root.dialogAccent
            onClicked: {
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

                                        contentItem: Label {
                                            leftPadding: 10
                                            rightPadding: 28
                                            text: builtInBaseCombo.displayText
                                            color: Theme.textPrimary
                                            font.pixelSize: 11
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideRight
                                        }

                                        background: Rectangle {
                                            radius: Theme.radiusSm
                                            color: builtInBaseCombo.pressed
                                                   ? Theme.controlSurfaceActive
                                                   : (builtInBaseCombo.hovered ? Theme.panelSurfaceSoft : Theme.controlSurface)
                                            border.color: Theme.controlBorder
                                            border.width: 1
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

        Layout.fillWidth: true
        implicitHeight: tokenLayout.implicitHeight + 8
        radius: Theme.radiusSm
        color: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.78 : 0.92)
        border.color: root.tokenChanged(token.key)
                      ? Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.60 : 0.42)
                      : Theme.panelBorder
        border.width: 1

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
                      "<i>Affects: " + root.tokenAreaTitle(token.key) + "</i>" +
                      (root.tokenChanged(token.key) ? "<br/><font color='" + root.dialogAccent + "'>Changed (Initial: " + root.previewColorFromState(root.initialState, token.key) + ")</font>" : "")
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
            anchors.margins: 4
            spacing: 6

            Rectangle {
                id: colorSwatch
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                radius: 5
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

            Label {
                text: token.title
                Layout.fillWidth: true
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            PremiumTextField {
                Layout.preferredWidth: 80
                implicitHeight: 24
                leftPadding: 6
                rightPadding: 6
                font.pixelSize: 10
                font.family: "monospace"
                premiumRadius: 4
                text: root.colorValue(token.key)
                placeholderText: "#FFFFFFFF"
                onTextEdited: root.setColorValue(token.key, text)
            }

            Button {
                id: resetTokenButton
                visible: root.tokenChanged(token.key)
                flat: true
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                onClicked: root.resetTokenToDefault(token.key)

                contentItem: Label {
                    text: "R"
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
                    border.color: Theme.withAlpha(Theme.panelBorder, 0.75)
                    border.width: 1
                }

                ToolTip.visible: hovered
                ToolTip.text: "Reset to default"
            }
        }
    }

    component ThemePreviewCard: Rectangle {

        // Preview color bindings
        readonly property color pBg:         root.previewColor("bg",                   Theme.bg)
        readonly property color pSurface:    root.previewColor("surface",              Theme.surface)
        readonly property color pSurfHov:    root.previewColor("surfaceHover",         Theme.surfaceHover)
        readonly property color pSurfAct:    root.previewColor("surfaceActive",        Theme.surfaceActive)
        readonly property color pText:       root.previewColor("textPrimary",          Theme.textPrimary)
        readonly property color pTextSec:    root.previewColor("textSecondary",        Theme.textSecondary)
        readonly property color pBorder:     root.previewColor("border",               Theme.border)
        readonly property color pAccent:     root.previewColor("accent",               Theme.accent)
        readonly property color pAccentText: root.previewColor("accentText",           Theme.accentText)
        readonly property color pFocusRing:  root.previewColor("focusRing",            Theme.focusRing)
        readonly property color pDanger:     root.previewColor("danger",               Theme.danger)
        readonly property color pSuccess:    root.previewColor("success",              Theme.success)
        readonly property color pWarning:    root.previewColor("warning",              Theme.warning)
        readonly property color pActiveAcc:  root.previewColor("activeAccent",         Theme.activeAccent)
        readonly property color pActiveGlow: root.previewColor("activeGlow",           Theme.activeGlow)
        readonly property color pPanelSoft:  root.previewColor("panelSurfaceSoft",     Theme.panelSurfaceSoft)
        readonly property color pPanelStrong: root.previewColor("panelSurfaceStrong",  Theme.panelSurfaceStrong)
        readonly property color pPanel:      root.previewColor("panelSurface",         Theme.panelSurface)
        readonly property color pPanelBrd:   root.previewColor("panelBorder",          Theme.panelBorder)
        readonly property color pCtrl:       root.previewColor("controlSurface",       Theme.controlSurface)
        readonly property color pCtrlAct:    root.previewColor("controlSurfaceActive", Theme.controlSurfaceActive)
        readonly property color pCtrlBrd:    root.previewColor("controlBorder",        Theme.controlBorder)
        readonly property color pItemHover:  root.previewColor("itemHoverFill",        Theme.itemHoverFill)
        readonly property color pItemCur:    root.previewColor("itemCurrentFill",      Theme.itemCurrentFill)
        readonly property color pItemCurBrd: root.previewColor("itemCurrentBorder",    Theme.itemCurrentBorder)
        readonly property color pSelFill:    root.previewColor("itemSelectedFill",     Theme.itemSelectedFill)
        readonly property color pSelFillInact: root.previewColor("itemSelectedFillInactive", Theme.itemSelectedFillInactive)
        readonly property color pSelBrd:     root.previewColor("itemSelectedBorder",   Theme.itemSelectedBorder)
        readonly property color pSelBrdInact: root.previewColor("itemSelectedBorderInactive", Theme.itemSelectedBorderInactive)
        readonly property color pStatusRail: root.previewColor("statusRailFill",       Theme.statusRailFill)
        readonly property color pMenuBrd:    root.previewColor("menuBorder",           Theme.menuBorder)
        readonly property color pMenuSep:    root.previewColor("menuSeparator",        Theme.menuSeparator)
        readonly property color pMenuPress:  root.previewColor("menuItemPressed",      Theme.menuItemPressed)
        readonly property color pGlassShadow: root.previewColor("glassShadow",         Theme.glassShadow)
        readonly property color pShadow:     root.previewColor("shadow",               Theme.shadow)
        readonly property color pOverlayScrim: root.previewColor("overlayScrim",       Theme.overlayScrim)
        readonly property color pSecondary:  root.previewColor("secondaryAccent",      Theme.secondaryAccent)
        readonly property color pWarm:       root.previewColor("warmAccent",           Theme.warmAccent)
        readonly property color pCatInfo:    root.previewColor("categoryInfo",         Theme.categoryInfo)
        readonly property color pCatNav:     root.previewColor("categoryNavigation",   Theme.categoryNavigation)
        readonly property color pCatAction:  root.previewColor("categoryAction",       Theme.categoryAction)
        readonly property color pCatUtility: root.previewColor("categoryUtility",      Theme.categoryUtility)
        readonly property color pCatSystem:  root.previewColor("categorySystem",       Theme.categorySystem)

        // Token highlight helpers
        // Returns true when the given token key is hovered in the left editor panel.
        function h(k)       { return root.hoveredTokenKey === k && k.length > 0 }
        function h2(a,b)    { return h(a) || h(b) }
        function h3(a,b,c)  { return h(a) || h(b) || h(c) }
        // Card background (bg, border)
        radius: Theme.radiusLg
        clip: true
        color: pBg
        border.color: h2("bg","border") ? root.dialogAccent : Theme.withAlpha(pBorder, 0.65)
        border.width:  h2("bg","border") ? 2 : 1

        // Subtle accent-tinted gradient on the background
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(pAccent.r, pAccent.g, pAccent.b, root.workingState.mode === "light" ? 0.06 : 0.10) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 16
            width: 184
            height: 178
            radius: 16
            color: Theme.withAlpha(pOverlayScrim, 0.70)
            z: 5
            TokenHighlight { keys: ["overlayScrim"] }

            Rectangle {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 14
                width: 156
                height: 146
                radius: 12
                color: pPanelStrong
                border.color: h2("menuBorder", "panelSurfaceStrong") ? root.dialogAccent : pMenuBrd
                border.width: h2("menuBorder", "panelSurfaceStrong") ? 2 : 1
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: h2("glassShadow", "shadow") ? root.dialogAccent : pGlassShadow
                    shadowBlur: 20
                    shadowVerticalOffset: 6
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 3

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            text: "Context menu"
                            color: pTextSec
                            font.pixelSize: 8
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            implicitWidth: 34
                            implicitHeight: 14
                            radius: 7
                            color: Theme.withAlpha(pCatSystem, 0.16)
                            border.color: h("categorySystem") ? root.dialogAccent : Theme.withAlpha(pCatSystem, 0.38)
                            border.width: h("categorySystem") ? 2 : 1

                            Label {
                                anchors.centerIn: parent
                                text: "sys"
                                color: h("categorySystem") ? root.dialogAccent : pCatSystem
                                font.pixelSize: 7
                                font.weight: Font.Bold
                            }
                            TokenHighlight { keys: ["categorySystem"] }
                        }
                    }

                    Repeater {
                        model: [
                            { label: "Open",       hint: "Enter",  pressed: false, disabled: false, danger: false, c: pCatAction,  key: "categoryAction" },
                            { label: "Pin folder", hint: "P",      pressed: true,  disabled: false, danger: false, c: pWarm,       key: "warmAccent" },
                            { label: "Copy path",  hint: "Ctrl+C", pressed: false, disabled: false, danger: false, c: pSecondary,  key: "secondaryAccent" },
                            { label: "Reveal",     hint: "",       pressed: false, disabled: true,  danger: false, c: pCatUtility, key: "categoryUtility" },
                            { label: "Delete",     hint: "Del",    pressed: false, disabled: false, danger: true,  c: pDanger,     key: "danger" }
                        ]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 19
                            radius: 7
                            color: modelData.pressed ? pMenuPress : "transparent"
                            opacity: modelData.disabled ? 0.56 : 1.0

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 7
                                anchors.rightMargin: 6
                                spacing: 6

                                Rectangle {
                                    implicitWidth: 7
                                    implicitHeight: 7
                                    radius: 4
                                    color: modelData.danger ? pDanger : modelData.c
                                    border.color: root.hoveredTokenKey === modelData.key ? root.dialogAccent : "transparent"
                                    border.width: root.hoveredTokenKey === modelData.key ? 1 : 0
                                }

                                Label {
                                    text: modelData.label
                                    color: modelData.danger ? pDanger : pText
                                    font.pixelSize: 8
                                    font.weight: modelData.pressed ? Font.DemiBold : Font.Normal
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    visible: modelData.hint.length > 0
                                    text: modelData.hint
                                    color: pTextSec
                                    font.pixelSize: 7
                                }
                            }

                            TokenHighlight {
                                keys: modelData.pressed
                                      ? ["menuItemPressed", modelData.key, "textPrimary", "textSecondary"]
                                      : [modelData.key, "textPrimary", "textSecondary"]
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: pMenuSep
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Repeater {
                            model: [
                                { label: "info", c: pCatInfo, key: "categoryInfo" },
                                { label: "nav",  c: pCatNav,  key: "categoryNavigation" }
                            ]
                            delegate: Rectangle {
                                implicitWidth: 42
                                implicitHeight: 16
                                radius: 8
                                color: Theme.withAlpha(modelData.c, 0.14)
                                border.color: root.hoveredTokenKey === modelData.key ? root.dialogAccent : Theme.withAlpha(modelData.c, 0.36)
                                border.width: root.hoveredTokenKey === modelData.key ? 2 : 1

                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: root.hoveredTokenKey === modelData.key ? root.dialogAccent : modelData.c
                                    font.pixelSize: 7
                                    font.weight: Font.Bold
                                }

                                TokenHighlight { keys: [modelData.key] }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }

                TokenHighlight { keys: ["menuBorder", "menuSeparator", "menuItemPressed", "panelSurfaceStrong", "glassShadow", "shadow"] }
            }
        }

        // Main layout
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8
            // TOOLBAR - panelSurface · panelBorder · accent · textPrimary
            //           controlSurface · controlBorder · accentText
            // Toolbar block
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 40
                radius: 10
                color: pPanel
                border.color: h2("panelSurface","panelBorder") ? root.dialogAccent : Theme.withAlpha(pPanelBrd, 0.85)
                border.width:  h2("panelSurface","panelBorder") ? 2 : 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10; anchors.rightMargin: 10
                    spacing: 8

                    // Accent dot
                    Rectangle {
                        width: 9; height: 9; radius: 5
                        color: pAccent
                        border.color: h("accent") ? root.dialogAccent : "transparent"
                        border.width: h("accent") ? 2 : 0
                    }

                    // Window / theme title - textPrimary
                    Label {
                        text: root.themeName().trim().length > 0 ? root.themeName() : "File Manager"
                        color: pText
                        font.pixelSize: 11; font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.maximumWidth: 130
                        Rectangle {
                            anchors.fill: parent; anchors.margins: -3
                            radius: 6; z: -1
                            color: h("textPrimary") ? Qt.rgba(root.dialogAccent.r, root.dialogAccent.g, root.dialogAccent.b, 0.16) : "transparent"
                            border.color: h("textPrimary") ? root.dialogAccent : "transparent"
                            border.width: h("textPrimary") ? 1 : 0
                        }
                    }

                    // Breadcrumb / path bar - controlSurface · controlBorder · textSecondary · accent
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 26; radius: 9
                        color: pCtrl
                        border.color: h3("controlSurface","controlBorder","textSecondary") ? root.dialogAccent : Theme.withAlpha(pCtrlBrd, 0.80)
                        border.width:  h3("controlSurface","controlBorder","textSecondary") ? 2 : 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8; anchors.rightMargin: 8
                            spacing: 4
                            Label {
                                text: "D:/"
                                color: pAccent; font.pixelSize: 9; font.weight: Font.DemiBold
                            }
                            Label {
                                text: "Projects / Design"
                                color: pTextSec; font.pixelSize: 9
                                Layout.fillWidth: true; elide: Text.ElideRight
                            }
                        }
                        TokenHighlight { keys: ["controlSurface","controlBorder","textSecondary"] }
                    }

                    // Pressed/active control - controlSurfaceActive · controlBorder
                    Rectangle {
                        implicitWidth: 26; implicitHeight: 26; radius: 9
                        color: pCtrlAct
                        border.color: h2("controlSurfaceActive","controlBorder") ? root.dialogAccent : Theme.withAlpha(pCtrlBrd, 0.65)
                        border.width: h2("controlSurfaceActive","controlBorder") ? 2 : 1
                        Label {
                            anchors.centerIn: parent; text: "←"
                            color: pText; font.pixelSize: 11; font.weight: Font.DemiBold
                        }
                        TokenHighlight { keys: ["controlSurfaceActive","controlBorder"] }
                    }

                    // Accent action button - accent · accentText
                    Rectangle {
                        implicitWidth: 54; implicitHeight: 26; radius: 9
                        color: pAccent
                        border.color: h2("accent","accentText") ? root.dialogAccent : "transparent"
                        border.width: h2("accent","accentText") ? 2 : 0
                        Label {
                            anchors.centerIn: parent; text: "Go"
                            color: pAccentText; font.pixelSize: 10; font.weight: Font.DemiBold
                        }
                        TokenHighlight { keys: ["accent","accentText"] }
                    }
                }
                TokenHighlight { keys: ["panelSurface","panelBorder"] }
            }
            // Content row: sidebar + file list.
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                // SIDEBAR - panelSurface · panelBorder · accent · textPrimary
                //           textSecondary · itemSelectedFill · itemSelectedBorder
                // Sidebar block
                Rectangle {
                    Layout.preferredWidth: 108
                    Layout.fillHeight: true
                    radius: 10
                    color: pPanel
                    border.color: h2("panelSurface","panelBorder") ? root.dialogAccent : Theme.withAlpha(pPanelBrd, 0.85)
                    border.width:  h2("panelSurface","panelBorder") ? 2 : 1

                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 8
                        spacing: 3

                        // Section label - accent
                        Label {
                            text: "LIBRARY"
                            color: pAccent; font.pixelSize: 8
                            font.weight: Font.Bold; font.letterSpacing: 0.8
                            Layout.bottomMargin: 2
                        }

                        // Nav items
                        Repeater {
                            model: [
                                { label: "Home",    active: true  },
                                { label: "Files",   active: false },
                                { label: "This PC", active: false },
                                { label: "Pinned",  active: false }
                            ]
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 26; radius: 8
                                color:        modelData.active ? pSelFillInact : "transparent"
                                border.color: modelData.active ? pSelBrdInact  : "transparent"
                                border.width: modelData.active ? 1 : 0

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 7; anchors.rightMargin: 5
                                    spacing: 6

                                    Rectangle {
                                        visible: modelData.active
                                        width: 4; height: 4; radius: 2
                                        color: pAccent
                                    }

                                    Label {
                                        text: modelData.label
                                        color: modelData.active ? pText : pTextSec
                                        font.pixelSize: 10
                                        font.weight: modelData.active ? Font.DemiBold : Font.Normal
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                    }
                                }

                                TokenHighlight {
                                    keys: modelData.active
                                          ? ["itemSelectedFillInactive","itemSelectedBorderInactive","accent","textPrimary"]
                                          : ["textSecondary"]
                                }
                            }
                        }

                        // Divider - border token
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: 1
                            color: h("border") ? root.dialogAccent : Theme.withAlpha(pBorder, 0.38)
                            Layout.topMargin: 3; Layout.bottomMargin: 3
                        }

                        // Pinned folder card - surfaceHover · textPrimary · textSecondary · accent
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: 62; radius: 9
                            color: pPanelSoft
                            border.color: h("surfaceHover") ? root.dialogAccent : Theme.withAlpha(pPanelBrd, 0.55)
                            border.width: h("surfaceHover") ? 2 : 1

                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 7
                                spacing: 4
                                Label {
                                    text: "Moodboards"
                                    color: pText; font.pixelSize: 9; font.weight: Font.DemiBold
                                }
                                Rectangle {
                                    Layout.fillWidth: true; implicitHeight: 4; radius: 2
                                    color: Theme.withAlpha(pAccent, 0.18)
                                    Rectangle {
                                        width: parent.width * 0.64; height: parent.height; radius: parent.radius
                                        color: pAccent
                                    }
                                }
                                Label {
                                    text: "64% · 14 files"
                                    color: pTextSec; font.pixelSize: 8
                                }
                            }
                            TokenHighlight { keys: ["panelSurfaceSoft","surfaceHover","textPrimary","textSecondary"] }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    TokenHighlight { keys: ["panelSurface","panelBorder"] }
                }
                // FILE LIST - surface · border · textPrimary · textSecondary
                //             surfaceHover · surfaceActive · itemSelectedFill
                //             itemSelectedBorder · accent · focusRing
                // File list block
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    radius: 10
                    color: pSurface
                    border.color: h2("surface","border") ? root.dialogAccent : Theme.withAlpha(pBorder, 0.55)
                    border.width:  h2("surface","border") ? 2 : 1
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: h2("activeAccent", "activeGlow") ? root.dialogAccent : pShadow
                        shadowBlur: 18
                        shadowVerticalOffset: 0
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -4
                        radius: 14
                        z: -1
                        color: Theme.withAlpha(pActiveGlow, h2("activeAccent", "activeGlow") ? 0.18 : 0.12)
                        border.color: h2("activeAccent", "activeGlow") ? root.dialogAccent : pActiveAcc
                        border.width: h2("activeAccent", "activeGlow") ? 2 : 1
                    }

                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 8
                        spacing: 0

                        // Column headers - textSecondary · border
                        RowLayout {
                            Layout.fillWidth: true; implicitHeight: 22; spacing: 0
                            Repeater {
                                model: ["Name", "Size", "Modified"]
                                delegate: Label {
                                    text: modelData; color: pTextSec
                                    font.pixelSize: 9; font.weight: Font.Medium
                                    Layout.fillWidth: index === 0
                                    Layout.preferredWidth: index === 0 ? -1 : 56
                                    leftPadding: index === 0 ? 6 : 0
                                }
                            }
                        }

                        // Header divider - border
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: 1
                            color: h("border") ? root.dialogAccent : Theme.withAlpha(pBorder, 0.38)
                            Layout.bottomMargin: 2
                        }

                        // File rows
                        Repeater {
                            model: [
                                { name: "design-system.fig", size: "-",      date: "2d ago",   state: "normal"   },
                                { name: "palette-review.png", size: "1.4 MB", date: "Today",    state: "hover"    },
                                { name: "notes.txt",          size: "4 KB",   date: "Now",      state: "selected" },
                                { name: "exports/",           size: "-",      date: "1w ago",   state: "normal"   },
                                { name: "theme-draft.json",   size: "2 KB",   date: "Just now", state: "active"   }
                            ]
                            delegate: Rectangle {
                                Layout.fillWidth: true; implicitHeight: 28; radius: 8

                                readonly property bool isSel: modelData.state === "selected"
                                readonly property bool isHov: modelData.state === "hover"
                                readonly property bool isAct: modelData.state === "active"

                                color:        isSel ? pSelFill  : isHov ? pItemHover : isAct ? pItemCur : "transparent"
                                border.color: isSel ? pSelBrd   : isAct ? pItemCurBrd : "transparent"
                                border.width: (isSel || isAct) ? 1 : 0

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 7; anchors.rightMargin: 7; spacing: 7

                                    Rectangle {
                                        width: 6; height: 6; radius: 3
                                        color: isSel ? pAccent : pTextSec
                                    }
                                    Label {
                                        text: modelData.name; color: pText
                                        font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight
                                    }
                                    Label {
                                        text: modelData.size; color: pTextSec
                                        font.pixelSize: 9; Layout.preferredWidth: 44
                                        horizontalAlignment: Text.AlignRight
                                    }
                                    Label {
                                        text: modelData.date; color: pTextSec
                                        font.pixelSize: 9; Layout.preferredWidth: 52; elide: Text.ElideRight
                                    }
                                }

                                TokenHighlight {
                                    keys: isSel  ? ["itemSelectedFill","itemSelectedBorder","accent","textPrimary","textSecondary"]
                                        : isHov  ? ["itemHoverFill","textPrimary","textSecondary"]
                                        : isAct  ? ["itemCurrentFill","itemCurrentBorder","textPrimary","textSecondary"]
                                        :          ["textPrimary","textSecondary"]
                                }
                            }
                        }

                        // Focus ring example - focusRing · controlSurface · controlBorder
                        RowLayout {
                            Layout.fillWidth: true; Layout.topMargin: 6; spacing: 8
                            Label { text: "Focus:"; color: pTextSec; font.pixelSize: 9 }
                            Rectangle {
                                implicitWidth: 72; implicitHeight: 22; radius: 8
                                color: pCtrl
                                border.color: h("focusRing") ? root.dialogAccent : pFocusRing
                                border.width: 2
                                Label {
                                    anchors.centerIn: parent; text: "Rename"
                                    color: pText; font.pixelSize: 9
                                }
                                TokenHighlight { keys: ["focusRing","controlSurface","controlBorder"] }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    TokenHighlight { keys: ["surface","border","activeAccent","activeGlow","shadow"] }
                }
            }
            // Status rail + controls strip
            RowLayout {
                Layout.fillWidth: true; spacing: 8

                // Status rail - panelSurface · panelBorder · accent (bar)
                //               success · warning · danger · textSecondary
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 38; radius: 10
                    color: pStatusRail
                    border.color: h2("panelSurface","panelBorder") ? root.dialogAccent : Theme.withAlpha(pPanelBrd, 0.80)
                    border.width:  h2("panelSurface","panelBorder") ? 2 : 1

                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; spacing: 10

                        ColumnLayout {
                            spacing: 2
                            Label { text: "2 operations"; color: pTextSec; font.pixelSize: 8 }
                            Rectangle {
                                implicitWidth: 52; implicitHeight: 4; radius: 2
                                color: Theme.withAlpha(pAccent, 0.18)
                                Rectangle {
                                    width: parent.width * 0.58; height: parent.height; radius: parent.radius
                                    color: pAccent
                                    border.color: h("accent") ? root.dialogAccent : "transparent"
                                    border.width: h("accent") ? 1 : 0
                                }
                            }
                        }

                        Rectangle { implicitWidth: 1; implicitHeight: 20; color: Theme.withAlpha(pPanelBrd, 0.55) }

                        Repeater {
                            model: [
                                { label: "Copied",  key: "success", c: pSuccess  },
                                { label: "Slow",    key: "warning", c: pWarning  },
                                { label: "Deleted", key: "danger",  c: pDanger   }
                            ]
                            delegate: Rectangle {
                                implicitWidth: badgeLbl.implicitWidth + 14
                                implicitHeight: 20; radius: 10
                                color: Theme.withAlpha(modelData.c, 0.16)
                                border.color: h(modelData.key) ? root.dialogAccent : Theme.withAlpha(modelData.c, 0.40)
                                border.width: h(modelData.key) ? 2 : 1
                                Label {
                                    id: badgeLbl; anchors.centerIn: parent
                                    text: modelData.label
                                    color: h(modelData.key) ? root.dialogAccent : modelData.c
                                    font.pixelSize: 9; font.weight: Font.DemiBold
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: root.workingState.mode === "light" ? "Light" : "Dark"
                            color: pTextSec; font.pixelSize: 9
                        }
                    }
                    TokenHighlight { keys: ["panelSurface","panelBorder","statusRailFill","textSecondary"] }
                }

                // Controls strip - accent · accentText · controlSurface
                //                  controlBorder · controlSurfaceActive · textPrimary
                ColumnLayout {
                    spacing: 5

                    // Primary action - accent · accentText
                    Rectangle {
                        implicitWidth: 86; implicitHeight: 28; radius: 10
                        color: pAccent
                        Label {
                            anchors.centerIn: parent; text: "Copy here"
                            color: pAccentText; font.pixelSize: 10; font.weight: Font.DemiBold
                        }
                        TokenHighlight { keys: ["accent","accentText"] }
                    }

                    // Secondary - controlSurface · controlBorder · textPrimary
                    Rectangle {
                        implicitWidth: 86; implicitHeight: 26; radius: 9
                        color: pCtrl
                        border.color: h2("controlSurface","controlBorder") ? root.dialogAccent : Theme.withAlpha(pCtrlBrd, 0.80)
                        border.width: h2("controlSurface","controlBorder") ? 2 : 1
                        Label {
                            anchors.centerIn: parent; text: "Move"
                            color: pText; font.pixelSize: 10
                        }
                        TokenHighlight { keys: ["controlSurface","controlBorder","textPrimary"] }
                    }

                    // Pressed - controlSurfaceActive · controlBorder
                    Rectangle {
                        implicitWidth: 86; implicitHeight: 22; radius: 8
                        color: pCtrlAct
                        border.color: h2("controlSurfaceActive","controlBorder") ? root.dialogAccent : Theme.withAlpha(pCtrlBrd, 0.60)
                        border.width: h2("controlSurfaceActive","controlBorder") ? 2 : 1
                        Label {
                            anchors.centerIn: parent; text: "Delete"
                            color: pText; font.pixelSize: 9
                        }
                        TokenHighlight { keys: ["controlSurfaceActive","controlBorder"] }
                    }
                }
            }
        }
    }

    // TokenHighlight - top-level inline component of the dialog.
    // Used inside ThemePreviewCard to overlay a glow on any Rectangle
    // whose token key(s) are currently hovered in the editor panel.
    component TokenHighlight: Rectangle {
        property var keys: []
        readonly property bool active: {
            for (var i = 0; i < keys.length; ++i)
                if (root.hoveredTokenKey === keys[i] && keys[i].length > 0) return true
            return false
        }
        anchors.fill: parent
        radius: parent.radius
        color:        active ? Qt.rgba(root.dialogAccent.r, root.dialogAccent.g, root.dialogAccent.b, 0.13) : "transparent"
        border.color: active ? root.dialogAccent : "transparent"
        border.width: active ? 2 : 0
        z: 10
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

            Label {
                text: "Hover a token on the left to see which UI elements it affects. The active app theme is unchanged until you save and select a file."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 11
                color: Theme.textSecondary
            }

            ThemePreviewCard {
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
