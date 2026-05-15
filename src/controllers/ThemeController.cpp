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
    return isDark() ? QColor("#111418") : QColor("#f0f2f5"); // Neutral light gray
}

QColor ThemeController::surface() const
{
    return isDark() ? QColor("#1a1f26") : QColor("#ffffff");
}

QColor ThemeController::surfaceHover() const
{
    // Dark: neutral surface lift. Light: cool surface lift.
    return isDark() ? QColor("#262d38") : QColor("#e7eef7");
}

QColor ThemeController::surfaceActive() const
{
    return isDark() ? QColor("#212832") : QColor("#d8ecff");
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
    return isDark() ? QColor("#53b7ff") : QColor("#0f7bd8");
}

QColor ThemeController::accentText() const
{
    return QColor("#ffffff");
}

QColor ThemeController::danger() const
{
    return isDark() ? QColor("#e65c68") : QColor("#c2414b");
}

QColor ThemeController::activeAccent() const
{
    // Neon Green for dark, Royal Blue for light
    return isDark() ? QColor("#aaff00") : QColor("#0055ff");
}

QColor ThemeController::activeGlow() const
{
    // Vibrant neon green glow for dark, subtle blue glow for light
    return isDark() ? QColor(170, 255, 0, 102) : QColor(0, 85, 255, 45);
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
