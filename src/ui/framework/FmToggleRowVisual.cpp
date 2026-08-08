#include "FmToggleRowVisual.h"

#include <QPainter>

FmToggleRowVisual::FmToggleRowVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmToggleRowVisual::paint(QPainter *painter)
{
    if (width() <= 0.0 || height() <= 0.0)
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.55);

    const QRectF bounds(0.5, 0.5, width() - 1.0, height() - 1.0);
    QColor fill = m_hovered ? m_hoverColor : m_surfaceColor;
    if (m_pressed)
        fill = QColor::fromRgbF(fill.redF(), fill.greenF(), fill.blueF(), fill.alphaF() * 0.82);

    QColor stroke = m_checked ? m_accentColor : m_borderColor;
    stroke.setAlphaF(m_checked ? 0.42 : stroke.alphaF());
    painter->setPen(QPen(stroke, 1.0));
    painter->setBrush(fill);
    painter->drawRoundedRect(bounds, m_radius, m_radius);

    QColor marker = m_accentColor;
    marker.setAlphaF(m_checked ? 0.90 : 0.48);
    painter->setPen(Qt::NoPen);
    painter->setBrush(marker);
    painter->drawRoundedRect(QRectF(7.0, 7.0, 2.0, qMax(0.0, height() - 14.0)), 1.0, 1.0);
    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmToggleRowVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setChecked, bool, m_checked, stateChanged)
FM_SETTER(setHovered, bool, m_hovered, stateChanged)
FM_SETTER(setPressed, bool, m_pressed, stateChanged)
FM_SETTER(setRadius, qreal, m_radius, geometryChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setHoverColor, const QColor &, m_hoverColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)

#undef FM_SETTER
