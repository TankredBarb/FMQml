import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../style"
import "dialogs"

Dialog {
    id: root

    title: "Settings"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 48 : 680, 680)
    height: Math.min(parent ? parent.height - 48 : 560, 560)
    padding: 0

    property var appRoot: null
    property bool workspaceResetPending: false
    property bool splitViewEnabled: false
    property bool previewPaneEnabled: false
    property bool hiddenFilesEnabled: false
    property bool nativeIconsEnabled: true
    property bool highQualitySystemIconsEnabled: true
    property bool thumbnailsEnabled: true
    property bool ultraLightModeEnabled: false
    property bool shellFirstQmlRestoreEnabled: false
    property bool systemTrayIconEnabled: false
    property bool allowOnlyOneInstanceEnabled: false
    property string fontFamilyValue: typeof appSettings !== "undefined" && appSettings
                                     ? appSettings.fontFamily
                                     : ""
    property int fontScaleValue: typeof appSettings !== "undefined" && appSettings
                                 ? appSettings.fontScale
                                 : 100
    property var fontFamilyOptions: []
    property bool googleDriveAuthorized: false
    signal themeEditorRequested()
    signal pluginManagerRequested()
    readonly property string appDataLocation: typeof appSettings !== "undefined" && appSettings
                                              ? appSettings.appDataLocation
                                              : ""
    readonly property string maintenanceStatus: typeof appSettings !== "undefined" && appSettings
                                                ? appSettings.settingsMaintenanceStatus
                                                : ""
    readonly property int settingsFormatVersion: typeof appSettings !== "undefined" && appSettings
                                                 ? appSettings.settingsFormatVersion
                                                 : 0
    readonly property bool maintenanceStatusIsError: maintenanceStatus.toLowerCase().indexOf("failed") >= 0
    readonly property color dialogAccent: Theme.accent
    readonly property color sectionFill: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.30 : 0.56)
    readonly property color sectionBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.24)
    readonly property color rowFill: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.30 : 0.52)
    readonly property color rowFillHover: Theme.withAlpha(Theme.surfaceHover, themeController.isDark ? 0.42 : 0.58)
    readonly property color rowBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.26 : 0.20)
    readonly property color detailText: Theme.readableOn(Theme.panelSurface, Theme.textSecondary)

    onOpened: {
        workspaceResetPending = false
        refreshState()
        refreshGoogleDriveAuthorization()
        Qt.callLater(() => contentItem.forceActiveFocus())
    }

    function workspace() {
        return typeof workspaceController !== "undefined" ? workspaceController : null
    }

    function displayPath(path) {
        if (!path || String(path).length === 0) {
            return ""
        }
        const workspaceCtrl = workspace()
        if (workspaceCtrl && workspaceCtrl.displayPath) {
            return workspaceCtrl.displayPath(String(path))
        }
        const value = String(path)
        if (value.indexOf("archive://") === 0 || value.indexOf("devices://") === 0 || value.indexOf("favorites://") === 0) {
            return value
        }
        return Qt.platform.os === "windows" ? value.replace(/\//g, "\\") : value
    }

    function refreshState() {
        const workspaceCtrl = workspace()
        splitViewEnabled = workspaceCtrl ? workspaceCtrl.splitEnabled : false
        previewPaneEnabled = root.appRoot ? root.appRoot.previewPaneVisible : false
        hiddenFilesEnabled = workspaceCtrl && workspaceCtrl.leftPanel
                             ? workspaceCtrl.leftPanel.directoryModel.showHidden
                             : false
        nativeIconsEnabled = typeof appSettings !== "undefined" && appSettings
                             ? appSettings.useNativeIcons
                             : true
        highQualitySystemIconsEnabled = typeof appSettings !== "undefined" && appSettings
                                        ? appSettings.useHighQualitySystemIcons
                                        : true
        thumbnailsEnabled = typeof appSettings !== "undefined" && appSettings
                            ? appSettings.showThumbnails
                            : true
        ultraLightModeEnabled = typeof appSettings !== "undefined" && appSettings
                                ? appSettings.ultraLightMode
                                : false
        shellFirstQmlRestoreEnabled = typeof appSettings !== "undefined" && appSettings
                                      ? appSettings.shellFirstQmlRestore
                                      : false
        systemTrayIconEnabled = typeof appSettings !== "undefined" && appSettings
                                ? appSettings.useSystemTrayIcon
                                : false
        allowOnlyOneInstanceEnabled = typeof appSettings !== "undefined" && appSettings
                                      ? appSettings.allowOnlyOneInstance
                                      : false
        fontFamilyValue = typeof appSettings !== "undefined" && appSettings
                          ? appSettings.fontFamily
                          : ""
        fontScaleValue = typeof appSettings !== "undefined" && appSettings
                         ? appSettings.fontScale
                         : 100
        rebuildFontFamilyOptions()
    }

    function rebuildFontFamilyOptions() {
        const options = [
            { label: "System default", value: "" }
        ]
        const available = typeof appSettings !== "undefined" && appSettings
                          ? appSettings.availableFontFamilies
                          : []
        let currentFound = fontFamilyValue.length === 0
        for (let i = 0; i < available.length; ++i) {
            const family = String(available[i])
            options.push({ label: family, value: family })
            if (family === fontFamilyValue) {
                currentFound = true
            }
        }
        if (!currentFound && fontFamilyValue.length > 0) {
            options.push({ label: fontFamilyValue + " (Unavailable here)", value: fontFamilyValue })
        }
        fontFamilyOptions = options
        Qt.callLater(syncFontFamilySelection)
    }

    function syncFontFamilySelection() {
        if (!fontFamilyCombo) {
            return
        }
        for (let i = 0; i < fontFamilyOptions.length; ++i) {
            if (String(fontFamilyOptions[i].value) === fontFamilyValue) {
                if (fontFamilyCombo.currentIndex !== i) {
                    fontFamilyCombo.currentIndex = i
                }
                return
            }
        }
        fontFamilyCombo.currentIndex = 0
    }

    function setSplitViewEnabled(enabled) {
        const workspaceCtrl = workspace()
        splitViewEnabled = enabled
        if (workspaceCtrl && workspaceCtrl.splitEnabled !== enabled) {
            workspaceCtrl.splitEnabled = enabled
        }
    }

    function setPreviewPaneEnabled(enabled) {
        previewPaneEnabled = enabled
        if (root.appRoot && root.appRoot.previewPaneVisible !== enabled) {
            root.appRoot.setPreviewPaneVisible(enabled)
        }
    }

    function setHiddenFilesEnabled(enabled) {
        const workspaceCtrl = workspace()
        hiddenFilesEnabled = enabled
        if (!workspaceCtrl) {
            return
        }
        workspaceCtrl.leftPanel.directoryModel.showHidden = enabled
        workspaceCtrl.rightPanel.directoryModel.showHidden = enabled
        workspaceCtrl.treeModel.showHidden = enabled
    }

    function setNativeIconsEnabled(enabled) {
        nativeIconsEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings && appSettings.useNativeIcons !== enabled) {
            appSettings.useNativeIcons = enabled
        }
    }

    function setThumbnailsEnabled(enabled) {
        thumbnailsEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings && appSettings.showThumbnails !== enabled) {
            appSettings.showThumbnails = enabled
        }
    }

    function setHighQualitySystemIconsEnabled(enabled) {
        highQualitySystemIconsEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.useHighQualitySystemIcons !== enabled) {
            appSettings.useHighQualitySystemIcons = enabled
        }
    }

    function setUltraLightModeEnabled(enabled) {
        ultraLightModeEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.ultraLightMode !== enabled) {
            appSettings.ultraLightMode = enabled
        }
    }

    function setShellFirstQmlRestoreEnabled(enabled) {
        shellFirstQmlRestoreEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.shellFirstQmlRestore !== enabled) {
            appSettings.shellFirstQmlRestore = enabled
        }
    }

    function setSystemTrayIconEnabled(enabled) {
        systemTrayIconEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.useSystemTrayIcon !== enabled) {
            appSettings.useSystemTrayIcon = enabled
        }
    }

    function setAllowOnlyOneInstanceEnabled(enabled) {
        allowOnlyOneInstanceEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.allowOnlyOneInstance !== enabled) {
            appSettings.allowOnlyOneInstance = enabled
        }
    }

    function setFontFamily(family) {
        const normalized = String(family || "")
        fontFamilyValue = normalized
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.fontFamily !== normalized) {
            appSettings.fontFamily = normalized
        }
    }

    function setFontScale(scale) {
        const normalized = Math.max(90, Math.min(150, Math.round(Number(scale))))
        fontScaleValue = normalized
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.fontScale !== normalized) {
            appSettings.fontScale = normalized
        }
    }

    function resetFontSettings() {
        setFontFamily("")
        setFontScale(100)
        rebuildFontFamilyOptions()
    }

    function defaultSettingsExportPath() {
        const base = appDataLocation && appDataLocation.length > 0 ? appDataLocation : ""
        const nativePath = base.length > 0 ? (base + "/fm-settings.json") : "fm-settings.json"
        const normalized = nativePath.replace(/\\/g, "/")
        if (/^[A-Za-z]:/.test(normalized)) {
            return "file:///" + normalized
        }
        return normalized === "fm-settings.json" ? normalized : "file:///" + normalized
    }

    function openImportDialog() {
        importDialog.open()
    }

    function openExportDialog() {
        exportDialog.selectedFile = defaultSettingsExportPath()
        exportDialog.open()
    }

    function exportSettingsToFile(fileUrl) {
        if (typeof appSettings === "undefined" || !appSettings) {
            return false
        }
        if (root.appRoot) {
            root.appRoot.saveWorkspaceStateNow(true)
        }
        return appSettings.exportSettings(fileUrl.toString())
    }

    function importSettingsFromFile(fileUrl) {
        if (typeof appSettings === "undefined" || !appSettings) {
            return false
        }
        const imported = appSettings.importSettings(fileUrl.toString())
        if (imported && root.appRoot) {
            root.appRoot.restoreWorkspaceState()
        }
        return imported
    }

    function openDataFolder() {
        if (typeof appSettings === "undefined" || !appSettings) {
            return
        }
        appSettings.openAppDataFolder()
    }

    function openThemeEditor() {
        themeEditorRequested()
    }

    function openPluginManager() {
        pluginManagerRequested()
    }

    function refreshGoogleDriveAuthorization() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            googleDriveAuthorized = false
            return
        }
        const result = pluginActionController.triggerAction("fm.gdrive-provider::authStatus", {})
        googleDriveAuthorized = !!(result && result.ok === true && result.signedIn === true)
    }

    function signOutGoogleDrive() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            if (root.appRoot) root.appRoot.showTransientInfo("Plugin controller is unavailable.")
            return
        }
        const result = pluginActionController.triggerAction("fm.gdrive-provider::signOut", {})
        root.refreshGoogleDriveAuthorization()
        if (root.appRoot) {
            root.appRoot.showTransientInfo(String(result.message || "Google Drive sign out requested."))
        }
    }

    function logInGoogleDrive() {
        const panel = root.appRoot && root.appRoot.activePanelController ? root.appRoot.activePanelController() : null
        if (panel && panel.openPath) {
            panel.openPath("gdrive://")
            root.close()
            return
        }
        if (root.appRoot) root.appRoot.showTransientInfo("No active panel is available.")
    }

    background: DialogShell {
        accentColor: root.dialogAccent
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
        shadowBlur: 16
        shadowVerticalOffset: 5
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/settings.svg"
        iconTint: root.dialogAccent
        accentColor: root.dialogAccent
        title: root.title
        subtitle: "Workspace, panels, theme, and persistence"
        closeText: "x"
        onCloseRequested: root.accept()
    }

    footer: DialogFooter {
        Item {
            Layout.fillWidth: true
        }

        DialogActionButton {
            text: "Close"
            highlighted: true
            primaryColor: root.dialogAccent
            onClicked: root.accept()
        }
    }

    contentItem: ColumnLayout {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        spacing: 0
        clip: true
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.accept()
                event.accepted = true
            }
        }

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: ScrollBar {
                id: verticalScrollBar
                policy: ScrollBar.AsNeeded
                interactive: true
                width: 8

                background: Item {
                    implicitWidth: 8
                }

                contentItem: Rectangle {
                    implicitWidth: 4
                    radius: 2
                    color: Theme.withAlpha(Theme.textSecondary,
                                           verticalScrollBar.pressed ? 0.46
                                                                     : (verticalScrollBar.active ? 0.30 : 0.18))
                }
            }

            Pane {
                width: scrollView.availableWidth
                padding: 16
                background: null

                ColumnLayout {
                    width: parent.width
                    spacing: 12

                    DialogSection {
                        title: "WORKSPACE"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder
                        radiusSize: Theme.radiusMd

                        SettingsToggleRow {
                            title: "Split view"
                            subtitle: "Show the second file panel"
                            checked: root.splitViewEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setSplitViewEnabled(checked)
                        }

                        SettingsToggleRow {
                            title: "Preview pane"
                            subtitle: "Keep the right preview pane visible"
                            checked: root.previewPaneEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setPreviewPaneEnabled(checked)
                        }

                    }

                    DialogSection {
                        title: "APP"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder
                        radiusSize: Theme.radiusMd

                        SettingsToggleRow {
                            title: "Use system tray icon"
                            subtitle: "Keep FM running in the notification area when the window is closed"
                            checked: root.systemTrayIconEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setSystemTrayIconEnabled(checked)
                        }

                        SettingsToggleRow {
                            title: "Allow only 1 instance"
                            subtitle: "Show a short splash and close when FM is already running"
                            checked: root.allowOnlyOneInstanceEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setAllowOnlyOneInstanceEnabled(checked)
                        }

                        SettingsContentBlock {
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: "Plugins"
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    Label {
                                        text: "View loaded provider/action plugins and load plugin files for this session."
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: root.detailText
                                    }
                                }

                                DialogActionButton {
                                    text: "Manage"
                                    highlighted: false
                                    secondaryTextColor: root.dialogAccent
                                    onClicked: root.openPluginManager()
                                }
                            }
                        }

                        SettingsContentBlock {
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: "Google Drive"
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    Label {
                                        text: "Authorization is kept in Windows Credential Manager until you sign out."
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: root.detailText
                                    }
                                }

                                DialogActionButton {
                                    text: root.googleDriveAuthorized ? "Sign out" : "Log in"
                                    highlighted: false
                                    secondaryTextColor: root.googleDriveAuthorized ? Theme.danger : root.dialogAccent
                                    onClicked: root.googleDriveAuthorized ? root.signOutGoogleDrive() : root.logInGoogleDrive()
                                }
                            }
                        }
                    }

                    DialogSection {
                        title: "TYPOGRAPHY"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder
                        radiusSize: Theme.radiusMd

                        SettingsContentBlock {
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Label {
                                            text: "Font family"
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeLabel
                                            font.weight: Font.DemiBold
                                            color: Theme.textPrimary
                                        }

                                        Label {
                                            text: "Apply one UI font across the app."
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeCaption
                                            color: root.detailText
                                        }
                                    }

                                    DialogActionButton {
                                        text: "Reset"
                                        highlighted: false
                                        secondaryTextColor: root.dialogAccent
                                        onClicked: root.resetFontSettings()
                                    }
                                }

                                SettingsComboBox {
                                    id: fontFamilyCombo
                                    Layout.fillWidth: true
                                    model: root.fontFamilyOptions
                                    textRole: "label"
                                    valueRole: "value"
                                    onActivated: root.setFontFamily(String(currentValue || ""))
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Label {
                                        text: "Scale"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    Label {
                                        text: root.fontScaleValue + "%"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                        color: root.dialogAccent
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }
                                }

                                Slider {
                                    id: fontScaleSlider
                                    Layout.fillWidth: true
                                    from: 90
                                    to: 150
                                    stepSize: 5
                                    snapMode: Slider.SnapAlways
                                    value: root.fontScaleValue
                                    onMoved: root.setFontScale(value)

                                    background: Item {
                                        implicitHeight: 20

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            height: 4
                                            radius: 2
                                            color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.36 : 0.62)
                                        }

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: fontScaleSlider.visualPosition * parent.width
                                            height: 4
                                            radius: 2
                                            color: root.dialogAccent
                                        }
                                    }

                                    handle: Rectangle {
                                        x: fontScaleSlider.leftPadding + fontScaleSlider.visualPosition * (fontScaleSlider.availableWidth - width)
                                        y: fontScaleSlider.topPadding + fontScaleSlider.availableHeight / 2 - height / 2
                                        width: 12
                                        height: 12
                                        radius: 6
                                        color: fontScaleSlider.pressed ? root.dialogAccent : Theme.panelSurface
                                        border.color: root.dialogAccent
                                        border.width: 1
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: previewColumn.implicitHeight + 16
                                    radius: Theme.radiusSm
                                    color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.44 : 0.62)
                                    border.color: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.24 : 0.20)
                                    border.width: 1

                                    ColumnLayout {
                                        id: previewColumn
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 2

                                        Label {
                                            text: "Preview"
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeTitle
                                            font.weight: Font.DemiBold
                                            color: Theme.textPrimary
                                        }

                                        Label {
                                            text: "Folders, files, and dialogs should stay readable at your selected scale."
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeBodyLarge
                                            color: Theme.textPrimary
                                        }

                                        Label {
                                            text: "Caption text uses the same typography system."
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeCaption
                                            color: Theme.textSecondary
                                        }
                                    }
                                }
                            }
                        }

                    }

                    DialogSection {
                        title: "FILES"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder
                        radiusSize: Theme.radiusMd

                        SettingsToggleRow {
                            title: "Hidden files"
                            subtitle: "Show hidden entries in panels and folder tree"
                            checked: root.hiddenFilesEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setHiddenFilesEnabled(checked)
                        }
                    }

                    DialogSection {
                        title: "PERFORMANCE"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder
                        radiusSize: Theme.radiusMd

                        SettingsToggleRow {
                            title: "Native icons"
                            subtitle: "Use Windows Shell icons instead of bundled file type icons"
                            checked: root.nativeIconsEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setNativeIconsEnabled(checked)
                        }

                        SettingsToggleRow {
                            title: "Use high quality system icons"
                            subtitle: "Request larger Windows Shell icons for big icon views to avoid scaling artifacts"
                            checked: root.highQualitySystemIconsEnabled
                            toggleEnabled: root.nativeIconsEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setHighQualitySystemIconsEnabled(checked)
                        }

                        SettingsToggleRow {
                            title: "Thumbnails"
                            subtitle: "Show generated previews in Grid and Brief views"
                            checked: root.thumbnailsEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setThumbnailsEnabled(checked)
                        }

                        SettingsToggleRow {
                            title: "Ultra light mode"
                            subtitle: "Use lightweight preview, disable thumbnails, and reduce decorative effects"
                            checked: root.ultraLightModeEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setUltraLightModeEnabled(checked)
                        }

                        SettingsToggleRow {
                            title: "Shell-first startup"
                            subtitle: "Show the main shell before QML layout restore; applies after restart"
                            checked: root.shellFirstQmlRestoreEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setShellFirstQmlRestoreEnabled(checked)
                        }

                    }

                    DialogSection {
                        title: "THEMES"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder
                        radiusSize: Theme.radiusMd

                        SettingsContentBlock {
                            Label {
                                text: "Theme Editor"
                                Layout.fillWidth: true
                                font.pixelSize: Theme.fontSizeLabel
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                            }

                            Label {
                                text: "Theme Editor starts from a neutral blank draft, never edits built-in themes, and saves separate custom files that later appear in the theme picker."
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: Theme.fontSizeLabel
                                color: root.detailText
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                DialogActionButton {
                                    text: "Open Theme Editor"
                                    highlighted: false
                                    secondaryTextColor: root.dialogAccent
                                    onClicked: root.openThemeEditor()
                                }

                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }

                    DialogSection {
                        title: "SETTINGS AND STATE"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder
                        radiusSize: Theme.radiusMd

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            SettingsContentBlock {
                                Label {
                                    text: "Settings file"
                                    Layout.fillWidth: true
                                    font.pixelSize: Theme.fontSizeLabel
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: "One settings file includes window geometry, both panels, split layout, preview state, theme, app preferences, and command palette history."
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: Theme.fontSizeLabel
                                    color: root.detailText
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    DialogActionButton {
                                        text: "Export settings"
                                        highlighted: false
                                        secondaryTextColor: root.dialogAccent
                                        onClicked: root.openExportDialog()
                                    }

                                    DialogActionButton {
                                        text: "Import settings"
                                        highlighted: false
                                        secondaryTextColor: root.dialogAccent
                                        onClicked: root.openImportDialog()
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }
                                }

                                Label {
                                    text: "Import applies the saved workspace, panel modes, theme, and preferences to the current session."
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: root.detailText
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: statusLayout.implicitHeight + 16
                                radius: Theme.radiusSm
                                color: root.maintenanceStatus.length > 0
                                       ? Theme.withAlpha(root.maintenanceStatusIsError ? Theme.danger : Theme.success,
                                                         themeController.isDark ? 0.10 : 0.07)
                                       : root.rowFill
                                border.color: root.maintenanceStatus.length > 0
                                              ? Theme.withAlpha(root.maintenanceStatusIsError ? Theme.danger : Theme.success, 0.32)
                                              : root.rowBorder
                                border.width: 1

                                ColumnLayout {
                                    id: statusLayout
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    Label {
                                        text: root.maintenanceStatus.length > 0
                                              ? root.maintenanceStatus
                                              : "Export creates a portable backup. Import restores it immediately."
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.weight: root.maintenanceStatus.length > 0 ? Font.DemiBold : Font.Normal
                                        color: root.maintenanceStatus.length > 0
                                               ? (root.maintenanceStatusIsError ? Theme.danger : Theme.success)
                                               : root.detailText
                                    }

                                    Label {
                                        text: "Settings format v" + root.settingsFormatVersion
                                        visible: root.settingsFormatVersion > 0
                                        Layout.fillWidth: true
                                        font.pixelSize: Theme.fontSizeMicro
                                        color: root.detailText
                                    }
                                }
                            }

                            SettingsContentBlock {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Label {
                                            text: "Reset saved workspace"
                                            font.pixelSize: Theme.fontSizeLabel
                                            font.weight: Font.DemiBold
                                            color: Theme.textPrimary
                                        }

                                        Label {
                                            text: root.workspaceResetPending
                                                  ? "Saved workspace and theme will reset on the next launch. The current session keeps running as-is until restart."
                                                  : "Clear saved workspace state and return to the default theme on the next launch. Current session and other preferences are kept."
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            font.pixelSize: Theme.fontSizeCaption
                                            color: root.workspaceResetPending ? Theme.success : root.detailText
                                        }
                                    }

                                    DialogActionButton {
                                        text: "Reset"
                                        highlighted: false
                                        enabled: !root.workspaceResetPending
                                        secondaryTextColor: root.workspaceResetPending ? root.detailText : root.dialogAccent
                                        onClicked: {
                                            if (root.appRoot) {
                                                root.appRoot.resetSavedWorkspaceState()
                                                root.workspaceResetPending = true
                                            }
                                        }
                                    }
                                }
                            }

                            SettingsContentBlock {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Label {
                                            text: "Command palette history"
                                            font.pixelSize: Theme.fontSizeLabel
                                            font.weight: Font.DemiBold
                                            color: Theme.textPrimary
                                        }

                                        Label {
                                            text: "Clear recent and frequent command ranking data. Commands stay available and future usage will build a fresh history."
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            font.pixelSize: Theme.fontSizeCaption
                                            color: root.detailText
                                        }
                                    }

                                    DialogActionButton {
                                        text: "Clear"
                                        highlighted: false
                                        secondaryTextColor: root.dialogAccent
                                        onClicked: {
                                            if (root.appRoot) {
                                                root.appRoot.resetCommandUsageStats()
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: Theme.withAlpha(Theme.border, 0.55)
                                radius: 0.5
                            }

                            SettingsContentBlock {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Label {
                                            text: "App data folder"
                                            font.pixelSize: Theme.fontSizeLabel
                                            font.weight: Font.DemiBold
                                            color: Theme.textPrimary
                                        }

                                        Label {
                                            text: root.appDataLocation.length > 0 ? root.displayPath(root.appDataLocation) : "App data path is not available."
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            font.pixelSize: Theme.fontSizeCaption
                                            color: root.detailText
                                        }
                                    }

                                    DialogActionButton {
                                        text: "Open folder"
                                        highlighted: false
                                        secondaryTextColor: root.dialogAccent
                                        onClicked: root.openDataFolder()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: root.workspace()
        function onSplitEnabledChanged() {
            root.splitViewEnabled = root.workspace() ? root.workspace().splitEnabled : false
        }
    }

    Connections {
        target: root.appRoot
        function onPreviewPaneVisibleChanged() {
            root.previewPaneEnabled = root.appRoot ? root.appRoot.previewPaneVisible : false
        }
    }

    Connections {
        target: root.workspace() && root.workspace().leftPanel
                ? root.workspace().leftPanel.directoryModel
                : null
        function onShowHiddenChanged() {
            root.hiddenFilesEnabled = root.workspace() && root.workspace().leftPanel
                                      ? root.workspace().leftPanel.directoryModel.showHidden
                                      : false
        }
    }

    Connections {
        target: typeof appSettings !== "undefined" ? appSettings : null
        function onUseNativeIconsChanged() {
            root.nativeIconsEnabled = appSettings ? appSettings.useNativeIcons : true
        }
        function onUseHighQualitySystemIconsChanged() {
            root.highQualitySystemIconsEnabled = appSettings ? appSettings.useHighQualitySystemIcons : true
        }
        function onShowThumbnailsChanged() {
            root.thumbnailsEnabled = appSettings ? appSettings.showThumbnails : true
        }
        function onUltraLightModeChanged() {
            root.ultraLightModeEnabled = appSettings ? appSettings.ultraLightMode : false
        }
        function onShellFirstQmlRestoreChanged() {
            root.shellFirstQmlRestoreEnabled = appSettings ? appSettings.shellFirstQmlRestore : false
        }
        function onUseSystemTrayIconChanged() {
            root.systemTrayIconEnabled = appSettings ? appSettings.useSystemTrayIcon : false
        }
        function onAllowOnlyOneInstanceChanged() {
            root.allowOnlyOneInstanceEnabled = appSettings ? appSettings.allowOnlyOneInstance : false
        }
        function onFontFamilyChanged() {
            root.fontFamilyValue = appSettings ? appSettings.fontFamily : ""
            root.rebuildFontFamilyOptions()
        }
        function onFontScaleChanged() {
            root.fontScaleValue = appSettings ? appSettings.fontScale : 100
        }
        function onSettingsMaintenanceStatusChanged() {
            if (root.workspaceResetPending && appSettings
                    && appSettings.settingsMaintenanceStatus.indexOf("cleared") === -1) {
                root.workspaceResetPending = false
            }
        }
    }

    FileDialog {
        id: importDialog
        title: "Import Settings"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Settings files (*.json)", "JSON files (*.json)"]
        onAccepted: root.importSettingsFromFile(selectedFile)
    }

    FileDialog {
        id: exportDialog
        title: "Export Settings"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["Settings files (*.json)", "JSON files (*.json)"]
        onAccepted: root.exportSettingsToFile(selectedFile)
    }

    component SettingsToggleRow: Rectangle {
        id: row

        property string title: ""
        property string subtitle: ""
        property bool checked: false
        property bool toggleEnabled: true
        property color accentColor: Theme.accent
        readonly property color titleColor: Theme.textPrimary
        readonly property color subtitleColor: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.74 : 0.82)
        signal toggled(bool checked)

        Layout.fillWidth: true
        implicitHeight: Math.max(48, rowLayout.implicitHeight + 10)
        radius: Theme.radiusSm
        color: rowMouse.containsMouse ? root.rowFillHover : root.rowFill
        border.color: Theme.withAlpha(row.accentColor,
                                      row.checked
                                      ? (themeController.isDark ? 0.40 : 0.34)
                                      : (themeController.isDark ? 0.26 : 0.24))
        border.width: 1
        opacity: 1.0

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 7
            width: 2
            radius: 1
            opacity: row.checked ? 1.0 : 0.58
            color: Theme.withAlpha(row.accentColor, themeController.isDark ? 0.86 : 0.72)
        }

        RowLayout {
            id: rowLayout
            anchors.fill: parent
            anchors.leftMargin: row.checked ? 14 : 10
            anchors.rightMargin: 10
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: row.title
                    Layout.fillWidth: true
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                    color: row.titleColor
                    elide: Text.ElideRight
                }

                Label {
                    text: row.subtitle
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    color: row.subtitleColor
                }
            }

            Switch {
                id: switchControl
                checked: row.checked
                enabled: row.toggleEnabled
                Layout.preferredWidth: 46
                Layout.preferredHeight: 26

                indicator: Rectangle {
                    implicitWidth: 40
                    implicitHeight: 22
                    x: switchControl.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: height / 2
                    color: switchControl.checked
                           ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.34 : 0.22)
                           : Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.82 : 0.92)
                    border.color: switchControl.checked
                                  ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.62 : 0.44)
                                  : root.rowBorder
                    border.width: 1
                    opacity: row.toggleEnabled ? 1.0 : 0.62

                    Rectangle {
                        x: switchControl.checked ? parent.width - width - 3 : 3
                        anchors.verticalCenter: parent.verticalCenter
                        width: 15
                        height: 15
                        radius: 7.5
                        color: switchControl.checked ? row.accentColor : Theme.withAlpha(Theme.textSecondary, 0.78)

                        Behavior on x {
                            NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
                        }
                    }
                }

                contentItem: Item {}
            }
        }

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            enabled: row.toggleEnabled
            cursorShape: row.toggleEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: row.toggled(!row.checked)
        }
    }

    component SettingsComboBox: ComboBox {
        id: combo

        delegate: ItemDelegate {
            width: combo.width
            height: Math.max(34, Theme.controlHeight - 4)
            highlighted: combo.highlightedIndex === index

            contentItem: Label {
                text: modelData.label
                color: highlighted ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel
                font.weight: highlighted ? Font.DemiBold : Font.Normal
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                leftPadding: 8
            }

            background: Rectangle {
                radius: Theme.radiusSm
                color: highlighted || hovered ? Theme.menuItemHover : "transparent"
            }
        }

        indicator: RecolorSvgIcon {
            x: combo.width - width - 10
            y: Math.round((combo.height - height) / 2)
            width: 10
            height: 10
            sourcePath: "../assets/icons/arrow-up.svg"
            sourceSize: Qt.size(16, 16)
            recolorEnabled: true
            recolorColor: Theme.textSecondary
            rotation: combo.opened ? 0 : 180
            opacity: 0.72
        }

        contentItem: Label {
            leftPadding: 10
            rightPadding: 24
            text: combo.displayText
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.Medium
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            implicitHeight: Theme.controlHeight
            radius: Theme.radiusSm
            color: Theme.panelSurfaceSoft
            border.color: combo.opened ? root.dialogAccent : Theme.panelBorder
            border.width: combo.opened ? 2 : 1
        }

        popup: Popup {
            y: combo.height + 4
            width: combo.width
            padding: 4
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }

            background: Rectangle {
                color: Theme.menuSurface
                radius: Theme.radiusSm
                border.color: Theme.menuBorder
            }
        }
    }

    component SettingsContentBlock: Rectangle {
        id: block

        default property alias content: blockContent.data

        Layout.fillWidth: true
        implicitHeight: blockContent.implicitHeight + 16
        radius: Theme.radiusSm
        color: root.rowFill
        border.color: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.30 : 0.24)
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 7
            width: 2
            radius: 1
            color: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.80 : 0.68)
        }

        ColumnLayout {
            id: blockContent
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 10
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            spacing: 6
        }
    }
}
