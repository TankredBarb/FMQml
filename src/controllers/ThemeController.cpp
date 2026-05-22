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
    return isDark() ? QColor("#080b10") : QColor("#f8fafc"); // Slate background
}

QColor ThemeController::surface() const
{
    return isDark() ? QColor("#111520") : QColor("#ffffff");
}

QColor ThemeController::surfaceHover() const
{
    return isDark() ? QColor("#1d2433") : QColor("#f1f5f9");
}

QColor ThemeController::surfaceActive() const
{
    return isDark() ? QColor("#171c28") : QColor("#e2e8f0");
}

QColor ThemeController::textPrimary() const
{
    return isDark() ? QColor("#f1f5f9") : QColor("#0f172a"); // Premium off-black for light
}

QColor ThemeController::textSecondary() const
{
    return isDark() ? QColor("#94a3b8") : QColor("#64748b");
}

QColor ThemeController::border() const
{
    return isDark() ? QColor("#1e293b") : QColor("#cbd5e1"); // Soft borders
}

QColor ThemeController::accent() const
{
    return isDark() ? QColor("#3b82f6") : QColor("#2563eb");
}

QColor ThemeController::accentText() const
{
    return QColor("#ffffff");
}

QColor ThemeController::danger() const
{
    return isDark() ? QColor("#f87171") : QColor("#dc2626");
}

QColor ThemeController::activeAccent() const
{
    return accent(); // Dynamically follow the accent color
}

QColor ThemeController::activeGlow() const
{
    QColor c = activeAccent();
    c.setAlpha(isDark() ? 50 : 30); // Dynamic alpha glow based on accent color
    return c;
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
