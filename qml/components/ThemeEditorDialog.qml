import "../style"
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Effects
import QtQuick.Layouts
import "common"
import "framework"
import "dialogs"

Dialog {
    id: root

    property var initialState: ({
    })
    property var defaultDraftState: ({
    })
    property var workingState: ({
    })
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
    readonly property color tokenRowFill: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.5 : 0.7)
    readonly property color tokenRowFillHover: Theme.withAlpha(Theme.surfaceHover, themeController.isDark ? 0.46 : 0.6)
    readonly property color tokenRowBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.32 : 0.24)
    readonly property color tokenHintText: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.68 : 0.74)
    readonly property var foundationTokens: [{
        "key": "bg",
        "title": "Background",
        "hint": "Main app backdrop"
    }, {
        "key": "surface",
        "title": "Surface",
        "hint": "Primary cards and panes"
    }, {
        "key": "surfaceHover",
        "title": "Surface Hover",
        "hint": "Hover state on surfaces"
    }, {
        "key": "surfaceActive",
        "title": "Surface Active",
        "hint": "Pressed or active state"
    }, {
        "key": "border",
        "title": "Border",
        "hint": "Base outlines and dividers"
    }]
    readonly property var textTokens: [{
        "key": "textPrimary",
        "title": "Primary Text",
        "hint": "Headings and key labels"
    }, {
        "key": "textSecondary",
        "title": "Secondary Text",
        "hint": "Hints and metadata"
    }, {
        "key": "accent",
        "title": "Accent",
        "hint": "Primary brand/action color"
    }, {
        "key": "accentText",
        "title": "Accent Text",
        "hint": "Text on accent surfaces"
    }, {
        "key": "focusRing",
        "title": "Focus Ring",
        "hint": "Keyboard focus highlight"
    }]
    readonly property var interfaceTokens: [{
        "key": "panelSurface",
        "title": "Panel Surface",
        "hint": "Panel body tone"
    }, {
        "key": "panelSurfaceSoft",
        "title": "Panel Surface Soft",
        "hint": "Subtle panel tint and soft overlays"
    }, {
        "key": "panelSurfaceStrong",
        "title": "Panel Surface Strong",
        "hint": "Elevated cards and menus"
    }, {
        "key": "panelBorder",
        "title": "Panel Border",
        "hint": "Panel outlines"
    }, {
        "key": "controlSurface",
        "title": "Control Surface",
        "hint": "Inputs and buttons"
    }, {
        "key": "controlSurfaceActive",
        "title": "Control Active",
        "hint": "Pressed controls"
    }, {
        "key": "controlBorder",
        "title": "Control Border",
        "hint": "Control outline"
    }]
    readonly property var stateTokens: [{
        "key": "itemHoverFill",
        "title": "Hover Fill",
        "hint": "Hovered rows and crumbs"
    }, {
        "key": "itemCurrentFill",
        "title": "Current Fill",
        "hint": "Focused current row"
    }, {
        "key": "itemCurrentBorder",
        "title": "Current Border",
        "hint": "Focused current row outline"
    }, {
        "key": "itemSelectedFill",
        "title": "Selection Fill",
        "hint": "Selected rows and chips"
    }, {
        "key": "itemSelectedFillInactive",
        "title": "Selection Fill Inactive",
        "hint": "Selection in inactive panels"
    }, {
        "key": "itemSelectedBorder",
        "title": "Selection Border",
        "hint": "Selection outline"
    }, {
        "key": "itemSelectedBorderInactive",
        "title": "Selection Border Inactive",
        "hint": "Selection outline in inactive panels"
    }, {
        "key": "danger",
        "title": "Danger",
        "hint": "Destructive actions"
    }, {
        "key": "success",
        "title": "Success",
        "hint": "Positive states"
    }, {
        "key": "warning",
        "title": "Warning",
        "hint": "Caution and alerts"
    }, {
        "key": "activeAccent",
        "title": "Active Panel Accent",
        "hint": "Bright frame for the active panel"
    }, {
        "key": "activeGlow",
        "title": "Active Panel Glow",
        "hint": "Outer glow around the active panel"
    }, {
        "key": "statusRailFill",
        "title": "Status Rail Fill",
        "hint": "Operation rail and footer strip"
    }]
    readonly property var utilityTokens: [{
        "key": "secondaryAccent",
        "title": "Secondary Accent",
        "hint": "Secondary semantic accent"
    }, {
        "key": "warmAccent",
        "title": "Warm Accent",
        "hint": "Warm utility and theme affordances"
    }, {
        "key": "categoryInfo",
        "title": "Category Info",
        "hint": "Info and navigation accents"
    }, {
        "key": "categoryNavigation",
        "title": "Category Navigation",
        "hint": "Navigation and route accents"
    }, {
        "key": "categoryAction",
        "title": "Category Action",
        "hint": "Action and operation accents"
    }, {
        "key": "categoryUtility",
        "title": "Category Utility",
        "hint": "Utility and view accents"
    }, {
        "key": "categorySystem",
        "title": "Category System",
        "hint": "System and admin accents"
    }]
    readonly property var overlayTokens: [{
        "key": "overlayScrim",
        "title": "Overlay Scrim",
        "hint": "Modal backdrop dimming"
    }, {
        "key": "menuBorder",
        "title": "Menu Border",
        "hint": "Menu and popup outlines"
    }, {
        "key": "menuSeparator",
        "title": "Menu Separator",
        "hint": "Menu dividers"
    }, {
        "key": "menuItemPressed",
        "title": "Menu Item Pressed",
        "hint": "Pressed menu row state"
    }, {
        "key": "chromeGradientStart",
        "title": "Chrome Gradient Start",
        "hint": "Window and toolbar gradient start"
    }, {
        "key": "chromeGradientMid",
        "title": "Chrome Gradient Mid",
        "hint": "Window and toolbar gradient middle"
    }, {
        "key": "chromeGradientEnd",
        "title": "Chrome Gradient End",
        "hint": "Window and toolbar gradient end"
    }, {
        "key": "glassShadow",
        "title": "Glass Shadow",
        "hint": "Elevated popup shadow"
    }, {
        "key": "shadow",
        "title": "Base Shadow",
        "hint": "Generic soft shadow tone"
    }]

    function cloneState(value) {
        return JSON.parse(JSON.stringify(value || {
        }));
    }

    function childDialogOpen() {
        return importDialog.visible || exportDialog.visible || colorPicker.visible;
    }

    function closeEditor() {
        if (!childDialogOpen())
            close();

    }

    function refreshBuiltInDrafts() {
        builtInDrafts = themeController.builtInThemeDrafts();
        if (builtInDraftIndex < 0 || builtInDraftIndex >= builtInDrafts.length)
            builtInDraftIndex = 0;

    }

    function ensureStateShape(state) {
        const next = cloneState(state);
        next.id = next.id ? next.id.toString() : "";
        next.name = next.name ? next.name.toString() : "";
        if (!next.mode || (next.mode !== "light" && next.mode !== "dark"))
            next.mode = "dark";

        if (!next.colors)
            next.colors = {
        };

        return next;
    }

    function syncDirtyState() {
        dirty = JSON.stringify(workingState) !== JSON.stringify(initialState);
    }

    function withUpdatedState(mutator) {
        const next = ensureStateShape(workingState);
        mutator(next);
        workingState = next;
        syncDirtyState();
    }

    function setStatus(message, isError) {
        statusMessage = message;
        statusIsError = !!isError;
    }

    function loadDefaultDraft() {
        const state = ensureStateShape(themeController.defaultThemeDraft());
        defaultDraftState = cloneState(state);
        initialState = cloneState(state);
        workingState = cloneState(state);
        baselineKind = "neutral";
        setStatus("", false);
        dirty = false;
    }

    function neutralDraftStateForMode(mode) {
        const nextMode = mode === "light" ? "light" : "dark";
        return ensureStateShape(themeController.defaultThemeDraftForMode(nextMode));
    }

    function loadNeutralDraftForMode(mode, preserveIdentity) {
        const nextMode = mode === "light" ? "light" : "dark";
        const state = neutralDraftStateForMode(nextMode);
        if (preserveIdentity && workingState) {
            state.id = themeId();
            state.name = themeName();
        }
        defaultDraftState = cloneState(state);
        initialState = cloneState(state);
        workingState = cloneState(state);
        baselineKind = "neutral";
        setStatus("Switched to the neutral " + (nextMode === "light" ? "light" : "dark") + " draft preset.", false);
        dirty = false;
    }

    function loadBuiltInDraft(index) {
        if (!builtInDrafts || index < 0 || index >= builtInDrafts.length) {
            setStatus("Built-in theme preset is not available.", true);
            return ;
        }
        const source = ensureStateShape(builtInDrafts[index]);
        const editable = cloneState(source);
        const sourceId = normalizedThemeId(source.id);
        editable.id = sourceId.length > 0 ? (sourceId + "-custom") : "custom-theme";
        editable.name = (source.name && source.name.length > 0 ? source.name : "Built-in Theme") + " Custom";
        builtInDraftIndex = index;
        defaultDraftState = cloneState(source);
        initialState = cloneState(editable);
        workingState = cloneState(editable);
        baselineKind = "builtin";
        setStatus("Loaded built-in colors from " + source.name + ". Save as a separate custom theme to keep edits.", false);
        dirty = false;
    }

    function normalizedThemeId(value) {
        const compact = (value || "").toString().trim().toLowerCase();
        if (compact.length === 0)
            return "";

        const slug = compact.replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
        return slug;
    }

    function themeName() {
        return workingState && typeof workingState.name === "string" ? workingState.name : "";
    }

    function themeId() {
        return workingState && typeof workingState.id === "string" ? workingState.id : "";
    }

    function colorValue(key) {
        if (!workingState || !workingState.colors)
            return "";

        const value = workingState.colors[key];
        return value ? value.toString() : "";
    }

    function previewColor(key, fallback) {
        const value = colorValue(key);
        return value && value.length > 0 ? value : fallback;
    }

    function tokenChanged(key) {
        return colorValue(key) !== previewColorFromState(initialState, key);
    }

    function previewColorFromState(state, key) {
        if (!state || !state.colors)
            return "";

        const value = state.colors[key];
        return value ? value.toString() : "";
    }

    function changedTokenModels() {
        const groups = allEditableTokens();
        const changed = [];
        for (let i = 0; i < groups.length; ++i) {
            const token = groups[i];
            if (tokenChanged(token.key))
                changed.push(token);

        }
        return changed;
    }

    function changedTokenCount() {
        return changedTokenModels().length;
    }

    function allEditableTokens() {
        return [].concat(foundationTokens).concat(textTokens).concat(interfaceTokens).concat(stateTokens).concat(utilityTokens).concat(overlayTokens);
    }

    function tokenArea(key) {
        switch (key) {
        case "bg":
        case "surface":
        case "border":
        case "surfaceHover":
        case "surfaceActive":
            return "background";
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
            return "chrome";
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
            return "controls";
        case "itemHoverFill":
        case "itemCurrentFill":
        case "itemCurrentBorder":
        case "itemSelectedFill":
        case "itemSelectedFillInactive":
        case "itemSelectedBorder":
        case "itemSelectedBorderInactive":
        case "textPrimary":
        case "textSecondary":
            return "list";
        case "overlayScrim":
            return "background";
        case "statusRailFill":
            return "status";
        case "danger":
        case "success":
        case "warning":
            return "status";
        default:
            return "background";
        }
    }

    function tokenAreaTitle(key) {
        switch (tokenArea(key)) {
        case "background":
            return "Background";
        case "chrome":
            return "Panel Chrome";
        case "controls":
            return "Controls";
        case "list":
            return "Content List";
        case "status":
            return "Status Badges";
        default:
            return "Preview";
        }
    }

    function tokenAreaAccent(key) {
        switch (tokenArea(key)) {
        case "background":
            return Theme.categoryInfo;
        case "chrome":
            return Theme.categorySystem;
        case "controls":
            return Theme.categoryUtility;
        case "list":
            return Theme.categoryNavigation;
        case "status":
            return Theme.categoryAction;
        default:
            return root.dialogAccent;
        }
    }

    function tokenModelForKey(key) {
        const groups = allEditableTokens();
        for (let i = 0; i < groups.length; ++i) {
            if (groups[i].key === key)
                return groups[i];

        }
        return null;
    }

    function hoveredTokenTitle() {
        const token = tokenModelForKey(hoveredTokenKey);
        return token ? token.title : "";
    }

    function hoveredTokenDetail() {
        const token = tokenModelForKey(hoveredTokenKey);
        if (!token)
            return "No token focused";

        return token.key + " - " + token.hint;
    }

    function changedTokensForArea(area) {
        const groups = allEditableTokens();
        const changed = [];
        for (let i = 0; i < groups.length; ++i) {
            const token = groups[i];
            if (tokenArea(token.key) === area && tokenChanged(token.key))
                changed.push(token);

        }
        return changed;
    }

    function areaMarkerDetails(area) {
        const changed = changedTokensForArea(area);
        if (changed.length === 0) {
            switch (area) {
            case "background":
                return "Window backdrop and base surfaces";
            case "chrome":
                return "Panels, sidebar and frame";
            case "controls":
                return "Toolbar, inputs and actions";
            case "list":
                return "Content rows and preview card";
            case "status":
                return "Badges and semantic states";
            default:
                return "Preview region";
            }
        }
        let labels = "";
        for (let i = 0; i < changed.length; ++i) {
            labels += (i === 0 ? "" : ", ") + changed[i].title;
        }
        return labels;
    }

    function setThemeName(value) {
        withUpdatedState(function(next) {
            next.name = value ? value.trim() : "";
        });
    }

    function setThemeId(value) {
        withUpdatedState(function(next) {
            next.id = normalizedThemeId(value);
        });
    }

    function setThemeMode(value) {
        const nextMode = value === "light" ? "light" : "dark";
        if (baselineKind === "builtin") {
            setStatus("Built-in based drafts keep their original tone. Load a neutral draft to switch Light/Dark.", false);
            return ;
        }
        if (workingState && workingState.mode === nextMode)
            return ;

        loadNeutralDraftForMode(nextMode, true);
    }

    function setColorValue(key, value) {
        withUpdatedState(function(next) {
            next.colors[key] = (value || "").toString().trim();
        });
    }

    function resetTokenToDefault(key) {
        const baselineDefaults = baselineKind === "builtin" ? defaultDraftState : neutralDraftStateForMode(workingState && workingState.mode === "light" ? "light" : "dark");
        const fallback = previewColorFromState(baselineDefaults, key) || previewColorFromState(defaultDraftState, key);
        withUpdatedState(function(next) {
            next.colors[key] = fallback;
        });
    }

    function resetDraft() {
        workingState = cloneState(initialState);
        setStatus("Draft reset to the current editor baseline.", false);
        dirty = false;
    }

    function loadDraftFromFile(fileUrl) {
        const state = themeController.readThemeStateFromFile(fileUrl.toString());
        if (!state || !state.colors) {
            setStatus("Theme file could not be loaded into the draft editor.", true);
            return ;
        }
        initialState = ensureStateShape(state);
        defaultDraftState = neutralDraftStateForMode(initialState.mode);
        workingState = cloneState(initialState);
        baselineKind = "file";
        setStatus("Theme draft loaded from file. Active app theme was not changed.", false);
        dirty = false;
    }

    function defaultSaveFileUrl() {
        const directory = themeController.customThemeDirectory();
        const draftId = normalizedThemeId(themeId());
        const fileName = (draftId.length > 0 ? draftId : "custom-theme") + ".json";
        const nativePath = directory.length > 0 ? (directory + "/" + fileName) : fileName;
        const normalized = nativePath.replace(/\\/g, "/");
        if (/^[A-Za-z]:/.test(normalized))
            return "file:///" + normalized;

        return normalized === fileName ? normalized : "file:///" + normalized;
    }

    function themeLibraryFolderUrl() {
        const directory = themeController.customThemeDirectory();
        if (directory.length === 0)
            return "";

        const normalized = directory.replace(/\\/g, "/");
        if (/^[A-Za-z]:/.test(normalized))
            return "file:///" + normalized;

        return "file:///" + normalized;
    }

    function validateDraftForSave(fileUrl) {
        const trimmedName = themeName().trim();
        const trimmedId = normalizedThemeId(themeId());
        if (trimmedName.length === 0) {
            setStatus("Theme name is required before saving.", true);
            return false;
        }
        if (trimmedId.length === 0) {
            setStatus("Theme id is required before saving.", true);
            return false;
        }
        if (!themeController.isThemeIdAvailable(trimmedId, fileUrl.toString())) {
            setStatus("Theme id is already used by a built-in or saved custom theme.", true);
            return false;
        }
        return true;
    }

    function saveDraftToFile(fileUrl) {
        if (!validateDraftForSave(fileUrl))
            return ;

        const stateToSave = ensureStateShape(workingState);
        stateToSave.id = normalizedThemeId(themeId());
        stateToSave.name = themeName().trim();
        const saved = themeController.writeThemeStateToFile(stateToSave, fileUrl.toString());
        if (!saved) {
            setStatus("Theme file could not be saved. Check the target path and draft values.", true);
            return ;
        }
        setStatus("Theme saved. Choose it later from the theme picker.", false);
        initialState = cloneState(stateToSave);
        workingState = cloneState(stateToSave);
        baselineKind = "file";
        dirty = false;
    }

    function openPickerForToken(key, title) {
        pickerTokenKey = key;
        pickerTokenTitle = title;
        colorPicker.selectedColor = previewColor(key, Theme.accent);
        colorPicker.open();
    }

    title: "Theme Editor"
    modal: true
    closePolicy: Popup.NoAutoClose
    focus: true
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 1600, 1600)
    height: Math.min(parent ? parent.height - 32 : 980, 980)
    padding: 0
    onOpened: {
        refreshBuiltInDrafts();
        loadDefaultDraft();
        Qt.callLater(() => {
            return contentItem.forceActiveFocus();
        });
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
            if (root.pickerTokenKey.length > 0)
                root.setColorValue(root.pickerTokenKey, selectedColor.toString());

        }
    }

    background: DialogShell {
        accentColor: root.dialogAccent
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.panelBorder
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/moon.svg"
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
                importDialog.currentFolder = root.themeLibraryFolderUrl();
                importDialog.open();
            }
        }

        DialogActionButton {
            text: "Save Theme As..."
            highlighted: true
            primaryColor: root.dialogAccent
            onClicked: {
                exportDialog.currentFolder = root.themeLibraryFolderUrl();
                exportDialog.selectedFile = root.defaultSaveFileUrl();
                exportDialog.open();
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
                root.closeEditor();
                event.accepted = true;
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 20
            Layout.bottomMargin: 12
            implicitHeight: statusColumn.implicitHeight + 16
            radius: Theme.radiusSm
            color: root.statusMessage.length > 0 ? Theme.withAlpha(root.statusIsError ? Theme.danger : Theme.categoryInfo, themeController.isDark ? 0.14 : 0.1) : Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.62 : 0.84)
            border.color: root.statusMessage.length > 0 ? Theme.withAlpha(root.statusIsError ? Theme.danger : Theme.categoryInfo, 0.45) : Theme.panelBorder
            border.width: 1

            ColumnLayout {
                id: statusColumn

                anchors.fill: parent
                anchors.margins: 8
                spacing: 2

                Label {
                    text: root.statusMessage.length > 0 ? root.statusMessage : "This editor starts from a neutral blank draft. It never edits built-in themes or recolors the active app theme."
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeCaption
                    color: root.statusMessage.length > 0 ? (root.statusIsError ? Theme.danger : Theme.categoryInfo) : Theme.textSecondary
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
                ScrollBar.vertical: FmScrollBar {
                    id: editorScrollBar
                    parent: editorScrollView.contentItem
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    policy: ScrollBar.AsNeeded
                }

                Pane {
                    width: editorScrollBar.scrollNeeded
                           ? Math.max(0, editorScrollBar.x - 6)
                           : editorScrollView.availableWidth
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
                                    font.pixelSize: Theme.fontSizeCaption
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
                                    font.pixelSize: Theme.fontSizeCaption
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
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textSecondary
                                }

                                RowLayout {
                                    spacing: 8

                                    ThemeEditorModePill {
                                        title: "Dark"
                                        selected: root.workingState.mode !== "light"
                                        enabled: root.baselineKind !== "builtin"
                                        accentColor: root.dialogAccent
                                        onClicked: root.setThemeMode("dark")
                                    }

                                    ThemeEditorModePill {
                                        title: "Light"
                                        selected: root.workingState.mode === "light"
                                        enabled: root.baselineKind !== "builtin"
                                        accentColor: root.dialogAccent
                                        onClicked: root.setThemeMode("light")
                                    }

                                }

                                Label {
                                    text: "Built-in base"
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textSecondary
                                    Layout.topMargin: 4
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    FmComboBox {
                                        id: builtInBaseCombo

                                        Layout.fillWidth: true
                                        implicitHeight: 30
                                        font.pixelSize: Theme.fontSizeCaption
                                        model: root.builtInDrafts
                                        textRole: "name"
                                        valueRole: "id"
                                        currentIndex: root.builtInDraftIndex
                                        onActivated: (index) => {
                                            return root.builtInDraftIndex = index;
                                        }

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
                                                    color: modelData && modelData.colors && modelData.colors.accent ? modelData.colors.accent : Theme.accent
                                                    border.color: Theme.withAlpha(Theme.textPrimary, 0.18)
                                                    border.width: 1
                                                }

                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 0

                                                    Label {
                                                        text: modelData && modelData.name ? modelData.name : ""
                                                        color: builtInBaseCombo.highlightedIndex === index ? Theme.accent : Theme.textPrimary
                                                        font.pixelSize: Theme.fontSizeCaption
                                                        font.weight: builtInBaseCombo.currentIndex === index ? Font.DemiBold : Font.Normal
                                                        Layout.fillWidth: true
                                                        elide: Text.ElideRight
                                                    }

                                                    Label {
                                                        text: modelData && modelData.subtitle ? modelData.subtitle : ""
                                                        color: Theme.textSecondary
                                                        font.pixelSize: Theme.scaledSize(9)
                                                        Layout.fillWidth: true
                                                        elide: Text.ElideRight
                                                        visible: text.length > 0
                                                    }

                                                }

                                            }

                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: builtInBaseCombo.highlightedIndex === index ? Theme.menuItemHover : (builtInBaseCombo.currentIndex === index ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.13 : 0.09) : "transparent")
                                                border.color: builtInBaseCombo.currentIndex === index ? Theme.withAlpha(Theme.accent, 0.34) : "transparent"
                                                border.width: builtInBaseCombo.currentIndex === index ? 1 : 0
                                            }

                                        }

                                    }

                                    FmIconButton {
                                        id: loadBuiltInButton

                                        Layout.preferredWidth: 32
                                        Layout.preferredHeight: 32
                                        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-right.svg"
                                        iconTone: "accent"
                                        iconSize: 16
                                        isHighlighted: true
                                        enabled: root.builtInDrafts.length > 0
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Load selected theme"
                                        Accessible.name: "Load selected theme"
                                        onClicked: root.loadBuiltInDraft(root.builtInDraftIndex)

                                    }

                                }

                                Label {
                                    visible: root.baselineKind === "builtin"
                                    text: "Tone is locked to the source theme; save creates a separate custom JSON."
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: Theme.fontSizeMicro
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

                                            delegate: ThemeEditorTokenRow {
                                                editor: root
                                            }

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

                                            delegate: ThemeEditorTokenRow {
                                                editor: root
                                            }

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

                                            delegate: ThemeEditorTokenRow {
                                                editor: root
                                            }

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

                                            delegate: ThemeEditorTokenRow {
                                                editor: root
                                            }

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

                                            delegate: ThemeEditorTokenRow {
                                                editor: root
                                            }

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

                                            delegate: ThemeEditorTokenRow {
                                                editor: root
                                            }

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

                                        delegate: ThemeEditorTokenRow {
                                            editor: root
                                        }

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

                                        delegate: ThemeEditorTokenRow {
                                            editor: root
                                        }

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

                                        delegate: ThemeEditorTokenRow {
                                            editor: root
                                        }

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

                                        delegate: ThemeEditorTokenRow {
                                            editor: root
                                        }

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

                                        delegate: ThemeEditorTokenRow {
                                            editor: root
                                        }

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

                                        delegate: ThemeEditorTokenRow {
                                            editor: root
                                        }

                                    }

                                }

                            }

                        }

                        ColumnLayout {
                            visible: root.compactLayout
                            Layout.fillWidth: true
                            spacing: 14

                            ThemeEditorPreviewSection {
                                editor: root
                                Layout.fillWidth: true
                                Layout.preferredHeight: 500
                            }

                            ThemeEditorSaveTargetSection {
                                editor: root
                            }

                        }

                    }

                }

            }

            ColumnLayout {
                visible: !root.compactLayout
                SplitView.preferredWidth: root.compactLayout ? 0 : (root.wideTokenLayout ? 860 : 720)
                SplitView.minimumWidth: root.compactLayout ? 0 : 500
                SplitView.maximumWidth: root.compactLayout ? 0 : 1.67772e+07
                SplitView.fillHeight: true
                spacing: 14

                ThemeEditorPreviewSection {
                    editor: root
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                ThemeEditorSaveTargetSection {
                    editor: root
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

                    Behavior on opacity {
                        NumberAnimation {
                            duration: 150
                        }

                    }

                }

                // The actual divider line
                Rectangle {
                    anchors.centerIn: parent
                    width: (SplitHandle.hovered || SplitHandle.pressed) ? 2 : 1
                    height: parent.height - 12
                    radius: 1
                    color: (SplitHandle.hovered || SplitHandle.pressed) ? root.dialogAccent : Theme.border
                    opacity: (SplitHandle.hovered || SplitHandle.pressed) ? 1 : 0.4

                    Behavior on width {
                        NumberAnimation {
                            duration: 100
                        }

                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }

                    }

                    Behavior on opacity {
                        NumberAnimation {
                            duration: 150
                        }

                    }

                }

            }

        }

    }

}
