#include "ThemeController.h"

#include <QGuiApplication>
#include <QEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>
#include <QUrl>

#include <algorithm>
#include <cmath>

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

QColor withAlpha(const QColor &color, qreal value)
{
    QColor c = color;
    c.setAlphaF(value);
    return c;
}

qreal contrastChannel(qreal value)
{
    return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

qreal luminance(const QColor &color)
{
    return 0.2126 * contrastChannel(color.redF())
        + 0.7152 * contrastChannel(color.greenF())
        + 0.0722 * contrastChannel(color.blueF());
}

qreal contrastRatio(const QColor &first, const QColor &second)
{
    const qreal firstLuminance = luminance(first);
    const qreal secondLuminance = luminance(second);
    const qreal lighter = std::max(firstLuminance, secondLuminance);
    const qreal darker = std::min(firstLuminance, secondLuminance);
    return (lighter + 0.05) / (darker + 0.05);
}

QColor mixColor(const QColor &base, const QColor &overlay, qreal amount)
{
    const qreal clampedAmount = qBound<qreal>(0.0, amount, 1.0);
    const qreal baseAmount = 1.0 - clampedAmount;
    return QColor::fromRgbF(
        base.redF() * baseAmount + overlay.redF() * clampedAmount,
        base.greenF() * baseAmount + overlay.greenF() * clampedAmount,
        base.blueF() * baseAmount + overlay.blueF() * clampedAmount,
        base.alphaF() * baseAmount + overlay.alphaF() * clampedAmount);
}

QColor readableColor(const QColor &background, const QColor &preferred, const QList<QColor> &fallbacks, qreal minimumRatio)
{
    QColor best = preferred.isValid() ? preferred : fallbacks.value(0, QColor(Qt::black));
    qreal bestRatio = contrastRatio(background, best);
    for (const QColor &candidate : fallbacks) {
        if (!candidate.isValid()) {
            continue;
        }
        const qreal ratio = contrastRatio(background, candidate);
        if (ratio > bestRatio) {
            best = candidate;
            bestRatio = ratio;
        }
    }
    return bestRatio >= minimumRatio ? best : fallbacks.value(0, best);
}

QColor visibleBorder(const QColor &background, const QColor &preferred, const QColor &fallback, bool dark)
{
    QColor border = preferred;
    if (!border.isValid() || border.alpha() == 0 || contrastRatio(background, border) < 1.25) {
        border = fallback;
    }
    if (contrastRatio(background, border) < 1.25) {
        border = dark ? background.lighter(160) : background.darker(135);
    }
    return border;
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
    palette.menuBorder = dark
        ? border.lighter(125)
        : border.darker(108);
    palette.menuSeparator = dark
        ? border.lighter(175)
        : border.darker(165);
    palette.menuItemPressed = surfaceHover.darker(118);
    palette.chromeGradientStart = dark
        ? alpha(secondaryAccent, 0.24)
        : alpha(secondaryAccent, 0.18);
    palette.chromeGradientMid = dark
        ? alpha(accent, 0.19)
        : alpha(accent, 0.13);
    palette.chromeGradientEnd = dark
        ? alpha(warmAccent, 0.13)
        : alpha(warmAccent, 0.08);
    palette.glassShadow = dark
        ? alpha(QColor(Qt::black), 0.36)
        : alpha(QColor(Qt::black), 0.16);
    palette.shadow = QColor(QStringLiteral("#10000000"));
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
    qGuiApp->installEventFilter(this);
    connect(qGuiApp->styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this]() { handleSystemThemeChanged(); });
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
        m_hasCustomPalette = false;
        m_customThemePath.clear();
        saveSettings();
        emit themeChanged();
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
QColor ThemeController::menuBorder() const { return activePalette().menuBorder; }
QColor ThemeController::menuSeparator() const { return activePalette().menuSeparator; }
QColor ThemeController::menuItemPressed() const { return activePalette().menuItemPressed; }
QColor ThemeController::chromeGradientStart() const { return activePalette().chromeGradientStart; }
QColor ThemeController::chromeGradientMid() const { return activePalette().chromeGradientMid; }
QColor ThemeController::chromeGradientEnd() const { return activePalette().chromeGradientEnd; }
QColor ThemeController::glassShadow() const { return activePalette().glassShadow; }
QColor ThemeController::shadow() const { return activePalette().shadow; }

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
    return themeStateFromPalette(defaultDraftPaletteForMode(true));
}

QVariantMap ThemeController::defaultThemeDraftForMode(const QString &mode) const
{
    const bool dark = QString::compare(mode, QStringLiteral("light"), Qt::CaseInsensitive) != 0;
    return themeStateFromPalette(defaultDraftPaletteForMode(dark));
}

QVariantList ThemeController::builtInThemeDrafts() const
{
    auto draftWithSubtitle = [](const ThemePalette &palette, const QString &subtitle) {
        QVariantMap draft = ThemeController::themeStateFromPalette(palette);
        draft[QStringLiteral("subtitle")] = subtitle;
        return draft;
    };

    QVariantList drafts;
    drafts.reserve(6);
    drafts.append(draftWithSubtitle(paletteForScheme(CatppuccinLatte), QStringLiteral("Soft light, blue, mauve")));
    drafts.append(draftWithSubtitle(paletteForScheme(AuroraGlass), QStringLiteral("Teal, orchid, blue")));
    drafts.append(draftWithSubtitle(paletteForScheme(PorcelainBloom), QStringLiteral("Porcelain, rose, leaf")));
    drafts.append(draftWithSubtitle(paletteForScheme(EmberLuxe), QStringLiteral("Amber, ruby, espresso")));
    drafts.append(draftWithSubtitle(paletteForScheme(GraphiteSage), QStringLiteral("Graphite, sage, brass")));
    drafts.append(draftWithSubtitle(paletteForScheme(VelvetExcess), QStringLiteral("Imperial white, gold, violet")));
    return drafts;
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
    const QString modeStr = state.value(QStringLiteral("mode")).toString();
    if (modeStr == QStringLiteral("system")) {
        m_mode = System;
        m_hasCustomPalette = false;
        m_customThemePath.clear();
        saveSettings();
        emit modeChanged();
        emit themeChanged();
        return true;
    }

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
    const Qt::ColorScheme colorScheme = QGuiApplication::styleHints()->colorScheme();
    if (colorScheme == Qt::ColorScheme::Dark) {
        m_systemIsDark = true;
        return;
    }
    if (colorScheme == Qt::ColorScheme::Light) {
        m_systemIsDark = false;
        return;
    }

    const QPalette palette = QGuiApplication::palette();
    m_systemIsDark = palette.color(QPalette::WindowText).lightness() > palette.color(QPalette::Window).lightness();
}

void ThemeController::handleSystemThemeChanged()
{
    const bool wasDark = m_systemIsDark;
    updateSystemTheme();
    if (m_mode != System) {
        return;
    }
    if (wasDark != m_systemIsDark) {
        emit modeChanged();
    }
    emit themeChanged();
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
        if (m_mode == System) {
            m_hasCustomPalette = false;
            m_customThemePath.clear();
            saveSettings();
            emit themeChanged();
        } else {
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
    if (m_mode == System && !m_hasCustomPalette) {
        return paletteFromSystem();
    }
    return m_hasCustomPalette ? m_customPalette : paletteForScheme(m_scheme);
}

ThemeController::ThemePalette ThemeController::paletteForScheme(ThemeScheme scheme) const
{
    switch (scheme) {
    case AuroraGlass: {
        ThemePalette palette = makePalette(
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
            QColor(QStringLiteral("#38BDF8")),
            QColor(QStringLiteral("#A78BFA")),
            QColor(QStringLiteral("#06B6D4")),
            QColor(QStringLiteral("#34D399")),
            QColor(QStringLiteral("#FB923C")));
        palette.chromeGradientStart = withAlpha(palette.categoryInfo, 0.24);
        palette.chromeGradientMid = withAlpha(palette.accent, 0.19);
        palette.chromeGradientEnd = withAlpha(palette.categoryNavigation, 0.13);
        return palette;
    }

    case PorcelainBloom: {
        ThemePalette palette = makePalette(
            QStringLiteral("porcelain-bloom"),
            QStringLiteral("Porcelain Bloom"),
            false,
            QColor(QStringLiteral("#FAFBFA")),
            QColor(QStringLiteral("#FFFFFF")),
            QColor(QStringLiteral("#FFF1F1")),
            QColor(QStringLiteral("#FFE3E1")),
            QColor(QStringLiteral("#2E2527")),
            QColor(QStringLiteral("#755F64")),
            QColor(QStringLiteral("#E7CACA")),
            QColor(QStringLiteral("#D61F3D")),
            QColor(QStringLiteral("#FFFFFF")),
            QColor(QStringLiteral("#B91C1C")),
            QColor(QStringLiteral("#E11D48")),
            QColor(QStringLiteral("#FB7185")),
            QColor(QStringLiteral("#0F9F8F")),
            QColor(QStringLiteral("#E75B35")),
            QColor(QStringLiteral("#2F8F5B")),
            QColor(QStringLiteral("#B7791F")),
            QColor(QStringLiteral("#0E8AA5")),
            QColor(QStringLiteral("#D92652")),
            QColor(QStringLiteral("#D61F3D")),
            QColor(QStringLiteral("#7C3AED")),
            QColor(QStringLiteral("#2F8F5B")));
        palette.panelSurface = QColor(QStringLiteral("#FFFFFF"));
        palette.panelSurfaceSoft = QColor(QStringLiteral("#FFF7F6"));
        palette.panelSurfaceStrong = QColor(QStringLiteral("#FFFFFF"));
        palette.panelBorder = withAlpha(QColor(QStringLiteral("#E2B7B7")), 0.88);
        palette.controlSurface = QColor(QStringLiteral("#FFF3F2"));
        palette.controlSurfaceActive = QColor(QStringLiteral("#FFE2E0"));
        palette.controlBorder = QColor(QStringLiteral("#E5B9B8"));
        palette.itemHoverFill = withAlpha(palette.accent, 0.09);
        palette.itemCurrentFill = withAlpha(palette.activeAccent, 0.10);
        palette.itemCurrentBorder = withAlpha(palette.activeAccent, 0.66);
        palette.itemSelectedFill = withAlpha(palette.activeAccent, 0.13);
        palette.itemSelectedFillInactive = withAlpha(palette.activeAccent, 0.08);
        palette.itemSelectedBorder = withAlpha(palette.activeAccent, 0.84);
        palette.itemSelectedBorderInactive = withAlpha(palette.activeAccent, 0.48);
        palette.statusRailFill = QColor(QStringLiteral("#FFF9F8"));
        palette.menuBorder = QColor(QStringLiteral("#DDAEAE"));
        palette.menuSeparator = QColor(QStringLiteral("#CA8D8F"));
        palette.menuItemPressed = QColor(QStringLiteral("#FFD8D5"));
        palette.chromeGradientStart = withAlpha(QColor(QStringLiteral("#F6A8B7")), 0.40);
        palette.chromeGradientMid = withAlpha(palette.activeGlow, 0.28);
        palette.chromeGradientEnd = withAlpha(QColor(QStringLiteral("#FFFFFF")), 0.08);
        return palette;
    }

    case EmberLuxe: {
        ThemePalette palette = makePalette(
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
            QColor(QStringLiteral("#FB923C")));
        palette.chromeGradientStart = withAlpha(palette.danger, 0.19);
        palette.chromeGradientMid = withAlpha(palette.accent, 0.18);
        palette.chromeGradientEnd = withAlpha(palette.warmAccent, 0.13);
        return palette;
    }

    case GraphiteSage: {
        ThemePalette palette = makePalette(
            QStringLiteral("graphite-sage"),
            QStringLiteral("Graphite Sage"),
            true,
            QColor(QStringLiteral("#111715")),
            QColor(QStringLiteral("#1B2421")),
            QColor(QStringLiteral("#24302B")),
            QColor(QStringLiteral("#2E3B35")),
            QColor(QStringLiteral("#EEF5F0")),
            QColor(QStringLiteral("#A7B6AD")),
            QColor(QStringLiteral("#34443D")),
            QColor(QStringLiteral("#74C69D")),
            QColor(QStringLiteral("#06110C")),
            QColor(QStringLiteral("#E86F7E")),
            QColor(QStringLiteral("#8FB7FF")),
            QColor(QStringLiteral("#6E8FEF")),
            QColor(QStringLiteral("#8FCAB8")),
            QColor(QStringLiteral("#D6A85C")),
            QColor(QStringLiteral("#7DCC9A")),
            QColor(QStringLiteral("#D6A85C")),
            QColor(QStringLiteral("#8FB7FF")),
            QColor(QStringLiteral("#B7A6D8")),
            QColor(QStringLiteral("#9BCFAD")),
            QColor(QStringLiteral("#8FCAB8")),
            QColor(QStringLiteral("#8AA0B8")));
        palette.chromeGradientStart = withAlpha(palette.categoryNavigation, 0.22);
        palette.chromeGradientMid = withAlpha(palette.accent, 0.18);
        palette.chromeGradientEnd = withAlpha(palette.warmAccent, 0.13);
        return palette;
    }

    case VelvetExcess: {
        auto alpha = [](const QColor &color, qreal value) {
            QColor c = color;
            c.setAlphaF(value);
            return c;
        };

        ThemePalette palette = makePalette(
            QStringLiteral("velvet-excess"),
            QStringLiteral("Calus Opulence"),
            false,
            QColor(QStringLiteral("#FFFEFA")),
            QColor(QStringLiteral("#FFF8E7")),
            QColor(QStringLiteral("#F7E9CA")),
            QColor(QStringLiteral("#EED9A8")),
            QColor(QStringLiteral("#2E2140")),
            QColor(QStringLiteral("#6E587E")),
            QColor(QStringLiteral("#C9961B")),
            QColor(QStringLiteral("#D19A13")),
            QColor(QStringLiteral("#1D1230")),
            QColor(QStringLiteral("#B7355F")),
            QColor(QStringLiteral("#7A4FF1")),
            QColor(QStringLiteral("#BDA2FF")),
            QColor(QStringLiteral("#6F50C8")),
            QColor(QStringLiteral("#E3A91E")),
            QColor(QStringLiteral("#17896D")),
            QColor(QStringLiteral("#B77600")),
            QColor(QStringLiteral("#8B5CF6")),
            QColor(QStringLiteral("#D19A13")),
            QColor(QStringLiteral("#6F50C8")),
            QColor(QStringLiteral("#17896D")),
            QColor(QStringLiteral("#9A6C18")));

        palette.panelSurface = QColor(QStringLiteral("#FFFFFF"));
        palette.panelSurfaceSoft = QColor(QStringLiteral("#FFF9EC"));
        palette.panelSurfaceStrong = QColor(QStringLiteral("#FFFFFF"));
        palette.panelBorder = alpha(QColor(QStringLiteral("#C99114")), 0.88);
        palette.controlSurface = QColor(QStringLiteral("#FFF0C9"));
        palette.controlSurfaceActive = QColor(QStringLiteral("#EEC76C"));
        palette.controlBorder = QColor(QStringLiteral("#C48A0E"));
        palette.itemHoverFill = alpha(QColor(QStringLiteral("#8B5CF6")), 0.10);
        palette.itemCurrentFill = alpha(QColor(QStringLiteral("#D19A13")), 0.18);
        palette.itemCurrentBorder = alpha(QColor(QStringLiteral("#D19A13")), 0.84);
        palette.itemSelectedFill = alpha(QColor(QStringLiteral("#7A4FF1")), 0.13);
        palette.itemSelectedFillInactive = alpha(QColor(QStringLiteral("#7A4FF1")), 0.08);
        palette.itemSelectedBorder = alpha(QColor(QStringLiteral("#7A4FF1")), 0.72);
        palette.itemSelectedBorderInactive = alpha(QColor(QStringLiteral("#7A4FF1")), 0.44);
        palette.statusRailFill = QColor(QStringLiteral("#FFFBF1"));
        palette.menuBorder = QColor(QStringLiteral("#C58B10"));
        palette.menuSeparator = QColor(QStringLiteral("#A96F00"));
        palette.menuItemPressed = QColor(QStringLiteral("#EBC46E"));
        palette.chromeGradientStart = alpha(QColor(QStringLiteral("#FFFFFF")), 0.35);
        palette.chromeGradientMid = alpha(palette.accent, 0.23);
        palette.chromeGradientEnd = alpha(palette.secondaryAccent, 0.18);
        palette.glassShadow = alpha(QColor(QStringLiteral("#5C4514")), 0.18);
        return palette;
    }

    case CatppuccinLatte:
    default: {
        ThemePalette palette = makePalette(
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
            QColor(QStringLiteral("#D75600")),
            QColor(QStringLiteral("#2F7D22")),
            QColor(QStringLiteral("#B86E00")),
            QColor(QStringLiteral("#0A7EA4")),
            QColor(QStringLiteral("#8839EF")),
            QColor(QStringLiteral("#117E83")),
            QColor(QStringLiteral("#2F7D22")),
            QColor(QStringLiteral("#D75600")));
        palette.chromeGradientStart = withAlpha(palette.secondaryAccent, 0.13);
        palette.chromeGradientMid = withAlpha(palette.accent, 0.09);
        palette.chromeGradientEnd = withAlpha(palette.warmAccent, 0.06);
        return palette;
    }
    }
}

ThemeController::ThemeScheme ThemeController::defaultSchemeForSystem() const
{
    return m_systemIsDark ? AuroraGlass : CatppuccinLatte;
}

ThemeController::ThemePalette ThemeController::defaultDraftPaletteForMode(bool dark) const
{
    if (dark) {
        return makePalette(
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
    }

    return makePalette(
        QString(),
        QString(),
        false,
        QColor(QStringLiteral("#F3F5F8")),
        QColor(QStringLiteral("#FFFFFF")),
        QColor(QStringLiteral("#E8EDF3")),
        QColor(QStringLiteral("#D9E1EB")),
        QColor(QStringLiteral("#202938")),
        QColor(QStringLiteral("#5F6B7C")),
        QColor(QStringLiteral("#C4CEDA")),
        QColor(QStringLiteral("#3A78C2")),
        QColor(QStringLiteral("#FFFFFF")),
        QColor(QStringLiteral("#D15B6C")),
        QColor(QStringLiteral("#4C84CC")),
        QColor(QStringLiteral("#5D90D4")),
        QColor(QStringLiteral("#6FA38E")),
        QColor(QStringLiteral("#D49C56")),
        QColor(QStringLiteral("#4E9A69")),
        QColor(QStringLiteral("#C88A30")),
        QColor(QStringLiteral("#4F93C8")),
        QColor(QStringLiteral("#8C79C9")),
        QColor(QStringLiteral("#3A78C2")),
        QColor(QStringLiteral("#6FA38E")),
        QColor(QStringLiteral("#C77A55")));
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
    colors.insert(QStringLiteral("menuBorder"), colorToString(palette.menuBorder));
    colors.insert(QStringLiteral("menuSeparator"), colorToString(palette.menuSeparator));
    colors.insert(QStringLiteral("menuItemPressed"), colorToString(palette.menuItemPressed));
    colors.insert(QStringLiteral("chromeGradientStart"), colorToString(palette.chromeGradientStart));
    colors.insert(QStringLiteral("chromeGradientMid"), colorToString(palette.chromeGradientMid));
    colors.insert(QStringLiteral("chromeGradientEnd"), colorToString(palette.chromeGradientEnd));
    colors.insert(QStringLiteral("glassShadow"), colorToString(palette.glassShadow));
    colors.insert(QStringLiteral("shadow"), colorToString(palette.shadow));

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
    if (key == QStringLiteral("porcelain-bloom")) {
        if (ok) *ok = true;
        return PorcelainBloom;
    }
    if (key == QStringLiteral("ember-luxe")) {
        if (ok) *ok = true;
        return EmberLuxe;
    }
    if (key == QStringLiteral("graphite-sage")) {
        if (ok) *ok = true;
        return GraphiteSage;
    }
    if (key == QStringLiteral("velvet-excess")) {
        if (ok) *ok = true;
        return VelvetExcess;
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
    resolved.menuBorder = colorFromString(colors.value(QStringLiteral("menuBorder")).toString(), resolved.dark ? resolved.border.lighter(125) : resolved.border.darker(108));
    resolved.menuSeparator = colorFromString(colors.value(QStringLiteral("menuSeparator")).toString(), resolved.dark ? resolved.border.lighter(175) : resolved.border.darker(165));
    resolved.menuItemPressed = colorFromString(colors.value(QStringLiteral("menuItemPressed")).toString(), resolved.surfaceHover.darker(118));
    resolved.chromeGradientStart = colorFromString(colors.value(QStringLiteral("chromeGradientStart")).toString(), resolved.chromeGradientStart);
    resolved.chromeGradientMid = colorFromString(colors.value(QStringLiteral("chromeGradientMid")).toString(), resolved.chromeGradientMid);
    resolved.chromeGradientEnd = colorFromString(colors.value(QStringLiteral("chromeGradientEnd")).toString(), resolved.chromeGradientEnd);
    resolved.glassShadow = colorFromString(colors.value(QStringLiteral("glassShadow")).toString(), resolved.dark ? alpha(QColor(Qt::black), 0.36) : alpha(QColor(Qt::black), 0.16));
    resolved.shadow = colorFromString(colors.value(QStringLiteral("shadow")).toString(), QColor(QStringLiteral("#10000000")));

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

ThemeController::ThemePalette ThemeController::paletteFromSystem() const
{
    const QPalette sysPal = QGuiApplication::palette();
    const Qt::ColorScheme colorScheme = QGuiApplication::styleHints()->colorScheme();
    const bool dark = colorScheme == Qt::ColorScheme::Dark
        ? true
        : (colorScheme == Qt::ColorScheme::Light
               ? false
               : sysPal.color(QPalette::WindowText).lightness() > sysPal.color(QPalette::Window).lightness());

    ThemePalette baseDraft = defaultDraftPaletteForMode(dark);

    QColor bg = sysPal.color(QPalette::Window).isValid() ? sysPal.color(QPalette::Window) : baseDraft.bg;
    QColor surface = sysPal.color(QPalette::Base).isValid() ? sysPal.color(QPalette::Base) : baseDraft.surface;
    if (contrastRatio(bg, surface) < 1.08) {
        surface = dark ? bg.lighter(118) : bg.darker(104);
    }
    QColor surfaceHover = dark ? surface.lighter(115) : surface.darker(108);
    QColor surfaceActive = dark ? surface.lighter(130) : surface.darker(118);

    QColor textPrimary = readableColor(
        surface,
        sysPal.color(QPalette::Text),
        {baseDraft.textPrimary, dark ? QColor(Qt::white) : QColor(Qt::black)},
        4.5);

    QColor textSecondary = sysPal.color(QPalette::PlaceholderText);
    if (!textSecondary.isValid() || textSecondary == textPrimary || contrastRatio(surface, textSecondary) < 3.0) {
        textSecondary = mixColor(textPrimary, surface, dark ? 0.38 : 0.42);
    }
    if (contrastRatio(surface, textSecondary) < 3.0) {
        textSecondary = baseDraft.textSecondary;
    }

    QColor border = visibleBorder(surface, sysPal.color(QPalette::Mid), baseDraft.border, dark);

    QColor accent = sysPal.color(QPalette::Highlight);
    if (!accent.isValid() || contrastRatio(surface, accent) < 1.35) {
        accent = baseDraft.accent;
    }
    QColor accentText = readableColor(
        accent,
        sysPal.color(QPalette::HighlightedText),
        {baseDraft.accentText, dark ? QColor(Qt::black) : QColor(Qt::white), textPrimary},
        4.5);

    const qreal categoryAccentMix = dark ? 0.22 : 0.18;
    const QColor categoryInfo = mixColor(baseDraft.categoryInfo, accent, categoryAccentMix);
    const QColor categoryNavigation = mixColor(baseDraft.categoryNavigation, accent, categoryAccentMix);
    const QColor categoryAction = mixColor(baseDraft.categoryAction, accent, categoryAccentMix);
    const QColor categoryUtility = mixColor(baseDraft.categoryUtility, accent, categoryAccentMix);
    const QColor categorySystem = mixColor(baseDraft.categorySystem, accent, categoryAccentMix);
    const QColor secondaryAccent = mixColor(baseDraft.secondaryAccent, accent, dark ? 0.18 : 0.15);
    const QColor warmAccent = mixColor(baseDraft.warmAccent, accent, dark ? 0.16 : 0.14);

    ThemePalette pal = makePalette(
        QStringLiteral("system"),
        QStringLiteral("System"),
        dark,
        bg,
        surface,
        surfaceHover,
        surfaceActive,
        textPrimary,
        textSecondary,
        border,
        accent,
        accentText,
        baseDraft.danger,
        accent,
        accent,
        secondaryAccent,
        warmAccent,
        baseDraft.success,
        baseDraft.warning,
        categoryInfo,
        categoryNavigation,
        categoryAction,
        categoryUtility,
        categorySystem
    );
    
    return pal;
}

QVariantMap ThemeController::systemThemeColors() const
{
    return themeStateFromPalette(paletteFromSystem());
}

bool ThemeController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qGuiApp
        && (event->type() == QEvent::ApplicationPaletteChange
            || event->type() == QEvent::ThemeChange)) {
        handleSystemThemeChanged();
    }
    return QObject::eventFilter(watched, event);
}
