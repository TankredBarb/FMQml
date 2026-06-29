#include "AppSettingsController.h"

#include <QByteArray>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
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
    
    // 6. Test settings export / import roundtrip
    controller.setRoleOverride("fileNameText", "#112233");
    controller.setRoleOverride("folderNameText", "#445566");
    
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
    if (freshController.isOverrideEnabled("fileNameText")) {
        return fail("fileNameText should be disabled on fresh controller");
    }
    
    if (!freshController.importSettings(settingsPath)) {
        return fail("Import settings failed");
    }
    
    if (!freshController.isOverrideEnabled("fileNameText") || freshController.overrideColor("fileNameText") != "#112233") {
        return fail("fileNameText not imported correctly");
    }
    if (!freshController.isOverrideEnabled("folderNameText") || freshController.overrideColor("folderNameText") != "#445566") {
        return fail("folderNameText not imported correctly");
    }
    
    // Clear settings after test
    {
        QSettings settings;
        settings.clear();
    }
    
    QTextStream(stdout) << "All settings controller tests passed successfully!\n";
    return 0;
}
