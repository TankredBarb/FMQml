#include "FmMenuItemVisual.h"

#include <QPainter>

FmMenuItemVisual::FmMenuItemVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmMenuItemVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 1.0 || height() <= 1.0)
        return;

    const qreal activation = qBound(0.0, m_activation, 1.0);
    const QRectF frame(0.0, 0.0, width(), height());
    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.38);

    if (activation > 0.0) {
        QColor fill = m_pressed ? m_pressedColor : m_hoverColor;
        fill.setAlphaF(fill.alphaF() * activation);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(frame, 2.0, 2.0);

        QColor accentWash = m_accentColor;
        accentWash.setAlphaF(0.08 * activation);
        painter->setBrush(accentWash);
        painter->drawRoundedRect(frame, 2.0, 2.0);

        QColor outline = m_accentColor;
        outline.setAlphaF((m_pressed ? 0.38 : 0.30) * activation);
        painter->setPen(QPen(outline, 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(frame.adjusted(0.5, 0.5, -0.5, -0.5), 2.0, 2.0);
    }

    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmMenuItemVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setPressed, bool, m_pressed, stateChanged)
FM_SETTER(setHoverColor, const QColor &, m_hoverColor, colorsChanged)
FM_SETTER(setPressedColor, const QColor &, m_pressedColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)

void FmMenuItemVisual::setActivation(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_activation, value))
        return;
    m_activation = value;
    emit stateChanged();
    update();
}

#undef FM_SETTER
