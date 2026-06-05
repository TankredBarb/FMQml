#include "AppSettingsController.h"

#include "../core/ArchiveSupport.h"
#include "ThemeController.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QRect>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QtGlobal>
#include <QUrl>

namespace {
constexpr auto WorkspaceGroup = "workspace";
constexpr auto AppearanceGroup = "appearance";
constexpr auto DeviceRoot = "devices://";
constexpr auto FavoritesRoot = "favorites://";
constexpr auto ExportFormatVersion = 2;
constexpr auto ByteArrayEncodingKey = "__encoding";
constexpr auto ByteArrayDataKey = "data";
constexpr auto ByteArrayEncodingBase64 = "base64";

QVariantMap variantMapFromJsonObject(const QJsonObject &object)
{
    return object.toVariantMap();
}

int boundedInt(const QVariant &value, int fallback, int min, int max)
{
    bool ok = false;
    const int candidate = value.toInt(&ok);
    if (!ok) {
        return fallback;
    }
    return qBound(min, candidate, max);
}

bool rectIntersectsAnyScreen(const QRect &rect)
{
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen && rect.intersects(screen->availableGeometry())) {
            return true;
        }
    }
    return false;
}

QRect availableScreenForRect(const QRect &rect)
{
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen && rect.intersects(screen->availableGeometry())) {
            return screen->availableGeometry();
        }
    }

    return QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1120, 720);
}

bool looksLikeAccidentallySavedMaximizedGeometry(const QRect &rect)
{
    const QRect screenRect = availableScreenForRect(rect);
    return qAbs(rect.width() - screenRect.width()) <= 2
        && qAbs(rect.height() - screenRect.height()) <= 2
        && qAbs(rect.x() - screenRect.x()) <= 2
        && qAbs(rect.y() - screenRect.y()) <= 2;
}

QString nearestExistingFolderAtOrAbove(const QString &path)
{
    QDir dir(QDir::fromNativeSeparators(path));
    while (!dir.path().isEmpty()) {
        const QString candidate = dir.absolutePath();
        const QFileInfo info(candidate);
        if (info.exists() && info.isDir()) {
            return QDir::cleanPath(candidate);
        }

        if (!dir.cdUp() || dir.absolutePath() == candidate) {
            break;
        }
    }
    return {};
}

}

AppSettingsController::AppSettingsController(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    m_useNativeIcons = settings.value(QStringLiteral("useNativeIcons"), true).toBool();
    m_useHighQualitySystemIcons = settings.value(QStringLiteral("useHighQualitySystemIcons"), true).toBool();
    m_showThumbnails = settings.value(QStringLiteral("showThumbnails"), true).toBool();
    m_ultraLightMode = settings.value(QStringLiteral("ultraLightMode"),
                                      settings.value(QStringLiteral("simplifyVisualsForPerformance"), false)).toBool();
    settings.remove(QStringLiteral("useNativeFileEnumerators"));
    m_previewDetailsRaised = settings.value(QStringLiteral("previewDetailsRaised"), false).toBool();
    m_useSystemTrayIcon = settings.value(QStringLiteral("useSystemTrayIcon"), false).toBool();
    settings.endGroup();
}

void AppSettingsController::setThemeController(ThemeController *themeController)
{
    m_themeController = themeController;
}

bool AppSettingsController::useNativeIcons() const
{
    return m_useNativeIcons;
}

void AppSettingsController::setUseNativeIcons(bool enabled)
{
    if (m_useNativeIcons == enabled) {
        return;
    }

    m_useNativeIcons = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("useNativeIcons"), m_useNativeIcons);
    settings.endGroup();
    emit useNativeIconsChanged();
}

bool AppSettingsController::useHighQualitySystemIcons() const
{
    return m_useHighQualitySystemIcons;
}

void AppSettingsController::setUseHighQualitySystemIcons(bool enabled)
{
    if (m_useHighQualitySystemIcons == enabled) {
        return;
    }

    m_useHighQualitySystemIcons = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("useHighQualitySystemIcons"), m_useHighQualitySystemIcons);
    settings.endGroup();
    emit useHighQualitySystemIconsChanged();
}

bool AppSettingsController::showThumbnails() const
{
    return m_showThumbnails;
}

void AppSettingsController::setShowThumbnails(bool enabled)
{
    if (m_showThumbnails == enabled) {
        return;
    }

    m_showThumbnails = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("showThumbnails"), m_showThumbnails);
    settings.endGroup();
    emit showThumbnailsChanged();
}

bool AppSettingsController::ultraLightMode() const
{
    return m_ultraLightMode;
}

void AppSettingsController::setUltraLightMode(bool enabled)
{
    if (m_ultraLightMode == enabled) {
        return;
    }

    m_ultraLightMode = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("ultraLightMode"), m_ultraLightMode);
    settings.endGroup();
    emit ultraLightModeChanged();
}

bool AppSettingsController::previewDetailsRaised() const
{
    return m_previewDetailsRaised;
}

void AppSettingsController::setPreviewDetailsRaised(bool enabled)
{
    if (m_previewDetailsRaised == enabled) {
        return;
    }

    m_previewDetailsRaised = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("previewDetailsRaised"), m_previewDetailsRaised);
    settings.endGroup();
    emit previewDetailsRaisedChanged();
}

bool AppSettingsController::useSystemTrayIcon() const
{
    return m_useSystemTrayIcon;
}

void AppSettingsController::setUseSystemTrayIcon(bool enabled)
{
    if (m_useSystemTrayIcon == enabled) {
        return;
    }

    m_useSystemTrayIcon = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("useSystemTrayIcon"), m_useSystemTrayIcon);
    settings.endGroup();
    emit useSystemTrayIconChanged();
}

QVariantMap AppSettingsController::workspaceState() const
{
    QSettings settings;
    settings.beginGroup(QLatin1String(WorkspaceGroup));

    QVariantMap state;
    if (settings.contains(QStringLiteral("windowX"))) {
        state[QStringLiteral("windowX")] = settings.value(QStringLiteral("windowX"));
    }
    if (settings.contains(QStringLiteral("windowY"))) {
        state[QStringLiteral("windowY")] = settings.value(QStringLiteral("windowY"));
    }
    state[QStringLiteral("windowWidth")] = settings.value(QStringLiteral("windowWidth"), 1120);
    state[QStringLiteral("windowHeight")] = settings.value(QStringLiteral("windowHeight"), 720);
    state[QStringLiteral("windowMaximized")] = settings.value(QStringLiteral("windowMaximized"), false).toBool();
    state[QStringLiteral("splitEnabled")] = settings.value(QStringLiteral("splitEnabled"), false).toBool();
    state[QStringLiteral("activePanel")] = boundedInt(settings.value(QStringLiteral("activePanel"), 0), 0, 0, 1);
    state[QStringLiteral("previewPaneVisible")] = settings.value(QStringLiteral("previewPaneVisible"), false).toBool();
    state[QStringLiteral("sidebarWidth")] = boundedInt(settings.value(QStringLiteral("sidebarWidth"), 200), 200, 140, 300);
    state[QStringLiteral("previewPaneWidth")] = boundedInt(settings.value(QStringLiteral("previewPaneWidth"), 340), 340, 280, 1200);
    state[QStringLiteral("fileWorkspaceSplitState")] = settings.value(QStringLiteral("fileWorkspaceSplitState"));
    state[QStringLiteral("leftPath")] = safeFolderPath(settings.value(QStringLiteral("leftPath")).toString());
    state[QStringLiteral("rightPath")] = safeFolderPath(settings.value(QStringLiteral("rightPath")).toString());
    state[QStringLiteral("leftViewMode")] = boundedInt(settings.value(QStringLiteral("leftViewMode"), 0), 0, 0, 2);
    state[QStringLiteral("rightViewMode")] = boundedInt(settings.value(QStringLiteral("rightViewMode"), 0), 0, 0, 2);
    state[QStringLiteral("leftGridIconSize")] = boundedInt(settings.value(QStringLiteral("leftGridIconSize"), 48), 48, 32, 96);
    state[QStringLiteral("rightGridIconSize")] = boundedInt(settings.value(QStringLiteral("rightGridIconSize"), 48), 48, 32, 96);
    state[QStringLiteral("leftBriefRowHeight")] = boundedInt(settings.value(QStringLiteral("leftBriefRowHeight"), 28), 28, 22, 64);
    state[QStringLiteral("rightBriefRowHeight")] = boundedInt(settings.value(QStringLiteral("rightBriefRowHeight"), 28), 28, 22, 64);
    state[QStringLiteral("leftDetailsVisualState")] = settings.value(QStringLiteral("leftDetailsVisualState")).toMap();
    state[QStringLiteral("rightDetailsVisualState")] = settings.value(QStringLiteral("rightDetailsVisualState")).toMap();
    state[QStringLiteral("leftSortRole")] = boundedInt(settings.value(QStringLiteral("leftSortRole"), 0), 0, 0, 5);
    state[QStringLiteral("rightSortRole")] = boundedInt(settings.value(QStringLiteral("rightSortRole"), 0), 0, 0, 5);
    state[QStringLiteral("leftSortOrder")] = boundedInt(settings.value(QStringLiteral("leftSortOrder"), 0), 0, 0, 1);
    state[QStringLiteral("rightSortOrder")] = boundedInt(settings.value(QStringLiteral("rightSortOrder"), 0), 0, 0, 1);
    state[QStringLiteral("leftMixFilesAndFolders")] = settings.value(QStringLiteral("leftMixFilesAndFolders"), false).toBool();
    state[QStringLiteral("rightMixFilesAndFolders")] = settings.value(QStringLiteral("rightMixFilesAndFolders"), false).toBool();
    state[QStringLiteral("showHidden")] = settings.value(QStringLiteral("showHidden"), false).toBool();

    settings.endGroup();
    return state;
}

void AppSettingsController::saveWorkspaceState(const QVariantMap &state)
{
    QSettings settings;
    settings.beginGroup(QLatin1String(WorkspaceGroup));

    const bool windowMaximized = state.value(QStringLiteral("windowMaximized")).toBool();
    if (!windowMaximized) {
        const int width = boundedInt(state.value(QStringLiteral("windowWidth")), 1120, 760, 10000);
        const int height = boundedInt(state.value(QStringLiteral("windowHeight")), 720, 480, 10000);
        settings.setValue(QStringLiteral("windowX"), state.value(QStringLiteral("windowX")).toInt());
        settings.setValue(QStringLiteral("windowY"), state.value(QStringLiteral("windowY")).toInt());
        settings.setValue(QStringLiteral("windowWidth"), width);
        settings.setValue(QStringLiteral("windowHeight"), height);
    }
    settings.setValue(QStringLiteral("windowMaximized"), windowMaximized);
    settings.setValue(QStringLiteral("splitEnabled"), state.value(QStringLiteral("splitEnabled")).toBool());
    settings.setValue(QStringLiteral("activePanel"), boundedInt(state.value(QStringLiteral("activePanel")), 0, 0, 1));
    settings.setValue(QStringLiteral("previewPaneVisible"), state.value(QStringLiteral("previewPaneVisible")).toBool());
    if (state.contains(QStringLiteral("sidebarWidth"))) {
        settings.setValue(QStringLiteral("sidebarWidth"),
                          boundedInt(state.value(QStringLiteral("sidebarWidth")), 200, 140, 300));
    }
    if (state.contains(QStringLiteral("previewPaneWidth"))) {
        settings.setValue(QStringLiteral("previewPaneWidth"),
                          boundedInt(state.value(QStringLiteral("previewPaneWidth")), 340, 280, 1200));
    }
    if (state.contains(QStringLiteral("fileWorkspaceSplitState"))) {
        settings.setValue(QStringLiteral("fileWorkspaceSplitState"),
                          state.value(QStringLiteral("fileWorkspaceSplitState")));
    }
    settings.setValue(QStringLiteral("leftPath"), safeFolderPath(state.value(QStringLiteral("leftPath")).toString()));
    settings.setValue(QStringLiteral("rightPath"), safeFolderPath(state.value(QStringLiteral("rightPath")).toString()));
    settings.setValue(QStringLiteral("leftViewMode"), boundedInt(state.value(QStringLiteral("leftViewMode")), 0, 0, 2));
    settings.setValue(QStringLiteral("rightViewMode"), boundedInt(state.value(QStringLiteral("rightViewMode")), 0, 0, 2));
    settings.setValue(QStringLiteral("leftGridIconSize"), boundedInt(state.value(QStringLiteral("leftGridIconSize")), 48, 32, 96));
    settings.setValue(QStringLiteral("rightGridIconSize"), boundedInt(state.value(QStringLiteral("rightGridIconSize")), 48, 32, 96));
    settings.setValue(QStringLiteral("leftBriefRowHeight"), boundedInt(state.value(QStringLiteral("leftBriefRowHeight")), 28, 22, 64));
    settings.setValue(QStringLiteral("rightBriefRowHeight"), boundedInt(state.value(QStringLiteral("rightBriefRowHeight")), 28, 22, 64));
    settings.setValue(QStringLiteral("leftDetailsVisualState"), state.value(QStringLiteral("leftDetailsVisualState")).toMap());
    settings.setValue(QStringLiteral("rightDetailsVisualState"), state.value(QStringLiteral("rightDetailsVisualState")).toMap());
    settings.setValue(QStringLiteral("leftSortRole"), boundedInt(state.value(QStringLiteral("leftSortRole")), 0, 0, 5));
    settings.setValue(QStringLiteral("rightSortRole"), boundedInt(state.value(QStringLiteral("rightSortRole")), 0, 0, 5));
    settings.setValue(QStringLiteral("leftSortOrder"), boundedInt(state.value(QStringLiteral("leftSortOrder")), 0, 0, 1));
    settings.setValue(QStringLiteral("rightSortOrder"), boundedInt(state.value(QStringLiteral("rightSortOrder")), 0, 0, 1));
    settings.setValue(QStringLiteral("leftMixFilesAndFolders"), state.value(QStringLiteral("leftMixFilesAndFolders")).toBool());
    settings.setValue(QStringLiteral("rightMixFilesAndFolders"), state.value(QStringLiteral("rightMixFilesAndFolders")).toBool());
    settings.setValue(QStringLiteral("showHidden"), state.value(QStringLiteral("showHidden")).toBool());

    settings.endGroup();
    emit workspaceStateChanged();
}

QString AppSettingsController::safeFolderPath(const QString &path) const
{
    const QString trimmed = path.trimmed();
    if (ArchiveSupport::isArchivePath(trimmed)) {
        const QFileInfo physicalInfo(ArchiveSupport::physicalArchivePath(trimmed));
        const QString parent = physicalInfo.absoluteDir().absolutePath();
        const QString existingParent = nearestExistingFolderAtOrAbove(parent);
        return existingParent.isEmpty() ? fallbackFolderPath() : existingParent;
    }

    if (isRestorableFolderPath(trimmed)) {
        return trimmed;
    }
    return fallbackFolderPath();
}

QVariantMap AppSettingsController::sanitizedWindowGeometry(const QVariantMap &state,
                                                           int fallbackWidth,
                                                           int fallbackHeight) const
{
    const int width = boundedInt(state.value(QStringLiteral("windowWidth")), fallbackWidth, 760, 10000);
    const int height = boundedInt(state.value(QStringLiteral("windowHeight")), fallbackHeight, 480, 10000);
    const int x = state.value(QStringLiteral("windowX")).toInt();
    const int y = state.value(QStringLiteral("windowY")).toInt();
    const QRect requested(x, y, width, height);

    QVariantMap result;
    result[QStringLiteral("width")] = width;
    result[QStringLiteral("height")] = height;

    if (state.contains(QStringLiteral("windowX"))
        && state.contains(QStringLiteral("windowY"))
        && rectIntersectsAnyScreen(requested)
        && !looksLikeAccidentallySavedMaximizedGeometry(requested)) {
        result[QStringLiteral("x")] = x;
        result[QStringLiteral("y")] = y;
        result[QStringLiteral("valid")] = true;
        return result;
    }

    const QRect screenRect = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, width, height);
    result[QStringLiteral("x")] = screenRect.center().x() - width / 2;
    result[QStringLiteral("y")] = screenRect.center().y() - height / 2;
    result[QStringLiteral("valid")] = true;
    return result;
}

void AppSettingsController::resetWorkspaceState()
{
    QSettings settings;
    settings.remove(QLatin1String(WorkspaceGroup));
    settings.remove(QStringLiteral("appearance/mode"));
    settings.remove(QStringLiteral("appearance/schemeId"));
    settings.remove(QStringLiteral("appearance/themeFilePath"));
    emit workspaceStateChanged();
    setSettingsMaintenanceStatus(QStringLiteral("Saved workspace and theme were cleared for the next launch."));
}

bool AppSettingsController::exportSettings(const QString &filePath)
{
    const QString localPath = normalizeLocalPath(filePath);
    if (localPath.isEmpty()) {
        setSettingsMaintenanceStatus(QStringLiteral("Settings export failed: invalid target path."));
        return false;
    }

    const QFileInfo info(localPath);
    if (!info.dir().exists()) {
        setSettingsMaintenanceStatus(QStringLiteral("Settings export failed: target folder does not exist."));
        return false;
    }

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setSettingsMaintenanceStatus(QStringLiteral("Settings export failed: could not write the selected file."));
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromVariant(exportableSettings());
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        setSettingsMaintenanceStatus(QStringLiteral("Settings export failed: could not write the JSON payload."));
        return false;
    }

    setSettingsMaintenanceStatus(QStringLiteral("Settings exported to %1.").arg(QDir::toNativeSeparators(localPath)));
    return true;
}

bool AppSettingsController::importSettings(const QString &filePath)
{
    const QString localPath = normalizeLocalPath(filePath);
    if (localPath.isEmpty()) {
        setSettingsMaintenanceStatus(QStringLiteral("Settings import failed: invalid source path."));
        return false;
    }

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setSettingsMaintenanceStatus(QStringLiteral("Settings import failed: could not open the selected file."));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setSettingsMaintenanceStatus(QStringLiteral("Settings import failed: file is not a valid settings JSON document."));
        return false;
    }

    const QJsonObject rootObject = document.object();
    const int version = rootObject.value(QStringLiteral("formatVersion")).toInt();
    if (version != 1 && version != ExportFormatVersion) {
        setSettingsMaintenanceStatus(QStringLiteral("Settings import failed: unsupported settings format version."));
        return false;
    }

    applyAppearanceSettings(variantMapFromJsonObject(rootObject.value(QStringLiteral("appearance")).toObject()));
    if (m_themeController && rootObject.contains(QStringLiteral("theme"))) {
        if (!m_themeController->importState(variantMapFromJsonObject(rootObject.value(QStringLiteral("theme")).toObject()))) {
            setSettingsMaintenanceStatus(QStringLiteral("Settings import failed: theme payload is invalid."));
            return false;
        }
    }
    if (rootObject.contains(QStringLiteral("palette"))) {
        const QVariantMap palette = variantMapFromJsonObject(rootObject.value(QStringLiteral("palette")).toObject());
        QSettings settings;
        settings.beginGroup(QStringLiteral("palette"));
        settings.setValue(QStringLiteral("counts"), palette.value(QStringLiteral("counts")).toMap());
        settings.setValue(QStringLiteral("timestamps"), palette.value(QStringLiteral("timestamps")).toMap());
        settings.endGroup();
    }
    saveWorkspaceState(importWorkspaceState(variantMapFromJsonObject(rootObject.value(QStringLiteral("workspace")).toObject())));
    setSettingsMaintenanceStatus(QStringLiteral("Settings imported from %1.").arg(QDir::toNativeSeparators(localPath)));
    return true;
}

bool AppSettingsController::openAppDataFolder() const
{
    const QString path = appDataLocation();
    if (path.isEmpty()) {
        return false;
    }

    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QVariantMap AppSettingsController::commandUsageStats() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("palette"));
    QVariantMap stats;
    stats[QStringLiteral("counts")] = settings.value(QStringLiteral("counts")).toMap();
    stats[QStringLiteral("timestamps")] = settings.value(QStringLiteral("timestamps")).toMap();
    settings.endGroup();
    return stats;
}

void AppSettingsController::recordCommandExecuted(const QString &commandId)
{
    if (commandId.isEmpty()) {
        return;
    }
    QSettings settings;
    settings.beginGroup(QStringLiteral("palette"));

    QVariantMap counts = settings.value(QStringLiteral("counts")).toMap();
    int count = counts.value(commandId, 0).toInt();
    counts[commandId] = count + 1;
    settings.setValue(QStringLiteral("counts"), counts);

    QVariantMap timestamps = settings.value(QStringLiteral("timestamps")).toMap();
    timestamps[commandId] = QDateTime::currentMSecsSinceEpoch();
    settings.setValue(QStringLiteral("timestamps"), timestamps);

    settings.endGroup();
}

void AppSettingsController::resetCommandUsageStats()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("palette"));
    settings.remove(QStringLiteral("counts"));
    settings.remove(QStringLiteral("timestamps"));
    settings.endGroup();
    setSettingsMaintenanceStatus(QStringLiteral("Command palette usage history was cleared."));
}

QString AppSettingsController::appDataLocation() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!path.isEmpty()) {
        return path;
    }

    path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!path.isEmpty()) {
        return path;
    }

    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

QString AppSettingsController::settingsMaintenanceStatus() const
{
    return m_settingsMaintenanceStatus;
}

int AppSettingsController::settingsFormatVersion() const
{
    return ExportFormatVersion;
}

QVariantMap AppSettingsController::appearanceSettings() const
{
    QVariantMap appearance;
    appearance[QStringLiteral("useNativeIcons")] = m_useNativeIcons;
    appearance[QStringLiteral("useHighQualitySystemIcons")] = m_useHighQualitySystemIcons;
    appearance[QStringLiteral("showThumbnails")] = m_showThumbnails;
    appearance[QStringLiteral("ultraLightMode")] = m_ultraLightMode;
    appearance[QStringLiteral("previewDetailsRaised")] = m_previewDetailsRaised;
    appearance[QStringLiteral("useSystemTrayIcon")] = m_useSystemTrayIcon;
    return appearance;
}

void AppSettingsController::applyAppearanceSettings(const QVariantMap &appearance)
{
    setUseNativeIcons(appearance.value(QStringLiteral("useNativeIcons"), m_useNativeIcons).toBool());
    setUseHighQualitySystemIcons(appearance.value(QStringLiteral("useHighQualitySystemIcons"),
                                                  m_useHighQualitySystemIcons).toBool());
    setShowThumbnails(appearance.value(QStringLiteral("showThumbnails"), m_showThumbnails).toBool());
    setUltraLightMode(appearance.value(QStringLiteral("ultraLightMode"),
                                       appearance.value(QStringLiteral("simplifyVisualsForPerformance"),
                                                        m_ultraLightMode)).toBool());
    setPreviewDetailsRaised(appearance.value(QStringLiteral("previewDetailsRaised"),
                                             m_previewDetailsRaised).toBool());
    setUseSystemTrayIcon(appearance.value(QStringLiteral("useSystemTrayIcon"),
                                          m_useSystemTrayIcon).toBool());
}

QVariantMap AppSettingsController::exportableSettings() const
{
    QVariantMap root;
    root[QStringLiteral("formatVersion")] = ExportFormatVersion;
    root[QStringLiteral("appearance")] = appearanceSettings();
    if (m_themeController) {
        root[QStringLiteral("theme")] = m_themeController->exportState();
    }
    root[QStringLiteral("palette")] = commandUsageStats();
    root[QStringLiteral("workspace")] = exportWorkspaceState(workspaceState());
    return root;
}

QVariantMap AppSettingsController::exportWorkspaceState(const QVariantMap &workspace) const
{
    QVariantMap exported = workspace;
    const QVariant splitState = workspace.value(QStringLiteral("fileWorkspaceSplitState"));
    if (splitState.metaType().id() == QMetaType::QByteArray) {
        QVariantMap encoded;
        encoded[QLatin1String(ByteArrayEncodingKey)] = QLatin1String(ByteArrayEncodingBase64);
        encoded[QLatin1String(ByteArrayDataKey)] = QString::fromLatin1(splitState.toByteArray().toBase64());
        exported[QStringLiteral("fileWorkspaceSplitState")] = encoded;
    }
    return exported;
}

QVariantMap AppSettingsController::importWorkspaceState(const QVariantMap &workspace) const
{
    QVariantMap imported = workspace;
    const QVariant splitState = workspace.value(QStringLiteral("fileWorkspaceSplitState"));
    if (splitState.metaType().id() == QMetaType::QVariantMap) {
        const QVariantMap encoded = splitState.toMap();
        const QString encoding = encoded.value(QLatin1String(ByteArrayEncodingKey)).toString();
        if (encoding == QLatin1String(ByteArrayEncodingBase64)) {
            const QByteArray decoded = QByteArray::fromBase64(encoded.value(QLatin1String(ByteArrayDataKey)).toString().toLatin1());
            imported[QStringLiteral("fileWorkspaceSplitState")] = decoded;
        }
    }
    return imported;
}

void AppSettingsController::setSettingsMaintenanceStatus(const QString &status)
{
    if (m_settingsMaintenanceStatus == status) {
        return;
    }

    m_settingsMaintenanceStatus = status;
    emit settingsMaintenanceStatusChanged();
}

QString AppSettingsController::normalizeLocalPath(const QString &filePath) const
{
    if (filePath.isEmpty()) {
        return QString();
    }

    const QUrl url(filePath);
    if (url.isValid() && url.isLocalFile()) {
        return url.toLocalFile();
    }

    return filePath;
}

QString AppSettingsController::fallbackFolderPath() const
{
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty() && QFileInfo(home).isDir()) {
        return home;
    }
    return QLatin1String(DeviceRoot);
}

bool AppSettingsController::isRestorableFolderPath(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }
    if (path == QLatin1String(DeviceRoot)) {
        return true;
    }
    if (path == QLatin1String(FavoritesRoot)) {
        return true;
    }
    if (ArchiveSupport::isArchivePath(path)) {
        return QFileInfo::exists(ArchiveSupport::physicalArchivePath(path));
    }

    const QFileInfo info(path);
    return info.exists() && info.isDir();
}
