#include "AppSettingsController.h"

#include "../core/ArchiveSupport.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QRect>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>

namespace {
constexpr auto WorkspaceGroup = "workspace";
constexpr auto AppearanceGroup = "appearance";
constexpr auto DeviceRoot = "devices://";

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
}

AppSettingsController::AppSettingsController(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    m_useNativeIcons = settings.value(QStringLiteral("useNativeIcons"), true).toBool();
    m_useHighQualitySystemIcons = settings.value(QStringLiteral("useHighQualitySystemIcons"), true).toBool();
    m_showThumbnails = settings.value(QStringLiteral("showThumbnails"), true).toBool();
    m_simplifyVisualsForPerformance = settings.value(QStringLiteral("simplifyVisualsForPerformance"), true).toBool();
    settings.endGroup();
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

bool AppSettingsController::simplifyVisualsForPerformance() const
{
    return m_simplifyVisualsForPerformance;
}

void AppSettingsController::setSimplifyVisualsForPerformance(bool enabled)
{
    if (m_simplifyVisualsForPerformance == enabled) {
        return;
    }

    m_simplifyVisualsForPerformance = enabled;
    QSettings settings;
    settings.beginGroup(QLatin1String(AppearanceGroup));
    settings.setValue(QStringLiteral("simplifyVisualsForPerformance"), m_simplifyVisualsForPerformance);
    settings.endGroup();
    emit simplifyVisualsForPerformanceChanged();
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
    settings.setValue(QStringLiteral("showHidden"), state.value(QStringLiteral("showHidden")).toBool());

    settings.endGroup();
    emit workspaceStateChanged();
}

QString AppSettingsController::safeFolderPath(const QString &path) const
{
    if (isRestorableFolderPath(path)) {
        return path;
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
    emit workspaceStateChanged();
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
    if (ArchiveSupport::isArchivePath(path)) {
        return QFileInfo::exists(ArchiveSupport::physicalArchivePath(path));
    }

    const QFileInfo info(path);
    return info.exists() && info.isDir();
}
