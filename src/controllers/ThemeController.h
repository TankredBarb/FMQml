#pragma once

#include <QObject>
#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class ThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(ThemeMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(ThemeScheme scheme READ scheme WRITE setScheme NOTIFY schemeChanged)
    Q_PROPERTY(QString schemeName READ schemeName NOTIFY themeChanged)
    Q_PROPERTY(bool customThemeLoaded READ customThemeLoaded NOTIFY themeChanged)
    Q_PROPERTY(QString themeFilePath READ themeFilePath NOTIFY themeChanged)
    Q_PROPERTY(bool isDark READ isDark NOTIFY themeChanged)
    Q_PROPERTY(QVariantMap systemThemeColors READ systemThemeColors NOTIFY themeChanged)

    Q_PROPERTY(QColor bg READ bg NOTIFY themeChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceHover READ surfaceHover NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceActive READ surfaceActive NOTIFY themeChanged)
    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY themeChanged)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY themeChanged)
    Q_PROPERTY(QColor border READ border NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QColor accentText READ accentText NOTIFY themeChanged)
    Q_PROPERTY(QColor danger READ danger NOTIFY themeChanged)
    Q_PROPERTY(QColor activeAccent READ activeAccent NOTIFY themeChanged)
    Q_PROPERTY(QColor activeGlow READ activeGlow NOTIFY themeChanged)
    Q_PROPERTY(QColor secondaryAccent READ secondaryAccent NOTIFY themeChanged)
    Q_PROPERTY(QColor warmAccent READ warmAccent NOTIFY themeChanged)
    Q_PROPERTY(QColor success READ success NOTIFY themeChanged)
    Q_PROPERTY(QColor warning READ warning NOTIFY themeChanged)
    Q_PROPERTY(QColor categoryInfo READ categoryInfo NOTIFY themeChanged)
    Q_PROPERTY(QColor categoryNavigation READ categoryNavigation NOTIFY themeChanged)
    Q_PROPERTY(QColor categoryAction READ categoryAction NOTIFY themeChanged)
    Q_PROPERTY(QColor categoryUtility READ categoryUtility NOTIFY themeChanged)
    Q_PROPERTY(QColor categorySystem READ categorySystem NOTIFY themeChanged)
    Q_PROPERTY(QColor overlayScrim READ overlayScrim NOTIFY themeChanged)
    Q_PROPERTY(QColor focusRing READ focusRing NOTIFY themeChanged)
    Q_PROPERTY(QColor panelSurface READ panelSurface NOTIFY themeChanged)
    Q_PROPERTY(QColor panelSurfaceSoft READ panelSurfaceSoft NOTIFY themeChanged)
    Q_PROPERTY(QColor panelSurfaceStrong READ panelSurfaceStrong NOTIFY themeChanged)
    Q_PROPERTY(QColor panelBorder READ panelBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor controlSurface READ controlSurface NOTIFY themeChanged)
    Q_PROPERTY(QColor controlSurfaceActive READ controlSurfaceActive NOTIFY themeChanged)
    Q_PROPERTY(QColor controlBorder READ controlBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor itemHoverFill READ itemHoverFill NOTIFY themeChanged)
    Q_PROPERTY(QColor itemCurrentFill READ itemCurrentFill NOTIFY themeChanged)
    Q_PROPERTY(QColor itemCurrentBorder READ itemCurrentBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor itemSelectedFill READ itemSelectedFill NOTIFY themeChanged)
    Q_PROPERTY(QColor itemSelectedFillInactive READ itemSelectedFillInactive NOTIFY themeChanged)
    Q_PROPERTY(QColor itemSelectedBorder READ itemSelectedBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor itemSelectedBorderInactive READ itemSelectedBorderInactive NOTIFY themeChanged)
    Q_PROPERTY(QColor statusRailFill READ statusRailFill NOTIFY themeChanged)
    Q_PROPERTY(QColor menuBorder READ menuBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor menuSeparator READ menuSeparator NOTIFY themeChanged)
    Q_PROPERTY(QColor menuItemPressed READ menuItemPressed NOTIFY themeChanged)
    Q_PROPERTY(QColor chromeGradientStart READ chromeGradientStart NOTIFY themeChanged)
    Q_PROPERTY(QColor chromeGradientMid READ chromeGradientMid NOTIFY themeChanged)
    Q_PROPERTY(QColor chromeGradientEnd READ chromeGradientEnd NOTIFY themeChanged)
    Q_PROPERTY(QColor glassShadow READ glassShadow NOTIFY themeChanged)
    Q_PROPERTY(QColor shadow READ shadow NOTIFY themeChanged)

public:
    struct ThemePalette {
        QString id;
        QString name;
        bool dark = true;
        QColor bg;
        QColor surface;
        QColor surfaceHover;
        QColor surfaceActive;
        QColor textPrimary;
        QColor textSecondary;
        QColor border;
        QColor accent;
        QColor accentText;
        QColor danger;
        QColor activeAccent;
        QColor activeGlow;
        QColor secondaryAccent;
        QColor warmAccent;
        QColor success;
        QColor warning;
        QColor categoryInfo;
        QColor categoryNavigation;
        QColor categoryAction;
        QColor categoryUtility;
        QColor categorySystem;
        QColor overlayScrim;
        QColor focusRing;
        QColor panelSurface;
        QColor panelSurfaceSoft;
        QColor panelSurfaceStrong;
        QColor panelBorder;
        QColor controlSurface;
        QColor controlSurfaceActive;
        QColor controlBorder;
        QColor itemHoverFill;
        QColor itemCurrentFill;
        QColor itemCurrentBorder;
        QColor itemSelectedFill;
        QColor itemSelectedFillInactive;
        QColor itemSelectedBorder;
        QColor itemSelectedBorderInactive;
        QColor statusRailFill;
        QColor menuBorder;
        QColor menuSeparator;
        QColor menuItemPressed;
        QColor chromeGradientStart;
        QColor chromeGradientMid;
        QColor chromeGradientEnd;
        QColor glassShadow;
        QColor shadow;
    };

    enum ThemeMode {
        Light,
        Dark,
        System
    };
    Q_ENUM(ThemeMode)

    enum ThemeScheme {
        CatppuccinLatte,
        AuroraGlass,
        PorcelainBloom,
        EmberLuxe,
        GraphiteSage,
        VelvetExcess
    };
    Q_ENUM(ThemeScheme)

    explicit ThemeController(QObject *parent = nullptr);

    ThemeMode mode() const;
    void setMode(ThemeMode mode);

    ThemeScheme scheme() const;
    void setScheme(ThemeScheme scheme);

    QString schemeName() const;
    bool customThemeLoaded() const;
    QString themeFilePath() const;

    bool isDark() const;
    QVariantMap systemThemeColors() const;

    QColor bg() const;
    QColor surface() const;
    QColor surfaceHover() const;
    QColor surfaceActive() const;
    QColor textPrimary() const;
    QColor textSecondary() const;
    QColor border() const;
    QColor accent() const;
    QColor accentText() const;
    QColor danger() const;
    QColor activeAccent() const;
    QColor activeGlow() const;
    QColor secondaryAccent() const;
    QColor warmAccent() const;
    QColor success() const;
    QColor warning() const;
    QColor categoryInfo() const;
    QColor categoryNavigation() const;
    QColor categoryAction() const;
    QColor categoryUtility() const;
    QColor categorySystem() const;
    QColor overlayScrim() const;
    QColor focusRing() const;
    QColor panelSurface() const;
    QColor panelSurfaceSoft() const;
    QColor panelSurfaceStrong() const;
    QColor panelBorder() const;
    QColor controlSurface() const;
    QColor controlSurfaceActive() const;
    QColor controlBorder() const;
    QColor itemHoverFill() const;
    QColor itemCurrentFill() const;
    QColor itemCurrentBorder() const;
    QColor itemSelectedFill() const;
    QColor itemSelectedFillInactive() const;
    QColor itemSelectedBorder() const;
    QColor itemSelectedBorderInactive() const;
    QColor statusRailFill() const;
    QColor menuBorder() const;
    QColor menuSeparator() const;
    QColor menuItemPressed() const;
    QColor chromeGradientStart() const;
    QColor chromeGradientMid() const;
    QColor chromeGradientEnd() const;
    QColor glassShadow() const;
    QColor shadow() const;

    Q_INVOKABLE bool saveThemeToFile(const QString &filePath) const;
    Q_INVOKABLE bool loadThemeFromFile(const QString &filePath);
    Q_INVOKABLE QVariantMap currentThemeState() const;
    Q_INVOKABLE bool applyThemeState(const QVariantMap &state);
    Q_INVOKABLE QVariantMap readThemeStateFromFile(const QString &filePath) const;
    Q_INVOKABLE bool writeThemeStateToFile(const QVariantMap &state, const QString &filePath) const;
    Q_INVOKABLE QVariantMap defaultThemeDraft() const;
    Q_INVOKABLE QVariantMap defaultThemeDraftForMode(const QString &mode) const;
    Q_INVOKABLE QVariantList builtInThemeDrafts() const;
    Q_INVOKABLE bool isThemeIdAvailable(const QString &themeId, const QString &excludeFilePath = QString()) const;
    Q_INVOKABLE QString customThemeDirectory() const;
    Q_INVOKABLE QVariantList availableCustomThemes() const;
    QVariantMap exportState() const;
    bool importState(const QVariantMap &state);
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void modeChanged();
    void schemeChanged();
    void themeChanged();

private:
    void updateSystemTheme();
    void handleSystemThemeChanged();
    void loadSettings();
    void saveSettings() const;
    void applyBuiltInScheme(ThemeScheme scheme, bool persist = true);
    void applyPalette(const ThemePalette &palette, bool customPalette, bool persist = true);
    ThemePalette activePalette() const;
    ThemePalette paletteFromSystem() const;
    ThemePalette paletteForScheme(ThemeScheme scheme) const;
    ThemePalette defaultDraftPaletteForMode(bool dark) const;
    ThemeScheme defaultSchemeForSystem() const;
    static QJsonObject themeJsonObject(const ThemePalette &palette);
    static QVariantMap themeStateFromPalette(const ThemePalette &palette);
    static QString colorToString(const QColor &color);
    static QColor colorFromString(const QString &value, const QColor &fallback = QColor());
    static ThemeScheme schemeFromId(const QString &id, bool *ok = nullptr);
    bool paletteFromState(const QVariantMap &state, ThemePalette *palette) const;
    QString resolvedCustomThemeDirectory() const;
    bool loadThemeFromFileInternal(const QString &filePath, bool persist);

    ThemeMode m_mode = System;
    ThemeScheme m_scheme = CatppuccinLatte;
    bool m_systemIsDark = false;
    bool m_hasCustomPalette = false;
    QString m_customThemePath;
    ThemePalette m_customPalette;
};
