#include "FmButtonVisual.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {
QColor buttonMixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmButtonVisual::FmButtonVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmButtonVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 1.0 || height() <= 1.0)
        return;

    const QRectF frame(0.5, 0.5, width() - 1.0, height() - 1.0);
    const qreal radius = qMin<qreal>(7.0, frame.height() / 2.0);
    const qreal activation = qBound(0.0, m_activation, 1.0);
    const bool visibleAtRest = m_primary || !m_flat || activation > 0.0 || m_active;
    if (!visibleAtRest)
        return;

    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.42);

    QColor base = buttonMixed(m_surfaceColor, m_accentColor,
                              m_primary ? (m_destructive ? 0.055 : 0.075)
                                        : 0.08 + activation * 0.22);
    if (m_flat && !m_primary)
        base.setAlphaF(base.alphaF() * (0.20 + activation * 0.65));

    QLinearGradient glass(frame.topLeft(), frame.bottomLeft());
    glass.setColorAt(0.0, buttonMixed(base, QColor(Qt::white), m_primary ? 0.24 : 0.20));
    glass.setColorAt(0.46, base);
    glass.setColorAt(1.0, buttonMixed(base, m_borderColor, m_primary ? 0.14 : 0.20));

    QColor outline = m_primary
        ? buttonMixed(m_borderColor, m_accentColor,
                      (m_destructive ? 0.58 : 0.46) + activation * 0.18)
        : (m_active ? buttonMixed(m_borderColor, m_accentColor, 0.78) : m_borderColor);
    if (m_flat && !m_primary)
        outline.setAlphaF(outline.alphaF() * activation);
    painter->setPen(QPen(outline, m_active ? 1.4 : 0.9));
    painter->setBrush(glass);
    painter->drawRoundedRect(frame, radius, radius);

    QPainterPath clip;
    clip.addRoundedRect(frame.adjusted(1.0, 1.0, -1.0, -1.0),
                        qMax(0.0, radius - 1.0), qMax(0.0, radius - 1.0));
    painter->setClipPath(clip);

    if (m_primary) {
        const qreal level = (m_destructive ? 0.30 : 0.26) + activation * 0.34;
        const QRectF liquidRect(frame.left(), frame.bottom() - frame.height() * level + 1.0,
                                frame.width(), frame.height() * level);
        QColor liquid = buttonMixed(base, m_accentColor, m_destructive ? 0.68 : 0.58);
        liquid.setAlphaF(qMin(1.0, liquid.alphaF()
                                  * ((m_destructive ? 0.48 : 0.38) + activation * 0.22)));
        QLinearGradient fill(liquidRect.topLeft(), liquidRect.bottomLeft());
        fill.setColorAt(0.0, buttonMixed(liquid, QColor(Qt::white), 0.20));
        fill.setColorAt(0.42, liquid);
        fill.setColorAt(1.0, buttonMixed(liquid, m_borderColor, 0.16));
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRect(liquidRect);

        QColor meniscus = buttonMixed(m_accentColor, QColor(Qt::white), 0.28);
        meniscus.setAlphaF(0.30 + activation * 0.22);
        painter->setPen(QPen(meniscus, 0.8, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(QPointF(frame.left() + radius * 0.55, liquidRect.top() + 0.6),
                          QPointF(frame.right() - radius * 0.55, liquidRect.top() + 0.6));
    } else if (activation > 0.0) {
        QColor liquid = buttonMixed(base, m_accentColor, 0.40);
        liquid.setAlphaF(qMin(1.0, liquid.alphaF() * (0.20 + activation * 0.34)));
        QLinearGradient fill(frame.topLeft(), frame.bottomLeft());
        fill.setColorAt(0.0, buttonMixed(liquid, QColor(Qt::white), 0.20));
        fill.setColorAt(0.55, liquid);
        fill.setColorAt(1.0, buttonMixed(liquid, m_borderColor, 0.16));
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRect(frame);
    }

    painter->setClipping(false);
    painter->setOpacity(isEnabled() ? 0.32 : 0.14);
    painter->setPen(QPen(QColor(255, 255, 255, 120), 0.7, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(QPointF(frame.left() + radius, frame.top() + 2.2),
                      QPointF(frame.right() - radius, frame.top() + 2.2));
    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmButtonVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setPrimary, bool, m_primary, stateChanged)
FM_SETTER(setDestructive, bool, m_destructive, stateChanged)
FM_SETTER(setFlat, bool, m_flat, stateChanged)
FM_SETTER(setActive, bool, m_active, stateChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)

void FmButtonVisual::setActivation(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_activation, value))
        return;
    m_activation = value;
    emit stateChanged();
    update();
}

#undef FM_SETTER
