#include "AppSettingsController.h"

#include "../core/ArchiveSupport.h"
#include "ThemeController.h"

#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
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

#include <algorithm>

namespace {
constexpr auto WorkspaceGroup = "workspace";
constexpr auto AppearanceGroup = "appearance";
constexpr auto DeviceRoot = "devices://";
constexpr auto FavoritesRoot = "favorites://";
constexpr auto ExportFormatVersion = 2;
constexpr auto ByteArrayEncodingKey = "__encoding";
constexpr auto ByteArrayDataKey = "data";
constexpr auto ByteArrayEncodingBase64 = "base64";
constexpr int DefaultFontScale = 100;
constexpr int MinFontScale = 90;
constexpr int MaxFontScale = 150;

bool hasExplicitNonLocalScheme(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return false;
    }

    const QString scheme = trimmed.left(separatorIndex).toLower();
    if (scheme == QLatin1String("file")) {
        return false;
    }

    if (!scheme.at(0).isLetter()) {
        return false;
    }
    for (const QChar ch : scheme) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('+') && ch != QLatin1Char('.') && ch != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

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

QString normalizedFontFamily(QString family)
{
    return family.trimmed();
}

struct RoleInfo {
    QString id;
    QString name;
    QString description;
    QString sample;
    QString fallbackToken;
    QString group;
};

const QList<RoleInfo>& getMvpRoles() {
    static const QList<RoleInfo> roles = {
        {QStringLiteral("fileNameText"), QStringLiteral("File Name"), QStringLiteral("Text color for files in the panel"), QStringLiteral("file_name.txt"), QStringLiteral("textPrimary"), QStringLiteral("File panels")},
        {QStringLiteral("folderNameText"), QStringLiteral("Folder Name"), QStringLiteral("Text color for folders in the panel"), QStringLiteral("Folder"), QStringLiteral("textPrimary"), QStringLiteral("File panels")},
        {QStringLiteral("fileExtensionText"), QStringLiteral("File Extension"), QStringLiteral("Text color for file extensions"), QStringLiteral("txt"), QStringLiteral("textSecondary"), QStringLiteral("File panels")},
        {QStringLiteral("fileSecondaryText"), QStringLiteral("Secondary Details"), QStringLiteral("Text color for file size, date, etc."), QStringLiteral("1.2 MB"), QStringLiteral("textSecondary"), QStringLiteral("File panels")},
        {QStringLiteral("filePathText"), QStringLiteral("File Path"), QStringLiteral("Text color for path snippets"), QStringLiteral("/home/user/docs"), QStringLiteral("textSecondary"), QStringLiteral("File panels")},
        {QStringLiteral("sidebarText"), QStringLiteral("Sidebar Item"), QStringLiteral("Text color for sidebar labels"), QStringLiteral("Documents"), QStringLiteral("textPrimary"), QStringLiteral("Navigation")},
        {QStringLiteral("thisPcText"), QStringLiteral("This PC Labels"), QStringLiteral("Text color for drives and storage overview labels"), QStringLiteral("Local Disk (C:)"), QStringLiteral("textPrimary"), QStringLiteral("Navigation")},
        {QStringLiteral("statusText"), QStringLiteral("Status Text"), QStringLiteral("Text color for panel footers and status summaries"), QStringLiteral("12 items selected"), QStringLiteral("textSecondary"), QStringLiteral("App chrome and workflows")},
        {QStringLiteral("dialogSecondaryText"), QStringLiteral("Dialog Secondary Text"), QStringLiteral("Text color for secondary labels in dialogs"), QStringLiteral("Press Space to preview"), QStringLiteral("textSecondary"), QStringLiteral("App chrome and workflows")},
        {QStringLiteral("commandPaletteText"), QStringLiteral("Command Palette Text"), QStringLiteral("Text color for command palette names, hints, shortcuts"), QStringLiteral("Ctrl+Shift+P"), QStringLiteral("textPrimary"), QStringLiteral("App chrome and workflows")}
    };
    return roles;
}

}

AppSettingsController::AppSettingsController(QObject *parent)
    : QObject(parent)
{
    m_defaultApplicationFont = QGuiApplication::font();
    m_availableFontFamilies = QFontDatabase().families();
    std::sort(m_availableFontFamilies.begin(), m_availableFontFamilies.end(),
              [](const QString &lhs, const QString &rhs) {
                  return QString::localeAwareCompare(lhs, rhs) < 0;
              });
    m_availableFontFamilies.removeDuplicates();

    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    m_useNativeIcons = settings.value(QStringLiteral("useNativeIcons"), true).toBool();
    m_useHighQualitySystemIcons = settings.value(QStringLiteral("useHighQualitySystemIcons"), true).toBool();
    m_showThumbnails = settings.value(QStringLiteral("showThumbnails"), true).toBool();
    m_ultraLightMode = settings.value(QStringLiteral("ultraLightMode"),
                                      settings.value(QStringLiteral("simplifyVisualsForPerformance"), false)).toBool();
    m_useGradientColors = settings.value(QStringLiteral("useGradientColors"), true).toBool();
    m_commandPaletteTransparency = settings.value(QStringLiteral("commandPaletteTransparency"), true).toBool();
    m_shellFirstQmlRestore = settings.value(QStringLiteral("shellFirstQmlRestore"), false).toBool();
    settings.remove(QStringLiteral("useNativeFileEnumerators"));
    m_previewDetailsRaised = settings.value(QStringLiteral("previewDetailsRaised"), false).toBool();
    m_useSystemTrayIcon = settings.value(QStringLiteral("useSystemTrayIcon"), false).toBool();
    m_allowOnlyOneInstance = settings.value(QStringLiteral("allowOnlyOneInstance"), false).toBool();
    m_useLimitedDragNDrop = settings.value(QStringLiteral("useLimitedDragNDrop"), false).toBool();
    m_fontFamily = normalizedFontFamily(settings.value(QStringLiteral("fontFamily")).toString());
    m_fontScale = boundedInt(settings.value(QStringLiteral("fontScale")), DefaultFontScale, MinFontScale, MaxFontScale);
    m_textColorOverrides = settings.value(QStringLiteral("textColorOverrides")).toMap();
    
    // Validate loaded overrides
    bool loadedChanged = false;
    for (auto it = m_textColorOverrides.begin(); it != m_textColorOverrides.end(); ++it) {
        const QString &roleId = it.key();
        bool isMvp = false;
        for (const RoleInfo &info : getMvpRoles()) {
            if (info.id == roleId) {
                isMvp = true;
                break;
            }
        }
        if (isMvp) {
            QVariantMap entry = it.value().toMap();
            if (entry.value(QStringLiteral("enabled"), false).toBool()) {
                QString color = entry.value(QStringLiteral("color")).toString();
                if (!QColor::isValidColorName(color)) {
                    entry[QStringLiteral("enabled")] = false;
                    *it = entry;
                    loadedChanged = true;
                }
            }
        }
    }
    if (loadedChanged) {
        settings.setValue(QStringLiteral("textColorOverrides"), m_textColorOverrides);
    }
    
    settings.endGroup();
    applyApplicationFont();
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

bool AppSettingsController::useGradientColors() const
{
    return m_useGradientColors;
}

void AppSettingsController::setUseGradientColors(bool enabled)
{
    if (m_useGradientColors == enabled) {
        return;
    }

    m_useGradientColors = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("useGradientColors"), m_useGradientColors);
    settings.endGroup();
    emit useGradientColorsChanged();
}

bool AppSettingsController::commandPaletteTransparency() const
{
    return m_commandPaletteTransparency;
}

void AppSettingsController::setCommandPaletteTransparency(bool enabled)
{
    if (m_commandPaletteTransparency == enabled) {
        return;
    }

    m_commandPaletteTransparency = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("commandPaletteTransparency"), m_commandPaletteTransparency);
    settings.endGroup();
    emit commandPaletteTransparencyChanged();
}

bool AppSettingsController::shellFirstQmlRestore() const
{
    return m_shellFirstQmlRestore;
}

void AppSettingsController::setShellFirstQmlRestore(bool enabled)
{
    if (m_shellFirstQmlRestore == enabled) {
        return;
    }

    m_shellFirstQmlRestore = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("shellFirstQmlRestore"), m_shellFirstQmlRestore);
    settings.endGroup();
    emit shellFirstQmlRestoreChanged();
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

bool AppSettingsController::allowOnlyOneInstance() const
{
    return m_allowOnlyOneInstance;
}

void AppSettingsController::setAllowOnlyOneInstance(bool enabled)
{
    if (m_allowOnlyOneInstance == enabled) {
        return;
    }

    m_allowOnlyOneInstance = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("allowOnlyOneInstance"), m_allowOnlyOneInstance);
    settings.endGroup();
    emit allowOnlyOneInstanceChanged();
}

bool AppSettingsController::useLimitedDragNDrop() const
{
    return m_useLimitedDragNDrop;
}

void AppSettingsController::setUseLimitedDragNDrop(bool enabled)
{
    if (m_useLimitedDragNDrop == enabled) {
        return;
    }

    m_useLimitedDragNDrop = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("useLimitedDragNDrop"), m_useLimitedDragNDrop);
    settings.endGroup();
    emit useLimitedDragNDropChanged();
}

QString AppSettingsController::fontFamily() const
{
    return m_fontFamily;
}

QString AppSettingsController::resolvedFontFamily() const
{
    return m_fontFamily.isEmpty() ? m_defaultApplicationFont.family() : m_fontFamily;
}

void AppSettingsController::setFontFamily(const QString &family)
{
    const QString normalized = normalizedFontFamily(family);
    if (m_fontFamily == normalized) {
        return;
    }

    m_fontFamily = normalized;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("fontFamily"), m_fontFamily);
    settings.endGroup();
    applyApplicationFont();
    emit fontFamilyChanged();
}

int AppSettingsController::fontScale() const
{
    return m_fontScale;
}

void AppSettingsController::setFontScale(int scale)
{
    const int normalized = qBound(MinFontScale, scale, MaxFontScale);
    if (m_fontScale == normalized) {
        return;
    }

    m_fontScale = normalized;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("fontScale"), m_fontScale);
    settings.endGroup();
    emit fontScaleChanged();
}

QStringList AppSettingsController::availableFontFamilies() const
{
    return m_availableFontFamilies;
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
    state[QStringLiteral("leftShowSelectionBadges")] = settings.value(QStringLiteral("leftShowSelectionBadges"), true).toBool();
    state[QStringLiteral("rightShowSelectionBadges")] = settings.value(QStringLiteral("rightShowSelectionBadges"), true).toBool();
    state[QStringLiteral("leftShowHoverPreviews")] = settings.value(QStringLiteral("leftShowHoverPreviews"), false).toBool();
    state[QStringLiteral("rightShowHoverPreviews")] = settings.value(QStringLiteral("rightShowHoverPreviews"), false).toBool();
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
    settings.setValue(QStringLiteral("leftPath"),
                      safeFolderPathForSave(state.value(QStringLiteral("leftPath")).toString(),
                                            settings.value(QStringLiteral("leftPath")).toString()));
    settings.setValue(QStringLiteral("rightPath"),
                      safeFolderPathForSave(state.value(QStringLiteral("rightPath")).toString(),
                                            settings.value(QStringLiteral("rightPath")).toString()));
    settings.setValue(QStringLiteral("leftViewMode"), boundedInt(state.value(QStringLiteral("leftViewMode")), 0, 0, 2));
    settings.setValue(QStringLiteral("rightViewMode"), boundedInt(state.value(QStringLiteral("rightViewMode")), 0, 0, 2));
    settings.setValue(QStringLiteral("leftGridIconSize"), boundedInt(state.value(QStringLiteral("leftGridIconSize")), 48, 32, 96));
    settings.setValue(QStringLiteral("rightGridIconSize"), boundedInt(state.value(QStringLiteral("rightGridIconSize")), 48, 32, 96));
    settings.setValue(QStringLiteral("leftBriefRowHeight"), boundedInt(state.value(QStringLiteral("leftBriefRowHeight")), 28, 22, 64));
    settings.setValue(QStringLiteral("rightBriefRowHeight"), boundedInt(state.value(QStringLiteral("rightBriefRowHeight")), 28, 22, 64));
    settings.setValue(QStringLiteral("leftShowSelectionBadges"), state.value(QStringLiteral("leftShowSelectionBadges"), true).toBool());
    settings.setValue(QStringLiteral("rightShowSelectionBadges"), state.value(QStringLiteral("rightShowSelectionBadges"), true).toBool());
    settings.setValue(QStringLiteral("leftShowHoverPreviews"), state.value(QStringLiteral("leftShowHoverPreviews"), false).toBool());
    settings.setValue(QStringLiteral("rightShowHoverPreviews"), state.value(QStringLiteral("rightShowHoverPreviews"), false).toBool());
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
    if (hasExplicitNonLocalScheme(trimmed)) {
        return fallbackFolderPath();
    }
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

QString AppSettingsController::safeFolderPathForSave(const QString &path, const QString &previousPath) const
{
    const QString trimmed = path.trimmed();
    if (hasExplicitNonLocalScheme(trimmed)) {
        const QString previous = safeFolderPath(previousPath);
        return previous.isEmpty() ? fallbackFolderPath() : previous;
    }
    return safeFolderPath(trimmed);
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
    appearance[QStringLiteral("useGradientColors")] = m_useGradientColors;
    appearance[QStringLiteral("commandPaletteTransparency")] = m_commandPaletteTransparency;
    appearance[QStringLiteral("shellFirstQmlRestore")] = m_shellFirstQmlRestore;
    appearance[QStringLiteral("previewDetailsRaised")] = m_previewDetailsRaised;
    appearance[QStringLiteral("useSystemTrayIcon")] = m_useSystemTrayIcon;
    appearance[QStringLiteral("allowOnlyOneInstance")] = m_allowOnlyOneInstance;
    appearance[QStringLiteral("useLimitedDragNDrop")] = m_useLimitedDragNDrop;
    appearance[QStringLiteral("fontFamily")] = m_fontFamily;
    appearance[QStringLiteral("fontScale")] = m_fontScale;
    appearance[QStringLiteral("textColorOverrides")] = m_textColorOverrides;
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
    setUseGradientColors(appearance.value(QStringLiteral("useGradientColors"),
                                          m_useGradientColors).toBool());
    setCommandPaletteTransparency(appearance.value(QStringLiteral("commandPaletteTransparency"),
                                                   m_commandPaletteTransparency).toBool());
    setShellFirstQmlRestore(appearance.value(QStringLiteral("shellFirstQmlRestore"),
                                             m_shellFirstQmlRestore).toBool());
    setPreviewDetailsRaised(appearance.value(QStringLiteral("previewDetailsRaised"),
                                             m_previewDetailsRaised).toBool());
    setUseSystemTrayIcon(appearance.value(QStringLiteral("useSystemTrayIcon"),
                                          m_useSystemTrayIcon).toBool());
    setAllowOnlyOneInstance(appearance.value(QStringLiteral("allowOnlyOneInstance"),
                                             m_allowOnlyOneInstance).toBool());
    setUseLimitedDragNDrop(appearance.value(QStringLiteral("useLimitedDragNDrop"),
                                            m_useLimitedDragNDrop).toBool());
    setFontFamily(appearance.value(QStringLiteral("fontFamily"), m_fontFamily).toString());
    setFontScale(appearance.value(QStringLiteral("fontScale"), m_fontScale).toInt());
    if (appearance.contains(QStringLiteral("textColorOverrides"))) {
        saveTextColorOverrides(appearance.value(QStringLiteral("textColorOverrides")).toMap());
    }
}

void AppSettingsController::applyApplicationFont() const
{
    QFont applicationFont = m_defaultApplicationFont;
    if (!m_fontFamily.isEmpty()) {
        applicationFont.setFamily(m_fontFamily);
    }
    QGuiApplication::setFont(applicationFont);
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
    return QDir::rootPath();
}

bool AppSettingsController::isRestorableFolderPath(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }
    if (ArchiveSupport::isArchivePath(path)) {
        return QFileInfo::exists(ArchiveSupport::physicalArchivePath(path));
    }
    if (path == QLatin1String(DeviceRoot) || path == QLatin1String(FavoritesRoot)) {
        return false;
    }
    if (hasExplicitNonLocalScheme(path)) {
        return false;
    }

    const QFileInfo info(path);
    return info.exists() && info.isDir();
}

QVariantMap AppSettingsController::textColorOverrides() const
{
    return m_textColorOverrides;
}

void AppSettingsController::setTextColorOverrides(const QVariantMap &overrides)
{
    saveTextColorOverrides(overrides);
}

bool AppSettingsController::isOverrideEnabled(const QString &roleId) const
{
    if (!m_textColorOverrides.contains(roleId)) {
        return false;
    }
    const QVariantMap entry = m_textColorOverrides.value(roleId).toMap();
    if (!entry.value(QStringLiteral("enabled"), false).toBool()) {
        return false;
    }
    const QString color = entry.value(QStringLiteral("color")).toString();
    return QColor::isValidColorName(color);
}

QString AppSettingsController::overrideColor(const QString &roleId) const
{
    if (isOverrideEnabled(roleId)) {
        return m_textColorOverrides.value(roleId).toMap().value(QStringLiteral("color")).toString();
    }
    return QString();
}

void AppSettingsController::setRoleOverride(const QString &roleId, const QString &color)
{
    QVariantMap entry = m_textColorOverrides.value(roleId).toMap();
    entry[QStringLiteral("color")] = color.trimmed();
    bool valid = QColor::isValidColorName(entry[QStringLiteral("color")].toString());
    entry[QStringLiteral("enabled")] = valid;
    
    m_textColorOverrides[roleId] = entry;
    
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("textColorOverrides"), m_textColorOverrides);
    settings.endGroup();
    
    emit textColorOverridesChanged();
}

void AppSettingsController::setRoleEnabled(const QString &roleId, bool enabled)
{
    QVariantMap entry = m_textColorOverrides.value(roleId).toMap();

    const QString color = entry.value(QStringLiteral("color")).toString().trimmed();
    entry[QStringLiteral("color")] = color;
    if (enabled && !QColor::isValidColorName(color)) {
        qWarning() << "Cannot enable text color override for role" << roleId
        << "because the stored color is invalid:" << color;
        enabled = false;
    }

    entry[QStringLiteral("enabled")] = enabled;
    
    m_textColorOverrides[roleId] = entry;
    
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("textColorOverrides"), m_textColorOverrides);
    settings.endGroup();
    
    emit textColorOverridesChanged();
}

void AppSettingsController::resetRole(const QString &roleId)
{
    QVariantMap entry = m_textColorOverrides.value(roleId).toMap();
    entry[QStringLiteral("enabled")] = false;
    
    m_textColorOverrides[roleId] = entry;
    
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("textColorOverrides"), m_textColorOverrides);
    settings.endGroup();
    
    emit textColorOverridesChanged();
}

void AppSettingsController::resetAll()
{
    bool changed = false;
    for (const RoleInfo &info : getMvpRoles()) {
        if (m_textColorOverrides.contains(info.id)) {
            QVariantMap entry = m_textColorOverrides.value(info.id).toMap();
            if (entry.value(QStringLiteral("enabled"), false).toBool()) {
                entry[QStringLiteral("enabled")] = false;
                m_textColorOverrides[info.id] = entry;
                changed = true;
            }
        }
    }
    
    if (changed) {
        QSettings settings;
        settings.beginGroup(QLatin1String(AppearanceGroup));
        settings.setValue(QStringLiteral("textColorOverrides"), m_textColorOverrides);
        settings.endGroup();
        
        emit textColorOverridesChanged();
    }
}

QVariantList AppSettingsController::rolesMetadata() const
{
    QVariantList list;
    for (const RoleInfo &info : getMvpRoles()) {
        QVariantMap map;
        map[QStringLiteral("id")] = info.id;
        map[QStringLiteral("name")] = info.name;
        map[QStringLiteral("description")] = info.description;
        map[QStringLiteral("sample")] = info.sample;
        map[QStringLiteral("fallbackToken")] = info.fallbackToken;
        map[QStringLiteral("group")] = info.group;
        list.append(map);
    }
    return list;
}

void AppSettingsController::saveTextColorOverrides(const QVariantMap &overrides)
{
    QVariantMap merged = m_textColorOverrides;
    
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        const QString &roleId = it.key();
        QVariantMap entry = it.value().toMap();
        
        bool enabled = entry.value(QStringLiteral("enabled"), false).toBool();
        QString color = entry.value(QStringLiteral("color")).toString().trimmed();
        
        if (enabled && !color.isEmpty()) {
            if (!QColor::isValidColorName(color)) {
                qWarning() << "Invalid override color for role" << roleId << ":" << color;
                enabled = false;
            }
        } else {
            enabled = false;
        }
        
        QVariantMap validatedEntry;
        validatedEntry[QStringLiteral("enabled")] = enabled;
        validatedEntry[QStringLiteral("color")] = color;
        
        merged[roleId] = validatedEntry;
    }
    
    if (m_textColorOverrides != merged) {
        m_textColorOverrides = merged;
        QSettings settings;
        settings.beginGroup(QLatin1String(AppearanceGroup));
        settings.setValue(QStringLiteral("textColorOverrides"), m_textColorOverrides);
        settings.endGroup();
        emit textColorOverridesChanged();
    }
}
