import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../style"
import "dialogs"
import "settings"

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
    property bool gradientColorsEnabled: true
    property bool commandPaletteTransparencyEnabled: true
    property bool shellFirstQmlRestoreEnabled: false
    property bool systemTrayIconEnabled: false
    property bool allowOnlyOneInstanceEnabled: false
    property bool limitedDragNDropEnabled: false
    property string fontFamilyValue: typeof appSettings !== "undefined" && appSettings
                                     ? appSettings.fontFamily
                                     : ""
    property int fontScaleValue: typeof appSettings !== "undefined" && appSettings
                                 ? appSettings.fontScale
                                 : 100
    property bool googleDriveAuthorized: false
    property bool megaAuthorized: false
    property bool instagramAuthorized: false
    property bool telegramAuthorized: false
    property string megaStatusText: "Sign in to browse, download, and upload to your MEGA Cloud Drive."
    property string instagramStatusText: "Import a HeaderString cookie file to enable profile pagination, stories, and direct media access."
    property string telegramStatusText: "Set Telegram API credentials, then sign in to browse files from Saved Messages and chats."

    Timer {
        id: megaRefreshTimer
        interval: 1000
        running: root.opened && root.megaAuthorized
        repeat: true
        onTriggered: root.refreshMegaAuthorization()
    }

    Timer {
        id: telegramRefreshTimer
        interval: 1500
        running: root.opened && telegramLoginDialog.visible && !root.telegramAuthorized
        repeat: true
        onTriggered: root.refreshTelegramAuthorization()
    }

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
        refreshMegaAuthorization()
        refreshInstagramAuthorization()
        refreshTelegramAuthorization()
        Qt.callLater(() => contentItem.forceActiveFocus())
    }

    function workspace() {
        return typeof workspaceController !== "undefined" ? workspaceController : null
    }

    function leaveSignedOutProvider(providerPrefix) {
        const workspaceCtrl = root.workspace()
        if (!workspaceCtrl) {
            return
        }
        const panels = [workspaceCtrl.leftPanel, workspaceCtrl.rightPanel]
        for (let i = 0; i < panels.length; ++i) {
            const panel = panels[i]
            const currentPath = panel ? String(panel.currentPath || "") : ""
            if (currentPath.startsWith(providerPrefix)) {
                panel.openPath("devices://")
            }
        }
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
        gradientColorsEnabled = typeof appSettings !== "undefined" && appSettings
                                ? appSettings.useGradientColors
                                : true
        commandPaletteTransparencyEnabled = typeof appSettings !== "undefined" && appSettings
                                            ? appSettings.commandPaletteTransparency
                                            : true
        shellFirstQmlRestoreEnabled = typeof appSettings !== "undefined" && appSettings
                                      ? appSettings.shellFirstQmlRestore
                                      : false
        systemTrayIconEnabled = typeof appSettings !== "undefined" && appSettings
                                ? appSettings.useSystemTrayIcon
                                : false
        allowOnlyOneInstanceEnabled = typeof appSettings !== "undefined" && appSettings
                                      ? appSettings.allowOnlyOneInstance
                                      : false
        limitedDragNDropEnabled = typeof appSettings !== "undefined" && appSettings
                                  ? appSettings.useLimitedDragNDrop
                                  : false
        fontFamilyValue = typeof appSettings !== "undefined" && appSettings
                          ? appSettings.fontFamily
                          : ""
        fontScaleValue = typeof appSettings !== "undefined" && appSettings
                         ? appSettings.fontScale
                         : 100
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
        if (enabled && !nativeIconsEnabled) {
            return
        }
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

    function setGradientColorsEnabled(enabled) {
        gradientColorsEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.useGradientColors !== enabled) {
            appSettings.useGradientColors = enabled
        }
    }

    function setCommandPaletteTransparencyEnabled(enabled) {
        commandPaletteTransparencyEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.commandPaletteTransparency !== enabled) {
            appSettings.commandPaletteTransparency = enabled
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

    function setLimitedDragNDropEnabled(enabled) {
        limitedDragNDropEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.useLimitedDragNDrop !== enabled) {
            appSettings.useLimitedDragNDrop = enabled
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

    function openTextColorOverrides() {
        root.close()
        if (root.appRoot && root.appRoot.openTextColorOverridesOverlay) {
            root.appRoot.openTextColorOverridesOverlay()
        }
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
        if (result && result.ok === true) {
            root.leaveSignedOutProvider("gdrive://")
        }
        root.refreshGoogleDriveAuthorization()
        if (root.appRoot) {
            root.appRoot.showTransientInfo(String(result.message || "Google Drive sign out requested."))
        }
    }

    function refreshMegaAuthorization() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            megaAuthorized = false
            megaStatusText = "Sign in to browse, download, and upload to your MEGA Cloud Drive."
            return
        }
        const result = pluginActionController.triggerAction("mega::authStatus", {})
        megaAuthorized = !!(result && result.ok === true && result.signedIn === true)
        if (megaAuthorized && result) {
            const email = result.accountEmail || ""
            let storageVal = ""
            if (result.properties) {
                for (let i = 0; i < result.properties.length; ++i) {
                    if (result.properties[i].label === "Storage usage" || result.properties[i].label === "Known used storage") {
                        storageVal = result.properties[i].value
                    }
                }
            }
            megaStatusText = "MEGA account access is active (" + email + ")." + (storageVal ? " Storage usage: " + storageVal + "." : "")
        } else {
            megaStatusText = "Sign in to browse, download, and upload to your MEGA Cloud Drive."
        }
    }

    function signOutMega() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            if (root.appRoot) root.appRoot.showTransientInfo("Plugin controller is unavailable.")
            return
        }
        const result = pluginActionController.triggerAction("mega::signOut", {})
        if (result && result.ok === true) {
            root.leaveSignedOutProvider("mega://")
        }
        root.refreshMegaAuthorization()
        if (root.appRoot) {
            root.appRoot.showTransientInfo(String(result.message || "MEGA sign out requested."))
        }
    }

    function openMegaLoginDialog() {
        megaLoginDialog.emailText = ""
        megaLoginDialog.passwordText = ""
        megaLoginDialog.open()
        megaLoginDialog.focusEmail()
    }

    function submitMegaLogin() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            if (root.appRoot) root.appRoot.showTransientInfo("Plugin controller is unavailable.")
            return
        }
        const result = pluginActionController.triggerAction("mega::signIn", {
            targetPath: "mega:///",
            parameters: {
                email: megaLoginDialog.emailText,
                password: megaLoginDialog.passwordText
            }
        })
        if (result && result.ok === true) {
            megaLoginDialog.close()
            root.refreshMegaAuthorization()
            const panel = root.appRoot && root.appRoot.activePanelController ? root.appRoot.activePanelController() : null
            if (panel && panel.openPath) {
                panel.openPath("mega:///")
                root.close()
            }
        }
        if (root.appRoot) {
            root.appRoot.showTransientInfo(String(result.message || "MEGA sign in requested."))
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

    function refreshInstagramAuthorization() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            instagramAuthorized = false
            instagramStatusText = "Import a HeaderString cookie file to enable profile pagination, stories, and direct media access."
            return
        }
        const result = pluginActionController.triggerAction("fm.instagram-provider::authStatus", {})
        instagramAuthorized = !!(result && result.ok === true && result.signedIn === true)
        if (instagramAuthorized && result) {
            instagramStatusText = "Instagram session is active" + (result.accountLabel ? " (" + result.accountLabel + ")" : "") + "."
            if (result.envOverride === true) {
                instagramStatusText += " Environment cookie override is active."
            }
        } else {
            instagramStatusText = "Import a HeaderString cookie file to enable profile pagination, stories, and direct media access."
        }
    }

    function openInstagramSessionImportDialog() {
        instagramSessionDialog.open()
    }

    function importInstagramSessionFile(fileUrl) {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            if (root.appRoot) root.appRoot.showTransientInfo("Plugin controller is unavailable.")
            return
        }
        const result = pluginActionController.triggerAction("fm.instagram-provider::importSession", {
            parameters: {
                fileUrl: fileUrl.toString()
            }
        })
        root.refreshInstagramAuthorization()
        if (root.appRoot) {
            root.appRoot.showTransientInfo(String(result.message || "Instagram session import requested."))
        }
    }

    function signOutInstagram() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            if (root.appRoot) root.appRoot.showTransientInfo("Plugin controller is unavailable.")
            return
        }
        const result = pluginActionController.triggerAction("fm.instagram-provider::signOut", {})
        if (result && result.ok === true) {
            root.leaveSignedOutProvider("instagram://")
        }
        root.refreshInstagramAuthorization()
        if (root.appRoot) {
            root.appRoot.showTransientInfo(String(result.message || "Instagram sign out requested."))
        }
    }

    function refreshTelegramAuthorization() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            telegramAuthorized = false
            telegramStatusText = "Plugin controller is unavailable."
            return
        }
        const result = pluginActionController.triggerAction("fm.telegram-provider::authStatus", {})
        telegramAuthorized = !!(result && result.ok === true && result.signedIn === true)
        telegramStatusText = result && result.message
                           ? String(result.message)
                           : "Enter Telegram API ID, API hash, and phone number to continue authorization."
    }

    function openTelegramLoginDialog() {
        telegramLoginDialog.apiIdText = ""
        telegramLoginDialog.apiHashText = ""
        telegramLoginDialog.phoneText = ""
        telegramLoginDialog.codeText = ""
        telegramLoginDialog.passwordText = ""
        telegramLoginDialog.open()
        telegramLoginDialog.focusApiId()
    }

    function triggerTelegramAuthAction(actionId, parameters, fallbackMessage) {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            if (root.appRoot) root.appRoot.showTransientInfo("Plugin controller is unavailable.")
            return
        }
        const result = pluginActionController.triggerAction("fm.telegram-provider::" + actionId, {
            targetPath: "telegram:///",
            parameters: parameters || {}
        })
        root.refreshTelegramAuthorization()
        if (root.appRoot) {
            root.appRoot.showTransientInfo(String(result.message || fallbackMessage))
        }
        if (result && result.ok === true && result.signedIn === true) {
            telegramLoginDialog.close()
        }
        if (actionId === "signOut" && result && result.ok === true) {
            root.leaveSignedOutProvider("telegram://")
        }
    }

    function submitTelegramPhone() {
        const apiId = Number(telegramLoginDialog.apiIdText.trim())
        root.triggerTelegramAuthAction("setPhoneNumber", {
            apiId: isNaN(apiId) ? 0 : apiId,
            apiHash: telegramLoginDialog.apiHashText.trim(),
            phoneNumber: telegramLoginDialog.phoneText
        }, "Telegram phone number submitted.")
    }

    function submitTelegramCode() {
        root.triggerTelegramAuthAction("checkCode", { code: telegramLoginDialog.codeText }, "Telegram login code submitted.")
    }

    function submitTelegramPassword() {
        root.triggerTelegramAuthAction("checkPassword", { password: telegramLoginDialog.passwordText }, "Telegram 2FA password submitted.")
    }

    function signOutTelegram() {
        root.triggerTelegramAuthAction("signOut", {}, "Telegram sign out requested.")
    }

    function openForgetTelegramLocalDataDialog() {
        telegramForgetLocalDataDialog.open()
    }

    function forgetTelegramLocalData() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            if (root.appRoot) root.appRoot.showTransientInfo("Plugin controller is unavailable.")
            return
        }
        const result = pluginActionController.triggerAction("fm.telegram-provider::forgetLocalData", {
            targetPath: "telegram:///",
            parameters: {}
        })
        telegramForgetLocalDataDialog.close()
        root.refreshTelegramAuthorization()
        if (root.appRoot) {
            root.appRoot.showTransientInfo(String(result && result.message ? result.message : "Telegram local data reset requested."))
        }
    }

    function openTelegramSource(target) {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            telegramStatusText = "Plugin controller is unavailable."
            return
        }
        const result = pluginActionController.triggerAction("fm.telegram-provider::openChat", {
            parameters: {
                target: String(target || "")
            }
        })
        if (!result || result.ok !== true || !result.openPath) {
            telegramStatusText = result && result.message ? result.message : "Enter a Telegram chat id, @username, or t.me link."
            return
        }
        const panel = root.appRoot && root.appRoot.activePanelController ? root.appRoot.activePanelController() : null
        if (panel && panel.openPath) {
            panel.openPath(result.openPath)
            root.close()
        }
    }

    function openFontSelector() {
        fontSelectorPopup.openSelector(root.fontFamilyValue,
                                       typeof appSettings !== "undefined" && appSettings
                                       ? appSettings.availableFontFamilies : [])
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

    MegaLoginDialog {
        id: megaLoginDialog
        dialogRoot: root
    }

    TelegramLoginDialog {
        id: telegramLoginDialog
        dialogRoot: root
    }

    TelegramForgetLocalDataDialog {
        id: telegramForgetLocalDataDialog
        dialogRoot: root
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

                    SettingsWorkspaceSection {
                        splitViewEnabled: root.splitViewEnabled
                        previewPaneEnabled: root.previewPaneEnabled
                        setSplitViewEnabled: root.setSplitViewEnabled
                        setPreviewPaneEnabled: root.setPreviewPaneEnabled
                    }

                    SettingsAppSection {
                        dialogRoot: root
                    }

                    SettingsProvidersSection {
                        dialogRoot: root
                    }

                    SettingsTypographySection {
                        dialogRoot: root
                    }

                    SettingsFilesSection {
                        hiddenFilesEnabled: root.hiddenFilesEnabled
                        setHiddenFilesEnabled: root.setHiddenFilesEnabled
                    }

                    SettingsPerformanceSection {
                        nativeIconsEnabled: root.nativeIconsEnabled
                        highQualitySystemIconsEnabled: root.highQualitySystemIconsEnabled
                        thumbnailsEnabled: root.thumbnailsEnabled
                        ultraLightModeEnabled: root.ultraLightModeEnabled
                        gradientColorsEnabled: root.gradientColorsEnabled
                        commandPaletteTransparencyEnabled: root.commandPaletteTransparencyEnabled
                        shellFirstQmlRestoreEnabled: root.shellFirstQmlRestoreEnabled
                        setNativeIconsEnabled: root.setNativeIconsEnabled
                        setHighQualitySystemIconsEnabled: root.setHighQualitySystemIconsEnabled
                        setThumbnailsEnabled: root.setThumbnailsEnabled
                        setUltraLightModeEnabled: root.setUltraLightModeEnabled
                        setGradientColorsEnabled: root.setGradientColorsEnabled
                        setCommandPaletteTransparencyEnabled: root.setCommandPaletteTransparencyEnabled
                        setShellFirstQmlRestoreEnabled: root.setShellFirstQmlRestoreEnabled
                    }

                    SettingsThemesSection {
                        openThemeEditor: root.openThemeEditor
                    }

                    SettingsStateSection {
                        dialogRoot: root
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
        function onUseGradientColorsChanged() {
            root.gradientColorsEnabled = appSettings ? appSettings.useGradientColors : true
        }
        function onCommandPaletteTransparencyChanged() {
            root.commandPaletteTransparencyEnabled = appSettings ? appSettings.commandPaletteTransparency : true
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
        function onUseLimitedDragNDropChanged() {
            root.limitedDragNDropEnabled = appSettings ? appSettings.useLimitedDragNDrop : false
        }
        function onFontFamilyChanged() {
            root.fontFamilyValue = appSettings ? appSettings.fontFamily : ""
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

    FileDialog {
        id: instagramSessionDialog
        title: "Import Instagram Session"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Cookie files (*.txt *.cookie *.cookies)", "Text files (*.txt)", "All files (*)"]
        onAccepted: root.importInstagramSessionFile(selectedFile)
    }

    FontSelectorPopup {
        id: fontSelectorPopup
        onFontSelected: (family) => root.setFontFamily(family)
    }
}
