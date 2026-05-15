#pragma once

#include <QObject>
#include <QColor>

class ThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(ThemeMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool isDark READ isDark NOTIFY themeChanged)
    
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

public:
    enum ThemeMode {
        Light,
        Dark,
        System
    };
    Q_ENUM(ThemeMode)

    explicit ThemeController(QObject *parent = nullptr);

    ThemeMode mode() const;
    void setMode(ThemeMode mode);

    bool isDark() const;

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

signals:
    void modeChanged();
    void themeChanged();

private:
    void updateSystemTheme();
    bool calculateIsDark() const;

    ThemeMode m_mode = System;
    bool m_systemIsDark = false;
};
