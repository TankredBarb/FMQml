#include "FmIconButtonVisual.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {
QColor iconButtonMixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmIconButtonVisual::FmIconButtonVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmIconButtonVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 2.0 || height() <= 2.0)
        return;

    const qreal activation = qBound(0.0, m_activation, 1.0);
    if (!m_showIdleSurface && !m_active && !m_focused && activation <= 0.0)
        return;

    const QRectF frame(1.0, 1.0, width() - 2.0, height() - 2.0);
    const qreal radius = qMin(frame.width(), frame.height()) / 2.0;
    QColor base = iconButtonMixed(m_surfaceColor, m_accentColor,
                                  m_active ? 0.30 : (0.08 + activation * 0.25));
    base.setAlphaF(base.alphaF() * (m_active ? 0.92 : (m_showIdleSurface ? 0.62 : 0.45) + activation * 0.30));

    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.42);

    QLinearGradient glass(frame.topLeft(), frame.bottomLeft());
    glass.setColorAt(0.0, iconButtonMixed(base, QColor(Qt::white), 0.22));
    glass.setColorAt(0.48, base);
    glass.setColorAt(1.0, iconButtonMixed(base, m_borderColor, 0.18));

    QColor outline = iconButtonMixed(m_borderColor, m_accentColor,
                                     m_focused ? 0.78 : (m_active ? 0.48 : activation * 0.28));
    outline.setAlphaF(outline.alphaF() * (m_active || m_focused ? 0.90 : (m_showIdleSurface ? 0.46 : activation)));
    painter->setPen(QPen(outline, m_focused ? 1.4 : 0.9));
    painter->setBrush(glass);
    painter->drawRoundedRect(frame, radius, radius);

    QPainterPath clip;
    clip.addRoundedRect(frame.adjusted(1.0, 1.0, -1.0, -1.0),
                        qMax(0.0, radius - 1.0), qMax(0.0, radius - 1.0));
    painter->setClipPath(clip);

    if (activation > 0.0) {
        QColor liquid = iconButtonMixed(base, m_accentColor, 0.42);
        liquid.setAlphaF(liquid.alphaF() * (0.18 + activation * 0.30));
        const qreal liquidTop = frame.bottom() - frame.height() * (0.36 + activation * 0.34);
        QLinearGradient fill(QPointF(0.0, liquidTop), frame.bottomLeft());
        fill.setColorAt(0.0, iconButtonMixed(liquid, QColor(Qt::white), 0.18));
        fill.setColorAt(1.0, iconButtonMixed(liquid, m_borderColor, 0.12));
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRect(QRectF(frame.left(), liquidTop, frame.width(), frame.bottom() - liquidTop));
    }

    painter->setClipping(false);
    painter->setOpacity(isEnabled() ? 0.28 : 0.12);
    painter->setPen(QPen(QColor(255, 255, 255, 120), 0.65, Qt::SolidLine, Qt::RoundCap));
    painter->drawArc(frame.adjusted(2.0, 2.0, -2.0, -2.0), 32 * 16, 116 * 16);

    if (m_active) {
        const qreal indicatorWidth = qMax<qreal>(6.0, frame.width() * 0.42);
        const QRectF indicator(frame.center().x() - indicatorWidth / 2.0,
                               frame.bottom() - 3.2, indicatorWidth, 1.8);
        painter->setOpacity(isEnabled() ? 0.92 : 0.38);
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_accentColor);
        painter->drawRoundedRect(indicator, 0.9, 0.9);
    }

    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmIconButtonVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setActive, bool, m_active, stateChanged)
FM_SETTER(setFocused, bool, m_focused, stateChanged)
FM_SETTER(setShowIdleSurface, bool, m_showIdleSurface, stateChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)

void FmIconButtonVisual::setActivation(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_activation, value))
        return;
    m_activation = value;
    emit stateChanged();
    update();
}

#undef FM_SETTER
