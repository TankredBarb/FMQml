#include "FmSwitchVisual.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {
QColor switchMixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmSwitchVisual::FmSwitchVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmSwitchVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 0.0 || height() <= 0.0)
        return;

    const qreal trackWidth = qMin<qreal>(42.0, width());
    const qreal trackHeight = qMin<qreal>(22.0, height());
    const QRectF track((width() - trackWidth) / 2.0,
                       (height() - trackHeight) / 2.0,
                       trackWidth,
                       trackHeight);
    const QRectF innerTrack = track.adjusted(1.0, 1.0, -1.0, -1.0);
    const qreal clampedPosition = qBound(0.0, m_position, 1.0);

    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.46);
    painter->setPen(QPen(m_borderColor, 1.0));
    painter->setBrush(m_surfaceColor);
    painter->drawRoundedRect(track, trackHeight / 2.0, trackHeight / 2.0);

    if (clampedPosition > 0.0 && innerTrack.width() > 0.0) {
        QPainterPath clip;
        clip.addRoundedRect(innerTrack, innerTrack.height() / 2.0, innerTrack.height() / 2.0);
        painter->setClipPath(clip);

        const qreal liquidWidth = innerTrack.width() * clampedPosition;
        const QRectF liquid(innerTrack.left(), innerTrack.top(), liquidWidth, innerTrack.height());
        const QColor liquidColor = switchMixed(m_surfaceColor, m_accentColor, 0.56);
        QLinearGradient liquidGradient(liquid.topLeft(), liquid.bottomLeft());
        liquidGradient.setColorAt(0.0, switchMixed(liquidColor, QColor(Qt::white), 0.24));
        liquidGradient.setColorAt(0.32, liquidColor);
        liquidGradient.setColorAt(0.72, liquidColor);
        liquidGradient.setColorAt(1.0, switchMixed(liquidColor, m_borderColor, 0.20));
        painter->setPen(QPen(switchMixed(liquidColor, m_borderColor, 0.18), 0.75));
        painter->setBrush(liquidGradient);
        painter->drawRoundedRect(liquid, innerTrack.height() / 2.0, innerTrack.height() / 2.0);

        if (liquid.width() > 6.0) {
            painter->setPen(QPen(QColor(255, 255, 255, 102), 1.0, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(QPointF(liquid.left() + 3.0, liquid.top() + 2.0),
                              QPointF(liquid.right() - 3.0, liquid.top() + 2.0));
        }
        painter->setClipping(false);
    }

    const qreal knobDiameter = 17.0;
    const qreal knobTravel = qMax(0.0, innerTrack.width() - knobDiameter);
    const qreal knobX = innerTrack.left() + knobTravel * clampedPosition;
    const QRectF knob(knobX,
                      track.center().y() - knobDiameter / 2.0,
                      knobDiameter,
                      knobDiameter);
    const QColor knobBorder = m_active || m_hovered || clampedPosition > 0.5
        ? m_accentColor
        : switchMixed(m_borderColor, m_idleColor, 0.26);
    QLinearGradient knobGradient(knob.topLeft(), knob.bottomLeft());
    knobGradient.setColorAt(0.0, switchMixed(m_surfaceColor, QColor(Qt::white), m_active ? 0.48 : 0.34));
    knobGradient.setColorAt(0.54, switchMixed(m_surfaceColor, m_accentColor,
                                              m_active ? 0.30 : 0.14 * clampedPosition));
    knobGradient.setColorAt(1.0, switchMixed(m_surfaceColor, m_borderColor, 0.24));
    painter->setPen(QPen(knobBorder, m_active ? 2.0 : 1.25));
    painter->setBrush(knobGradient);
    painter->drawEllipse(knob);

    QColor centerColor = clampedPosition > 0.5 ? m_accentColor : m_idleColor;
    centerColor.setAlphaF(m_active ? 0.90 : (m_hovered ? 0.72 : 0.52));
    painter->setPen(Qt::NoPen);
    painter->setBrush(centerColor);
    painter->drawEllipse(QRectF(knob.center().x() - 2.25, knob.center().y() - 2.25, 4.5, 4.5));

    painter->setPen(QPen(QColor(255, 255, 255, 122), 1.0, Qt::SolidLine, Qt::RoundCap));
    painter->drawArc(knob.adjusted(3.5, 3.0, -3.5, -4.5), 35 * 16, 110 * 16);
    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmSwitchVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setActive, bool, m_active, activeChanged)
FM_SETTER(setHovered, bool, m_hovered, hoveredChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setIdleColor, const QColor &, m_idleColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)

void FmSwitchVisual::setPosition(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_position, value))
        return;
    m_position = value;
    emit positionChanged();
    update();
}

#undef FM_SETTER
