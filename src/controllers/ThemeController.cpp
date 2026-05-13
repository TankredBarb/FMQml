#include "ThemeController.h"
#include <QGuiApplication>
#include <QPalette>

ThemeController::ThemeController(QObject *parent)
    : QObject(parent)
{
    updateSystemTheme();
    // In a real app, we'd connect to system palette changes
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
    emit themeChanged();
}

bool ThemeController::isDark() const
{
    return calculateIsDark();
}

QColor ThemeController::bg() const
{
    return isDark() ? QColor("#1a1c1e") : QColor("#f0f2f5"); // Neutral light gray
}

QColor ThemeController::surface() const
{
    return isDark() ? QColor("#242629") : QColor("#ffffff");
}

QColor ThemeController::surfaceHover() const
{
    // Dark: Rich Burgundy (Ubuntu-inspired). Light: Saturated warm Amber-Orange.
    return isDark() ? QColor(136, 14, 79, 120) : QColor(255, 152, 0, 80); 
}

QColor ThemeController::surfaceActive() const
{
    return isDark() ? QColor("#004d40") : QColor("#b2ebf2"); // Vibrant Teal/Cyan selection
}

QColor ThemeController::textPrimary() const
{
    return isDark() ? QColor("#e2e2e6") : QColor("#000000"); // Pure black for light theme
}

QColor ThemeController::textSecondary() const
{
    return isDark() ? QColor("#90949a") : QColor("#444444");
}

QColor ThemeController::border() const
{
    return isDark() ? QColor("#3f434a") : QColor("#808080"); // Slightly darker border for Light theme
}

QColor ThemeController::accent() const
{
    return isDark() ? QColor("#00e5ff") : QColor("#0097a7"); // Neon Cyan for Dark, Deep Teal/Cyan for Light
}

QColor ThemeController::accentText() const
{
    return QColor("#ffffff");
}

QColor ThemeController::danger() const
{
    return isDark() ? QColor("#e65c68") : QColor("#c2414b");
}

void ThemeController::updateSystemTheme()
{
    const QPalette palette = QGuiApplication::palette();
    m_systemIsDark = palette.color(QPalette::WindowText).lightness() > palette.color(QPalette::Window).lightness();
}

bool ThemeController::calculateIsDark() const
{
    if (m_mode == System) {
        return m_systemIsDark;
    }
    return m_mode == Dark;
}
