#include "AppSettingsController.h"

#include <QByteArray>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJSEngine>
#include <QTemporaryDir>
#include <QTextStream>
#include <QSettings>
#include <QVariantMap>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));

    // Need QGuiApplication because AppSettingsController initializes application fonts
    QGuiApplication app(argc, argv);
    
    // Set organization and app name for QSettings isolation
    QCoreApplication::setOrganizationName("FMQmlTestOrg");
    QCoreApplication::setApplicationName("AppSettingsControllerTest");
    
    // Clear any existing settings for clean test run
    {
        QSettings settings;
        settings.clear();
    }
    
    AppSettingsController controller;

    if (controller.commandPaletteTransparencyStrength() != 60) {
        return fail("command palette transparency strength should default to 60");
    }
    if (controller.surfaceBlur()) {
        return fail("surface blur should be disabled by default");
    }
    if (controller.surfaceBlurStrength() != 72) {
        return fail("surface blur strength should default to 72");
    }
    if (controller.hoverPreviewTransparency()) {
        return fail("hover preview transparency should be disabled by default");
    }
    if (controller.folderPeekTransparency()) {
        return fail("hover and Peek surface defaults are incorrect");
    }
    if (controller.quickLookTransparency()) {
        return fail("quick look transparency should be disabled by default");
    }
    if (controller.propertiesDialogTransparency()) {
        return fail("properties dialog transparency should be disabled by default");
    }
    if (controller.workspaceDialogsTransparency()) {
        return fail("workspace dialogs transparency should be disabled by default");
    }
    if (controller.uiLabViewportPreset() != 1
        || controller.uiLabBackgroundPreset() != 0
        || !controller.uiLabStateMatrix()) {
        return fail("UI Lab environment defaults are incorrect");
    }
    controller.setUiLabViewportPreset(99);
    controller.setUiLabBackgroundPreset(-5);
    controller.setUiLabStateMatrix(false);
    if (controller.uiLabViewportPreset() != 3
        || controller.uiLabBackgroundPreset() != 0
        || controller.uiLabStateMatrix()) {
        return fail("UI Lab environment validation failed");
    }
    controller.setUiLabViewportPreset(2);
    controller.setUiLabBackgroundPreset(1);
    {
        AppSettingsController persistedController;
        if (persistedController.uiLabViewportPreset() != 2
            || persistedController.uiLabBackgroundPreset() != 1
            || persistedController.uiLabStateMatrix()) {
            return fail("UI Lab environment was not persisted");
        }
    }
    controller.setCommandPaletteTransparencyStrength(120);
    if (controller.commandPaletteTransparencyStrength() != 100) {
        return fail("command palette transparency strength should clamp to 100");
    }
    controller.setCommandPaletteTransparencyStrength(-10);
    if (controller.commandPaletteTransparencyStrength() != 0) {
        return fail("command palette transparency strength should clamp to 0");
    }
    controller.setCommandPaletteTransparencyStrength(65);
    controller.setSurfaceBlur(true);
    controller.setSurfaceBlurStrength(120);
    if (controller.surfaceBlurStrength() != 100) {
        return fail("surface blur strength should clamp to 100");
    }
    controller.setSurfaceBlurStrength(-10);
    if (controller.surfaceBlurStrength() != 0) {
        return fail("surface blur strength should clamp to 0");
    }
    controller.setSurfaceBlurStrength(65);
    controller.setHoverPreviewTransparency(true);
    controller.setFolderPeekTransparency(true);
    controller.setQuickLookTransparency(true);
    controller.setPropertiesDialogTransparency(true);
    controller.setWorkspaceDialogsTransparency(true);

    const QVariantMap defaultWorkspace = controller.workspaceState();
    if (defaultWorkspace.value("previewPanePlacement").toString() != "right") {
        return fail("preview pane placement should default to right");
    }
    if (defaultWorkspace.value("sidebarPanelOrder").toStringList() != QStringList{"places", "recent", "folders"}
        || defaultWorkspace.value("sidebarHiddenPanels").toStringList() != QStringList{"recent"}
        || !defaultWorkspace.value("sidebarCollapsedPanels").toStringList().isEmpty()
        || defaultWorkspace.value("sidebarPanelWeights").toMap()
            != QVariantMap{{"folders", 1.0}, {"places", 1.0}, {"recent", 1.0}}) {
        return fail("sidebar workspace settings have incorrect defaults");
    }
    if (!qFuzzyCompare(defaultWorkspace.value("filePanelSplitRatio").toDouble(), 0.5)) {
        return fail("file panel split ratio should default to 0.5");
    }
    if (defaultWorkspace.value("filePanelSplitRatioStored").toBool()) {
        return fail("default file panel split ratio should use the legacy migration path");
    }
    if (defaultWorkspace.value("leftShowMediaHoverPreviews").toBool()
        || defaultWorkspace.value("rightShowMediaHoverPreviews").toBool()
        || defaultWorkspace.value("leftShowFolderHoverPreviews").toBool()
        || defaultWorkspace.value("rightShowFolderHoverPreviews").toBool()
        || defaultWorkspace.value("leftFolderPeekEnabled").toBool()
        || defaultWorkspace.value("rightFolderPeekEnabled").toBool()) {
        return fail("hover preview workspace settings should be disabled by default");
    }

    QVariantMap hoverWorkspace;
    hoverWorkspace["leftShowMediaHoverPreviews"] = true;
    hoverWorkspace["rightShowMediaHoverPreviews"] = false;
    hoverWorkspace["leftShowFolderHoverPreviews"] = false;
    hoverWorkspace["rightShowFolderHoverPreviews"] = true;
    hoverWorkspace["leftFolderPeekEnabled"] = true;
    hoverWorkspace["rightFolderPeekEnabled"] = false;
    controller.saveWorkspaceState(hoverWorkspace);
    const QVariantMap savedHoverWorkspace = controller.workspaceState();
    if (!savedHoverWorkspace.value("leftShowMediaHoverPreviews").toBool()
        || savedHoverWorkspace.value("rightShowMediaHoverPreviews").toBool()
        || savedHoverWorkspace.value("leftShowFolderHoverPreviews").toBool()
        || !savedHoverWorkspace.value("rightShowFolderHoverPreviews").toBool()
        || !savedHoverWorkspace.value("leftFolderPeekEnabled").toBool()
        || savedHoverWorkspace.value("rightFolderPeekEnabled").toBool()) {
        return fail("independent hover preview workspace settings were not persisted");
    }
    
    // 1. Verify default state (all overrides disabled)
    QVariantList metadata = controller.rolesMetadata();
    if (metadata.isEmpty()) {
        return fail("rolesMetadata returned empty list");
    }
    
    for (const QVariant &var : metadata) {
        QVariantMap role = var.toMap();
        QString id = role["id"].toString();
        if (controller.isOverrideEnabled(id)) {
            return fail(QString("role %1 should be disabled by default").arg(id));
        }
        if (!controller.overrideColor(id).isEmpty()) {
            return fail(QString("role %1 should have no override color by default").arg(id));
        }
    }
    
    // 2. Test saving valid overrides
    QVariantMap inputOverrides;
    QVariantMap fileNameEntry;
    fileNameEntry["enabled"] = true;
    fileNameEntry["color"] = "#EAF2FF";
    inputOverrides["fileNameText"] = fileNameEntry;
    
    QVariantMap folderNameEntry;
    folderNameEntry["enabled"] = false;
    folderNameEntry["color"] = "#FF0000";
    inputOverrides["folderNameText"] = folderNameEntry;
    
    // Test validation: invalid color must be disabled
    QVariantMap invalidEntry;
    invalidEntry["enabled"] = true;
    invalidEntry["color"] = "invalid_color_xyz";
    inputOverrides["sidebarText"] = invalidEntry;
    
    controller.saveTextColorOverrides(inputOverrides);
    
    if (!controller.isOverrideEnabled("fileNameText")) {
        return fail("fileNameText should be enabled");
    }
    if (controller.overrideColor("fileNameText") != "#EAF2FF") {
        return fail("fileNameText has incorrect color override");
    }
    if (controller.isOverrideEnabled("folderNameText")) {
        return fail("folderNameText should be disabled");
    }
    if (controller.isOverrideEnabled("sidebarText")) {
        return fail("sidebarText should be disabled due to invalid color");
    }
    
    // 3. Test setRoleOverride / setRoleEnabled
    controller.setRoleOverride("sidebarText", "#00FF00");
    if (!controller.isOverrideEnabled("sidebarText")) {
        return fail("sidebarText should be enabled after valid setRoleOverride");
    }
    if (controller.overrideColor("sidebarText") != "#00FF00") {
        return fail("sidebarText has incorrect color override after setRoleOverride");
    }
    
    controller.setRoleEnabled("sidebarText", false);
    if (controller.isOverrideEnabled("sidebarText")) {
        return fail("sidebarText should be disabled after setRoleEnabled(false)");
    }
    
    // 4. Test resetRole (disables but keeps color)
    controller.setRoleOverride("statusText", "#FFFF00");
    controller.resetRole("statusText");
    if (controller.isOverrideEnabled("statusText")) {
        return fail("statusText should be disabled after reset");
    }
    // Color should be preserved in map
    QVariantMap overrides = controller.textColorOverrides();
    QVariantMap statusEntry = overrides["statusText"].toMap();
    if (statusEntry["color"].toString() != "#FFFF00") {
        return fail("statusText color should be preserved after resetRole");
    }
    
    // 5. Test resetAll
    controller.setRoleOverride("filePathText", "#00FFFF");
    controller.resetAll();
    if (controller.isOverrideEnabled("filePathText")) {
        return fail("filePathText should be disabled after resetAll");
    }
    
    // 6. Test workspace placement and ratio validation
    QVariantMap workspaceInput;
    workspaceInput["previewPanePlacement"] = "between-panels";
    workspaceInput["filePanelSplitRatio"] = 0.64;
    QJSEngine sidebarStateEngine;
    QJSValue sidebarOrder = sidebarStateEngine.newArray(3);
    sidebarOrder.setProperty(0, QStringLiteral("folders"));
    sidebarOrder.setProperty(1, QStringLiteral("unknown"));
    sidebarOrder.setProperty(2, QStringLiteral("folders"));
    QJSValue sidebarHidden = sidebarStateEngine.newArray(3);
    sidebarHidden.setProperty(0, QStringLiteral("places"));
    sidebarHidden.setProperty(1, QStringLiteral("unknown"));
    sidebarHidden.setProperty(2, QStringLiteral("places"));
    QJSValue sidebarCollapsed = sidebarStateEngine.newArray(3);
    sidebarCollapsed.setProperty(0, QStringLiteral("folders"));
    sidebarCollapsed.setProperty(1, QStringLiteral("invalid"));
    sidebarCollapsed.setProperty(2, QStringLiteral("folders"));
    workspaceInput["sidebarPanelOrder"] = QVariant::fromValue(sidebarOrder);
    workspaceInput["sidebarHiddenPanels"] = QVariant::fromValue(sidebarHidden);
    workspaceInput["sidebarCollapsedPanels"] = QVariant::fromValue(sidebarCollapsed);
    workspaceInput["sidebarPanelWeights"] = QVariantMap{
        {"places", 0.01}, {"recent", 2.5}, {"folders", 100.0}, {"unknown", 4.0}};
    controller.saveWorkspaceState(workspaceInput);

    QVariantMap savedWorkspace = controller.workspaceState();
    if (savedWorkspace.value("previewPanePlacement").toString() != "between-panels") {
        return fail("preview pane placement was not saved");
    }
    if (qAbs(savedWorkspace.value("filePanelSplitRatio").toDouble() - 0.64) > 0.0001) {
        return fail("file panel split ratio was not saved");
    }
    if (!savedWorkspace.value("filePanelSplitRatioStored").toBool()) {
        return fail("saved file panel split ratio should bypass the legacy migration path");
    }
    if (savedWorkspace.value("sidebarPanelOrder").toStringList() != QStringList{"folders", "places", "recent"}
        || savedWorkspace.value("sidebarHiddenPanels").toStringList() != QStringList{"places"}
        || savedWorkspace.value("sidebarCollapsedPanels").toStringList() != QStringList{"folders"}
        || savedWorkspace.value("sidebarPanelWeights").toMap()
            != QVariantMap{{"folders", 20.0}, {"places", 0.05}, {"recent", 2.5}}) {
        return fail("sidebar workspace settings were not sanitized and persisted");
    }

    controller.setSidebarPanelEnabled(QStringLiteral("folders"), false);
    controller.setSidebarPanelEnabled(QStringLiteral("places"), true);
    controller.setSidebarPanelCollapsed(QStringLiteral("folders"), false);
    controller.setSidebarPanelCollapsed(QStringLiteral("places"), true);
    const QVariantMap scalarSidebarWorkspace = controller.workspaceState();
    if (scalarSidebarWorkspace.value("sidebarHiddenPanels").toStringList() != QStringList{"folders"}
        || scalarSidebarWorkspace.value("sidebarCollapsedPanels").toStringList() != QStringList{"places"}) {
        return fail("scalar sidebar persistence operations failed");
    }
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("workspace"));
        settings.setValue(QStringLiteral("sidebarHiddenPanels"), QStringLiteral("folders"));
        settings.setValue(QStringLiteral("sidebarCollapsedPanels"), QStringLiteral("places"));
        settings.endGroup();
        settings.sync();
    }
    const QVariantMap singleSidebarWorkspace = controller.workspaceState();
    if (singleSidebarWorkspace.value("sidebarHiddenPanels").toStringList() != QStringList{"folders"}
        || singleSidebarWorkspace.value("sidebarCollapsedPanels").toStringList() != QStringList{"places"}) {
        return fail("single sidebar panel IDs were not restored from scalar settings values");
    }

    {
        QSettings settings;
        settings.beginGroup("workspace");
        settings.setValue("previewPanePlacement", "invalid-placement");
        settings.setValue("filePanelSplitRatio", 2.0);
        settings.endGroup();
    }
    const QVariantMap sanitizedWorkspace = controller.workspaceState();
    if (sanitizedWorkspace.value("previewPanePlacement").toString() != "right") {
        return fail("invalid preview pane placement should fall back to right");
    }
    if (qAbs(sanitizedWorkspace.value("filePanelSplitRatio").toDouble() - 0.9) > 0.0001) {
        return fail("file panel split ratio should be clamped to 0.9");
    }

    controller.saveWorkspaceState(workspaceInput);

    // 7. Test settings export / import roundtrip
    controller.setRoleOverride("fileNameText", "#112233");
    controller.setRoleOverride("folderNameText", "#445566");
    {
        const QVariantList iconOverrides = {
            QVariantMap{{QStringLiteral("suffix"), QStringLiteral("epub")},
                        {QStringLiteral("sourceType"), QStringLiteral("bundled")},
                        {QStringLiteral("sourceValue"), QStringLiteral("epub")}}
        };
        QSettings settings;
        settings.beginGroup(QStringLiteral("appearance"));
        settings.setValue(QStringLiteral("iconOverridesRules"),
                          QJsonDocument::fromVariant(iconOverrides).toJson(QJsonDocument::Compact));
        settings.endGroup();
    }
    
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail("Failed to create temporary directory for settings export");
    }
    
    QString settingsPath = tempDir.filePath("settings.json");
    if (!controller.exportSettings(settingsPath)) {
        return fail("Export settings failed");
    }
    
    // Clear settings
    {
        QSettings settings;
        settings.clear();
    }
    AppSettingsController freshController;
    if (freshController.uiLabViewportPreset() != 1
        || freshController.uiLabBackgroundPreset() != 0
        || !freshController.uiLabStateMatrix()) {
        return fail("cleared UI Lab environment should return to defaults");
    }
    if (freshController.isOverrideEnabled("fileNameText")) {
        return fail("fileNameText should be disabled on fresh controller");
    }
    
    if (!freshController.importSettings(settingsPath)) {
        return fail("Import settings failed");
    }
    if (freshController.commandPaletteTransparencyStrength() != 65) {
        return fail("command palette transparency strength not imported correctly");
    }
    if (!freshController.surfaceBlur()) {
        return fail("surface blur not imported correctly");
    }
    if (freshController.surfaceBlurStrength() != 65) {
        return fail("surface blur strength not imported correctly");
    }
    if (!freshController.hoverPreviewTransparency()) {
        return fail("hover preview transparency not imported correctly");
    }
    if (!freshController.folderPeekTransparency()) {
        return fail("Folder Peek surface settings not imported correctly");
    }
    if (!freshController.quickLookTransparency()) {
        return fail("quick look transparency not imported correctly");
    }
    if (!freshController.propertiesDialogTransparency()) {
        return fail("properties dialog transparency not imported correctly");
    }
    if (!freshController.workspaceDialogsTransparency()) {
        return fail("workspace dialogs transparency not imported correctly");
    }
    
    if (!freshController.isOverrideEnabled("fileNameText") || freshController.overrideColor("fileNameText") != "#112233") {
        return fail("fileNameText not imported correctly");
    }
    if (!freshController.isOverrideEnabled("folderNameText") || freshController.overrideColor("folderNameText") != "#445566") {
        return fail("folderNameText not imported correctly");
    }
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("appearance"));
        const QVariantList iconOverrides = QJsonDocument::fromJson(
            settings.value(QStringLiteral("iconOverridesRules")).toByteArray()).toVariant().toList();
        settings.endGroup();
        if (iconOverrides.size() != 1
            || iconOverrides.constFirst().toMap().value(QStringLiteral("suffix")).toString() != QStringLiteral("epub")) {
            return fail("icon overrides not imported correctly");
        }
    }
    const QVariantMap importedWorkspace = freshController.workspaceState();
    if (importedWorkspace.value("previewPanePlacement").toString() != "between-panels") {
        return fail("preview pane placement not imported correctly");
    }
    if (qAbs(importedWorkspace.value("filePanelSplitRatio").toDouble() - 0.64) > 0.0001) {
        return fail("file panel split ratio not imported correctly");
    }
    
    // Clear settings after test
    {
        QSettings settings;
        settings.clear();
    }
    
    QTextStream(stdout) << "All settings controller tests passed successfully!\n";
    return 0;
}
