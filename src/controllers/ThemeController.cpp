#include "ThemeController.h"

#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {
QString normalizeThemeFilePath(const QString &value)
{
    if (value.isEmpty()) {
        return {};
    }

    const QUrl url(value);
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }

    return value;
}

ThemeController::ThemePalette makePalette(
    const QString &id,
    const QString &name,
    bool dark,
    const QColor &bg,
    const QColor &surface,
    const QColor &surfaceHover,
    const QColor &surfaceActive,
    const QColor &textPrimary,
    const QColor &textSecondary,
    const QColor &border,
    const QColor &accent,
    const QColor &accentText,
    const QColor &danger,
    const QColor &activeAccent,
    const QColor &activeGlow,
    const QColor &secondaryAccent,
    const QColor &warmAccent,
    const QColor &success,
    const QColor &warning,
    const QColor &categoryInfo,
    const QColor &categoryNavigation,
    const QColor &categoryAction,
    const QColor &categoryUtility,
    const QColor &categorySystem)
{
    auto alpha = [](const QColor &color, qreal value) {
        QColor c = color;
        c.setAlphaF(value);
        return c;
    };

    ThemeController::ThemePalette palette;
    palette.id = id;
    palette.name = name;
    palette.dark = dark;
    palette.bg = bg;
    palette.surface = surface;
    palette.surfaceHover = surfaceHover;
    palette.surfaceActive = surfaceActive;
    palette.textPrimary = textPrimary;
    palette.textSecondary = textSecondary;
    palette.border = border;
    palette.accent = accent;
    palette.accentText = accentText;
    palette.danger = danger;
    palette.activeAccent = activeAccent;
    palette.activeGlow = activeGlow;
    palette.secondaryAccent = secondaryAccent;
    palette.warmAccent = warmAccent;
    palette.success = success;
    palette.warning = warning;
    palette.categoryInfo = categoryInfo;
    palette.categoryNavigation = categoryNavigation;
    palette.categoryAction = categoryAction;
    palette.categoryUtility = categoryUtility;
    palette.categorySystem = categorySystem;
    palette.overlayScrim = alpha(bg, dark ? 0.52 : 0.30);
    palette.focusRing = alpha(accent, dark ? 0.82 : 0.88);
    palette.panelSurface = surface;
    palette.panelSurfaceSoft = dark
        ? alpha(surface, 0.56)
        : alpha(bg, 0.48);
    palette.panelSurfaceStrong = dark
        ? alpha(surface, 0.90)
        : alpha(bg, 0.84);
    palette.panelBorder = dark
        ? alpha(Qt::white, 0.14)
        : alpha(border, 0.72);
    palette.controlSurface = surfaceHover;
    palette.controlSurfaceActive = surfaceActive;
    palette.controlBorder = border;
    palette.itemHoverFill = dark
        ? alpha(Qt::white, 0.10)
        : alpha(accent, 0.13);
    palette.itemCurrentFill = dark
        ? alpha(Qt::white, 0.08)
        : alpha(accent, 0.09);
    palette.itemCurrentBorder = dark
        ? alpha(Qt::white, 0.25)
        : alpha(accent, 0.55);
    palette.itemSelectedFill = dark
        ? alpha(Qt::white, 0.18)
        : alpha(accent, 0.13);
    palette.itemSelectedFillInactive = dark
        ? alpha(Qt::white, 0.12)
        : alpha(accent, 0.09);
    palette.itemSelectedBorder = dark
        ? alpha(Qt::white, 0.35)
        : alpha(accent, 0.85);
    palette.itemSelectedBorderInactive = dark
        ? alpha(Qt::white, 0.20)
        : alpha(accent, 0.55);
    palette.statusRailFill = dark
        ? alpha(surface, 0.98)
        : alpha(bg, 0.995);
    return palette;
}

QString normalizedThemeIdKey(const QString &value)
{
    return value.trimmed().toLower();
}
}

ThemeController::ThemeController(QObject *parent)
    : QObject(parent)
{
    updateSystemTheme();
    loadSettings();
}

ThemeController::ThemeMode ThemeController::mode() const
{
    return m_mode;
}

void ThemeController::setMode(ThemeMode mode)
{
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;
    emit modeChanged();

    if (mode == Light) {
        applyBuiltInScheme(CatppuccinLatte);
    } else if (mode == Dark) {
        applyBuiltInScheme(AuroraGlass);
    } else {
        applyBuiltInScheme(defaultSchemeForSystem());
    }
}

ThemeController::ThemeScheme ThemeController::scheme() const
{
    return m_scheme;
}

void ThemeController::setScheme(ThemeScheme scheme)
{
    applyBuiltInScheme(scheme);
}

QString ThemeController::schemeName() const
{
    return activePalette().name;
}

bool ThemeController::customThemeLoaded() const
{
    return m_hasCustomPalette;
}

QString ThemeController::themeFilePath() const
{
    return m_customThemePath;
}

bool ThemeController::isDark() const
{
    return activePalette().dark;
}

QColor ThemeController::bg() const { return activePalette().bg; }
QColor ThemeController::surface() const { return activePalette().surface; }
QColor ThemeController::surfaceHover() const { return activePalette().surfaceHover; }
QColor ThemeController::surfaceActive() const { return activePalette().surfaceActive; }
QColor ThemeController::textPrimary() const { return activePalette().textPrimary; }
QColor ThemeController::textSecondary() const { return activePalette().textSecondary; }
QColor ThemeController::border() const { return activePalette().border; }
QColor ThemeController::accent() const { return activePalette().accent; }
QColor ThemeController::accentText() const { return activePalette().accentText; }
QColor ThemeController::danger() const { return activePalette().danger; }
QColor ThemeController::activeAccent() const { return activePalette().activeAccent; }
QColor ThemeController::activeGlow() const { return activePalette().activeGlow; }
QColor ThemeController::secondaryAccent() const { return activePalette().secondaryAccent; }
QColor ThemeController::warmAccent() const { return activePalette().warmAccent; }
QColor ThemeController::success() const { return activePalette().success; }
QColor ThemeController::warning() const { return activePalette().warning; }
QColor ThemeController::categoryInfo() const { return activePalette().categoryInfo; }
QColor ThemeController::categoryNavigation() const { return activePalette().categoryNavigation; }
QColor ThemeController::categoryAction() const { return activePalette().categoryAction; }
QColor ThemeController::categoryUtility() const { return activePalette().categoryUtility; }
QColor ThemeController::categorySystem() const { return activePalette().categorySystem; }
QColor ThemeController::overlayScrim() const { return activePalette().overlayScrim; }
QColor ThemeController::focusRing() const { return activePalette().focusRing; }
QColor ThemeController::panelSurface() const { return activePalette().panelSurface; }
QColor ThemeController::panelSurfaceSoft() const { return activePalette().panelSurfaceSoft; }
QColor ThemeController::panelSurfaceStrong() const { return activePalette().panelSurfaceStrong; }
QColor ThemeController::panelBorder() const { return activePalette().panelBorder; }
QColor ThemeController::controlSurface() const { return activePalette().controlSurface; }
QColor ThemeController::controlSurfaceActive() const { return activePalette().controlSurfaceActive; }
QColor ThemeController::controlBorder() const { return activePalette().controlBorder; }
QColor ThemeController::itemHoverFill() const { return activePalette().itemHoverFill; }
QColor ThemeController::itemCurrentFill() const { return activePalette().itemCurrentFill; }
QColor ThemeController::itemCurrentBorder() const { return activePalette().itemCurrentBorder; }
QColor ThemeController::itemSelectedFill() const { return activePalette().itemSelectedFill; }
QColor ThemeController::itemSelectedFillInactive() const { return activePalette().itemSelectedFillInactive; }
QColor ThemeController::itemSelectedBorder() const { return activePalette().itemSelectedBorder; }
QColor ThemeController::itemSelectedBorderInactive() const { return activePalette().itemSelectedBorderInactive; }
QColor ThemeController::statusRailFill() const { return activePalette().statusRailFill; }

bool ThemeController::saveThemeToFile(const QString &filePath) const
{
    const QString path = normalizeThemeFilePath(filePath);
    if (path.isEmpty()) {
        return false;
    }

    const QJsonObject root = themeJsonObject(activePalette());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0) {
        return false;
    }

    return file.commit();
}

bool ThemeController::loadThemeFromFile(const QString &filePath)
{
    return loadThemeFromFileInternal(filePath, true);
}

QVariantMap ThemeController::currentThemeState() const
{
    return exportState();
}

bool ThemeController::applyThemeState(const QVariantMap &state)
{
    return importState(state);
}

QVariantMap ThemeController::readThemeStateFromFile(const QString &filePath) const
{
    const QString path = normalizeThemeFilePath(filePath);
    if (path.isEmpty()) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return {};
    }

    ThemePalette palette;
    const QVariantMap state = doc.object().toVariantMap();
    if (!paletteFromState(state, &palette)) {
        return {};
    }

    QVariantMap normalized = themeStateFromPalette(palette);
    normalized[QStringLiteral("customThemeLoaded")] = true;
    normalized[QStringLiteral("themeFilePath")] = path;
    normalized[QStringLiteral("schemeId")] = palette.id;
    normalized[QStringLiteral("schemeName")] = palette.name;
    normalized[QStringLiteral("mode")] = palette.dark
        ? QStringLiteral("dark")
        : QStringLiteral("light");
    return normalized;
}

bool ThemeController::writeThemeStateToFile(const QVariantMap &state, const QString &filePath) const
{
    const QString path = normalizeThemeFilePath(filePath);
    if (path.isEmpty()) {
        return false;
    }

    ThemePalette palette;
    if (!paletteFromState(state, &palette)) {
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    if (file.write(QJsonDocument(themeJsonObject(palette)).toJson(QJsonDocument::Indented)) < 0) {
        return false;
    }

    return file.commit();
}

QVariantMap ThemeController::defaultThemeDraft() const
{
    const ThemePalette palette = makePalette(
        QString(),
        QString(),
        true,
        QColor(QStringLiteral("#13161A")),
        QColor(QStringLiteral("#1D2228")),
        QColor(QStringLiteral("#252B33")),
        QColor(QStringLiteral("#2D3540")),
        QColor(QStringLiteral("#EEF2F6")),
        QColor(QStringLiteral("#A8B0BA")),
        QColor(QStringLiteral("#3B4652")),
        QColor(QStringLiteral("#7AA2D3")),
        QColor(QStringLiteral("#F7FAFC")),
        QColor(QStringLiteral("#D96C7A")),
        QColor(QStringLiteral("#8AAEDC")),
        QColor(QStringLiteral("#7AA2D3")),
        QColor(QStringLiteral("#8CB5A1")),
        QColor(QStringLiteral("#D0A56C")),
        QColor(QStringLiteral("#7EB892")),
        QColor(QStringLiteral("#D2A14F")),
        QColor(QStringLiteral("#7EB6D9")),
        QColor(QStringLiteral("#A993D6")),
        QColor(QStringLiteral("#7AA2D3")),
        QColor(QStringLiteral("#8CB5A1")),
        QColor(QStringLiteral("#C97A5A")));
    return themeStateFromPalette(palette);
}

bool ThemeController::isThemeIdAvailable(const QString &themeId, const QString &excludeFilePath) const
{
    const QString requestedId = normalizedThemeIdKey(themeId);
    if (requestedId.isEmpty()) {
        return false;
    }

    bool builtInOk = false;
    schemeFromId(requestedId, &builtInOk);
    if (builtInOk) {
        return false;
    }

    const QString excludedPath = normalizeThemeFilePath(excludeFilePath);
    const QVariantList customThemes = availableCustomThemes();
    for (const QVariant &entryValue : customThemes) {
        const QVariantMap entry = entryValue.toMap();
        if (normalizedThemeIdKey(entry.value(QStringLiteral("id")).toString()) != requestedId) {
            continue;
        }
        if (!excludedPath.isEmpty()
                && normalizeThemeFilePath(entry.value(QStringLiteral("filePath")).toString()) == excludedPath) {
            continue;
        }
        return false;
    }

    return true;
}

QString ThemeController::customThemeDirectory() const
{
    return resolvedCustomThemeDirectory();
}

QVariantList ThemeController::availableCustomThemes() const
{
    const QString directoryPath = resolvedCustomThemeDirectory();
    if (directoryPath.isEmpty()) {
        return {};
    }

    const QDir dir(directoryPath);
    const QFileInfoList entries = dir.entryInfoList(
        QStringList() << QStringLiteral("*.json"),
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);

    QVariantList themes;
    for (const QFileInfo &entry : entries) {
        const QVariantMap state = readThemeStateFromFile(entry.absoluteFilePath());
        if (state.isEmpty()) {
            continue;
        }

        QVariantMap item;
        item[QStringLiteral("id")] = state.value(QStringLiteral("id"));
        item[QStringLiteral("name")] = state.value(QStringLiteral("name"));
        item[QStringLiteral("mode")] = state.value(QStringLiteral("mode"));
        item[QStringLiteral("filePath")] = entry.absoluteFilePath();
        item[QStringLiteral("fileName")] = entry.fileName();
        item[QStringLiteral("colors")] = state.value(QStringLiteral("colors"));
        themes.append(item);
    }

    return themes;
}

QVariantMap ThemeController::exportState() const
{
    QVariantMap state = themeStateFromPalette(activePalette());
    state[QStringLiteral("customThemeLoaded")] = m_hasCustomPalette;
    state[QStringLiteral("themeFilePath")] = m_customThemePath;
    state[QStringLiteral("schemeId")] = activePalette().id;
    state[QStringLiteral("schemeName")] = activePalette().name;
    state[QStringLiteral("mode")] = m_mode == Light
        ? QStringLiteral("light")
        : (m_mode == Dark ? QStringLiteral("dark") : QStringLiteral("system"));
    return state;
}

bool ThemeController::importState(const QVariantMap &state)
{
    ThemePalette palette;
    if (!paletteFromState(state, &palette)) {
        return false;
    }

    const bool customThemeLoaded = state.value(QStringLiteral("customThemeLoaded")).toBool();
    applyPalette(palette, customThemeLoaded, true);
    m_customThemePath = customThemeLoaded ? state.value(QStringLiteral("themeFilePath")).toString() : QString();
    saveSettings();
    emit themeChanged();
    return true;
}

void ThemeController::updateSystemTheme()
{
    const QPalette palette = QGuiApplication::palette();
    m_systemIsDark = palette.color(QPalette::WindowText).lightness() > palette.color(QPalette::Window).lightness();
}

void ThemeController::loadSettings()
{
    QSettings settings;
    const int storedMode = settings.value(QStringLiteral("appearance/mode"), int(System)).toInt();
    const QString storedThemeFile = settings.value(QStringLiteral("appearance/themeFilePath")).toString();
    const QString storedSchemeId = settings.value(QStringLiteral("appearance/schemeId")).toString();

    const ThemeMode mode = static_cast<ThemeMode>(qBound(int(Light), storedMode, int(System)));
    m_mode = mode;

    bool loadedCustomTheme = false;
    if (!storedThemeFile.isEmpty()) {
        loadedCustomTheme = loadThemeFromFileInternal(storedThemeFile, false);
        if (loadedCustomTheme) {
            saveSettings();
        }
    }

    if (!loadedCustomTheme) {
        bool ok = false;
        ThemeScheme storedScheme = schemeFromId(storedSchemeId, &ok);
        if (!ok) {
            storedScheme = (mode == Dark) ? AuroraGlass
                        : (mode == Light) ? CatppuccinLatte
                        : defaultSchemeForSystem();
        }
        applyBuiltInScheme(storedScheme, false);
        saveSettings();
    }
}

void ThemeController::saveSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/mode"), int(m_mode));
    settings.setValue(QStringLiteral("appearance/schemeId"), activePalette().id);
    settings.setValue(QStringLiteral("appearance/themeFilePath"), m_hasCustomPalette ? m_customThemePath : QString());
}

void ThemeController::applyBuiltInScheme(ThemeScheme scheme, bool persist)
{
    const ThemePalette palette = paletteForScheme(scheme);
    m_hasCustomPalette = false;
    m_customThemePath.clear();
    m_scheme = scheme;
    m_mode = palette.dark ? Dark : Light;
    if (persist) {
        saveSettings();
    }
    emit schemeChanged();
    emit modeChanged();
    emit themeChanged();
}

void ThemeController::applyPalette(const ThemePalette &palette, bool customPalette, bool persist)
{
    if (customPalette) {
        m_hasCustomPalette = true;
        m_customPalette = palette;
    } else {
        m_hasCustomPalette = false;
        m_customPalette = ThemePalette();
    }

    if (!customPalette) {
        m_scheme = schemeFromId(palette.id);
        m_customThemePath.clear();
    }

    m_mode = palette.dark ? Dark : Light;

    if (persist) {
        saveSettings();
    }
    emit schemeChanged();
    emit modeChanged();
    emit themeChanged();
}

ThemeController::ThemePalette ThemeController::activePalette() const
{
    return m_hasCustomPalette ? m_customPalette : paletteForScheme(m_scheme);
}

ThemeController::ThemePalette ThemeController::paletteForScheme(ThemeScheme scheme) const
{
    switch (scheme) {
    case AuroraGlass:
        return makePalette(
            QStringLiteral("aurora-glass"),
            QStringLiteral("Aurora Glass"),
            true,
            QColor(QStringLiteral("#08111F")),
            QColor(QStringLiteral("#102033")),
            QColor(QStringLiteral("#17304A")),
            QColor(QStringLiteral("#1C3A59")),
            QColor(QStringLiteral("#E6F3FF")),
            QColor(QStringLiteral("#9DB4C8")),
            QColor(QStringLiteral("#29445E")),
            QColor(QStringLiteral("#2DD4BF")),
            QColor(QStringLiteral("#FFFFFF")),
            QColor(QStringLiteral("#F472B6")),
            QColor(QStringLiteral("#2DD4BF")),
            QColor(QStringLiteral("#06B6D4")),
            QColor(QStringLiteral("#2DD4BF")),
            QColor(QStringLiteral("#F59E0B")),
            QColor(QStringLiteral("#4ADE80")),
            QColor(QStringLiteral("#F59E0B")),
            QColor(QStringLiteral("#2DD4BF")),
            QColor(QStringLiteral("#8B5CF6")),
            QColor(QStringLiteral("#06B6D4")),
            QColor(QStringLiteral("#34D399")),
            QColor(QStringLiteral("#F97316")));

    case OxideGarden:
        return makePalette(
            QStringLiteral("oxide-garden"),
            QStringLiteral("Oxide Garden"),
            false,
            QColor(QStringLiteral("#F3EDDF")),
            QColor(QStringLiteral("#E8DDC4")),
            QColor(QStringLiteral("#DCCBA8")),
            QColor(QStringLiteral("#CDB68B")),
            QColor(QStringLiteral("#26251C")),
            QColor(QStringLiteral("#645F46")),
            QColor(QStringLiteral("#B8A878")),
            QColor(QStringLiteral("#A3442F")),
            QColor(QStringLiteral("#FFF8EA")),
            QColor(QStringLiteral("#B3263A")),
            QColor(QStringLiteral("#2F6F5E")),
            QColor(QStringLiteral("#7A4F22")),
            QColor(QStringLiteral("#58733D")),
            QColor(QStringLiteral("#C4742B")),
            QColor(QStringLiteral("#3F7F4A")),
            QColor(QStringLiteral("#B58522")),
            QColor(QStringLiteral("#2F6F5E")),
            QColor(QStringLiteral("#7A4F22")),
            QColor(QStringLiteral("#A3442F")),
            QColor(QStringLiteral("#58733D")),
            QColor(QStringLiteral("#6E4B2A")));

    case EmberLuxe:
        return makePalette(
            QStringLiteral("ember-luxe"),
            QStringLiteral("Ember Luxe"),
            true,
            QColor(QStringLiteral("#100C0A")),
            QColor(QStringLiteral("#1B1410")),
            QColor(QStringLiteral("#2A1E17")),
            QColor(QStringLiteral("#35261C")),
            QColor(QStringLiteral("#FFF7ED")),
            QColor(QStringLiteral("#C8B6A6")),
            QColor(QStringLiteral("#4A3426")),
            QColor(QStringLiteral("#F59E0B")),
            QColor(QStringLiteral("#FFF7ED")),
            QColor(QStringLiteral("#F43F5E")),
            QColor(QStringLiteral("#FBBF24")),
            QColor(QStringLiteral("#F59E0B")),
            QColor(QStringLiteral("#F59E0B")),
            QColor(QStringLiteral("#F97316")),
            QColor(QStringLiteral("#4ADE80")),
            QColor(QStringLiteral("#FBBF24")),
            QColor(QStringLiteral("#38BDF8")),
            QColor(QStringLiteral("#A78BFA")),
            QColor(QStringLiteral("#F59E0B")),
            QColor(QStringLiteral("#22C55E")),
            QColor(QStringLiteral("#DC2626")));

    case CatppuccinLatte:
    default:
        return makePalette(
            QStringLiteral("catppuccin-latte"),
            QStringLiteral("Catppuccin Latte"),
            false,
            QColor(QStringLiteral("#EFF1F5")),
            QColor(QStringLiteral("#E6E9EF")),
            QColor(QStringLiteral("#DCE0E8")),
            QColor(QStringLiteral("#CCD0DA")),
            QColor(QStringLiteral("#4C4F69")),
            QColor(QStringLiteral("#6C6F85")),
            QColor(QStringLiteral("#BCC0CC")),
            QColor(QStringLiteral("#1E66F5")),
            QColor(QStringLiteral("#EFF1F5")),
            QColor(QStringLiteral("#D20F39")),
            QColor(QStringLiteral("#1E66F5")),
            QColor(QStringLiteral("#7287FD")),
            QColor(QStringLiteral("#179299")),
            QColor(QStringLiteral("#FE640B")),
            QColor(QStringLiteral("#40A02B")),
            QColor(QStringLiteral("#DF8E1D")),
            QColor(QStringLiteral("#209FB5")),
            QColor(QStringLiteral("#8839EF")),
            QColor(QStringLiteral("#179299")),
            QColor(QStringLiteral("#40A02B")),
            QColor(QStringLiteral("#FE640B")));
    }
}

ThemeController::ThemeScheme ThemeController::defaultSchemeForSystem() const
{
    return m_systemIsDark ? AuroraGlass : CatppuccinLatte;
}

QJsonObject ThemeController::themeJsonObject(const ThemePalette &palette)
{
    QJsonObject colors;
    colors.insert(QStringLiteral("bg"), colorToString(palette.bg));
    colors.insert(QStringLiteral("surface"), colorToString(palette.surface));
    colors.insert(QStringLiteral("surfaceHover"), colorToString(palette.surfaceHover));
    colors.insert(QStringLiteral("surfaceActive"), colorToString(palette.surfaceActive));
    colors.insert(QStringLiteral("textPrimary"), colorToString(palette.textPrimary));
    colors.insert(QStringLiteral("textSecondary"), colorToString(palette.textSecondary));
    colors.insert(QStringLiteral("border"), colorToString(palette.border));
    colors.insert(QStringLiteral("accent"), colorToString(palette.accent));
    colors.insert(QStringLiteral("accentText"), colorToString(palette.accentText));
    colors.insert(QStringLiteral("danger"), colorToString(palette.danger));
    colors.insert(QStringLiteral("activeAccent"), colorToString(palette.activeAccent));
    colors.insert(QStringLiteral("activeGlow"), colorToString(palette.activeGlow));
    colors.insert(QStringLiteral("secondaryAccent"), colorToString(palette.secondaryAccent));
    colors.insert(QStringLiteral("warmAccent"), colorToString(palette.warmAccent));
    colors.insert(QStringLiteral("success"), colorToString(palette.success));
    colors.insert(QStringLiteral("warning"), colorToString(palette.warning));
    colors.insert(QStringLiteral("categoryInfo"), colorToString(palette.categoryInfo));
    colors.insert(QStringLiteral("categoryNavigation"), colorToString(palette.categoryNavigation));
    colors.insert(QStringLiteral("categoryAction"), colorToString(palette.categoryAction));
    colors.insert(QStringLiteral("categoryUtility"), colorToString(palette.categoryUtility));
    colors.insert(QStringLiteral("categorySystem"), colorToString(palette.categorySystem));
    colors.insert(QStringLiteral("overlayScrim"), colorToString(palette.overlayScrim));
    colors.insert(QStringLiteral("focusRing"), colorToString(palette.focusRing));
    colors.insert(QStringLiteral("panelSurface"), colorToString(palette.panelSurface));
    colors.insert(QStringLiteral("panelSurfaceSoft"), colorToString(palette.panelSurfaceSoft));
    colors.insert(QStringLiteral("panelSurfaceStrong"), colorToString(palette.panelSurfaceStrong));
    colors.insert(QStringLiteral("panelBorder"), colorToString(palette.panelBorder));
    colors.insert(QStringLiteral("controlSurface"), colorToString(palette.controlSurface));
    colors.insert(QStringLiteral("controlSurfaceActive"), colorToString(palette.controlSurfaceActive));
    colors.insert(QStringLiteral("controlBorder"), colorToString(palette.controlBorder));
    colors.insert(QStringLiteral("itemHoverFill"), colorToString(palette.itemHoverFill));
    colors.insert(QStringLiteral("itemCurrentFill"), colorToString(palette.itemCurrentFill));
    colors.insert(QStringLiteral("itemCurrentBorder"), colorToString(palette.itemCurrentBorder));
    colors.insert(QStringLiteral("itemSelectedFill"), colorToString(palette.itemSelectedFill));
    colors.insert(QStringLiteral("itemSelectedFillInactive"), colorToString(palette.itemSelectedFillInactive));
    colors.insert(QStringLiteral("itemSelectedBorder"), colorToString(palette.itemSelectedBorder));
    colors.insert(QStringLiteral("itemSelectedBorderInactive"), colorToString(palette.itemSelectedBorderInactive));
    colors.insert(QStringLiteral("statusRailFill"), colorToString(palette.statusRailFill));

    QJsonObject root;
    root.insert(QStringLiteral("id"), palette.id);
    root.insert(QStringLiteral("name"), palette.name);
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("mode"), palette.dark ? QStringLiteral("dark") : QStringLiteral("light"));
    root.insert(QStringLiteral("colors"), colors);
    return root;
}

QVariantMap ThemeController::themeStateFromPalette(const ThemePalette &palette)
{
    return themeJsonObject(palette).toVariantMap();
}

QString ThemeController::colorToString(const QColor &color)
{
    return color.name(QColor::HexArgb);
}

QColor ThemeController::colorFromString(const QString &value, const QColor &fallback)
{
    const QColor color(value);
    return color.isValid() ? color : fallback;
}

ThemeController::ThemeScheme ThemeController::schemeFromId(const QString &id, bool *ok)
{
    const QString key = id.trimmed().toLower();
    if (key == QStringLiteral("catppuccin-latte")
            || key == QStringLiteral("neon-carbon")) {
        if (ok) *ok = true;
        return CatppuccinLatte;
    }
    if (key == QStringLiteral("aurora-glass")) {
        if (ok) *ok = true;
        return AuroraGlass;
    }
    if (key == QStringLiteral("oxide-garden")
            || key == QStringLiteral("porcelain-spectrum")) {
        if (ok) *ok = true;
        return OxideGarden;
    }
    if (key == QStringLiteral("ember-luxe")) {
        if (ok) *ok = true;
        return EmberLuxe;
    }
    if (ok) *ok = false;
    return CatppuccinLatte;
}

bool ThemeController::paletteFromState(const QVariantMap &state, ThemePalette *palette) const
{
    if (!palette) {
        return false;
    }

    const QVariantMap colors = state.value(QStringLiteral("colors")).toMap();
    if (colors.isEmpty()) {
        return false;
    }

    const QString id = state.value(QStringLiteral("id"), QStringLiteral("custom-theme")).toString();
    const QString name = state.value(QStringLiteral("name"), id).toString();
    const QString mode = state.value(QStringLiteral("mode"), QStringLiteral("dark")).toString();
    const bool dark = QString::compare(mode, QStringLiteral("light"), Qt::CaseInsensitive) != 0;

    ThemePalette resolved = makePalette(
        id,
        name,
        dark,
        colorFromString(colors.value(QStringLiteral("bg")).toString(), QColor(QStringLiteral("#070A0F"))),
        colorFromString(colors.value(QStringLiteral("surface")).toString(), QColor(QStringLiteral("#101722"))),
        colorFromString(colors.value(QStringLiteral("surfaceHover")).toString(), QColor(QStringLiteral("#172235"))),
        colorFromString(colors.value(QStringLiteral("surfaceActive")).toString(), QColor(QStringLiteral("#1D2A40"))),
        colorFromString(colors.value(QStringLiteral("textPrimary")).toString(), QColor(QStringLiteral("#EAF2FF"))),
        colorFromString(colors.value(QStringLiteral("textSecondary")).toString(), QColor(QStringLiteral("#94A3B8"))),
        colorFromString(colors.value(QStringLiteral("border")).toString(), QColor(QStringLiteral("#263247"))),
        colorFromString(colors.value(QStringLiteral("accent")).toString(), QColor(QStringLiteral("#22D3EE"))),
        colorFromString(colors.value(QStringLiteral("accentText")).toString(), QColor(QStringLiteral("#FFFFFF"))),
        colorFromString(colors.value(QStringLiteral("danger")).toString(), QColor(QStringLiteral("#FB7185"))),
        colorFromString(colors.value(QStringLiteral("activeAccent")).toString(), QColor(QStringLiteral("#A855F7"))),
        colorFromString(colors.value(QStringLiteral("activeGlow")).toString(), QColor(QStringLiteral("#A855F7"))),
        colorFromString(colors.value(QStringLiteral("secondaryAccent")).toString(), QColor(QStringLiteral("#2DD4BF"))),
        colorFromString(colors.value(QStringLiteral("warmAccent")).toString(), QColor(QStringLiteral("#FBBF24"))),
        colorFromString(colors.value(QStringLiteral("success")).toString(), QColor(QStringLiteral("#22C55E"))),
        colorFromString(colors.value(QStringLiteral("warning")).toString(), QColor(QStringLiteral("#F59E0B"))),
        colorFromString(colors.value(QStringLiteral("categoryInfo")).toString(), QColor(QStringLiteral("#38BDF8"))),
        colorFromString(colors.value(QStringLiteral("categoryNavigation")).toString(), QColor(QStringLiteral("#8B5CF6"))),
        colorFromString(colors.value(QStringLiteral("categoryAction")).toString(), QColor(QStringLiteral("#2DD4BF"))),
        colorFromString(colors.value(QStringLiteral("categoryUtility")).toString(), QColor(QStringLiteral("#22C55E"))),
        colorFromString(colors.value(QStringLiteral("categorySystem")).toString(), QColor(QStringLiteral("#F97316"))));

    auto alpha = [](const QColor &color, qreal value) {
        QColor c = color;
        c.setAlphaF(value);
        return c;
    };
    resolved.overlayScrim = colorFromString(colors.value(QStringLiteral("overlayScrim")).toString(), alpha(resolved.bg, resolved.dark ? 0.52 : 0.30));
    resolved.focusRing = colorFromString(colors.value(QStringLiteral("focusRing")).toString(), alpha(resolved.accent, resolved.dark ? 0.82 : 0.88));
    resolved.panelSurface = colorFromString(colors.value(QStringLiteral("panelSurface")).toString(), resolved.surface);
    resolved.panelSurfaceSoft = colorFromString(colors.value(QStringLiteral("panelSurfaceSoft")).toString(), resolved.dark ? alpha(resolved.surface, 0.56) : alpha(resolved.bg, 0.48));
    resolved.panelSurfaceStrong = colorFromString(colors.value(QStringLiteral("panelSurfaceStrong")).toString(), resolved.dark ? alpha(resolved.surface, 0.90) : alpha(resolved.bg, 0.84));
    resolved.panelBorder = colorFromString(colors.value(QStringLiteral("panelBorder")).toString(), resolved.dark ? alpha(Qt::white, 0.14) : alpha(resolved.border, 0.72));
    resolved.controlSurface = colorFromString(colors.value(QStringLiteral("controlSurface")).toString(), resolved.surfaceHover);
    resolved.controlSurfaceActive = colorFromString(colors.value(QStringLiteral("controlSurfaceActive")).toString(), resolved.surfaceActive);
    resolved.controlBorder = colorFromString(colors.value(QStringLiteral("controlBorder")).toString(), resolved.border);
    resolved.itemHoverFill = colorFromString(colors.value(QStringLiteral("itemHoverFill")).toString(), resolved.dark ? alpha(Qt::white, 0.10) : alpha(resolved.accent, 0.13));
    resolved.itemCurrentFill = colorFromString(colors.value(QStringLiteral("itemCurrentFill")).toString(), resolved.dark ? alpha(Qt::white, 0.08) : alpha(resolved.accent, 0.09));
    resolved.itemCurrentBorder = colorFromString(colors.value(QStringLiteral("itemCurrentBorder")).toString(), resolved.dark ? alpha(Qt::white, 0.25) : alpha(resolved.accent, 0.55));
    resolved.itemSelectedFill = colorFromString(colors.value(QStringLiteral("itemSelectedFill")).toString(), resolved.dark ? alpha(Qt::white, 0.18) : alpha(resolved.accent, 0.13));
    resolved.itemSelectedFillInactive = colorFromString(colors.value(QStringLiteral("itemSelectedFillInactive")).toString(), resolved.dark ? alpha(Qt::white, 0.12) : alpha(resolved.accent, 0.09));
    resolved.itemSelectedBorder = colorFromString(colors.value(QStringLiteral("itemSelectedBorder")).toString(), resolved.dark ? alpha(Qt::white, 0.35) : alpha(resolved.accent, 0.85));
    resolved.itemSelectedBorderInactive = colorFromString(colors.value(QStringLiteral("itemSelectedBorderInactive")).toString(), resolved.dark ? alpha(Qt::white, 0.20) : alpha(resolved.accent, 0.55));
    resolved.statusRailFill = colorFromString(colors.value(QStringLiteral("statusRailFill")).toString(), resolved.dark ? alpha(resolved.surface, 0.98) : alpha(resolved.bg, 0.995));

    *palette = resolved;
    return true;
}

QString ThemeController::resolvedCustomThemeDirectory() const
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    }
    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    if (basePath.isEmpty()) {
        return {};
    }

    QDir dir(basePath);
    if (!dir.mkpath(QStringLiteral("themes"))) {
        return {};
    }
    return dir.filePath(QStringLiteral("themes"));
}

bool ThemeController::loadThemeFromFileInternal(const QString &filePath, bool persist)
{
    const QString path = normalizeThemeFilePath(filePath);
    if (path.isEmpty()) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }

    ThemePalette palette;
    if (!paletteFromState(doc.object().toVariantMap(), &palette)) {
        return false;
    }

    applyPalette(palette, true, persist);
    m_customThemePath = path;
    if (persist) {
        saveSettings();
    }
    emit themeChanged();
    return true;
}
