#include "FmSliderVisual.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {
QColor sliderMixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmSliderVisual::FmSliderVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmSliderVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 0.0 || height() <= 0.0)
        return;

    const qreal handleDiameter = qMin(m_handleSize, height());
    const qreal handleRadius = handleDiameter / 2.0;
    const qreal trackHeight = qMin(m_trackHeight, height());
    const QRectF track(handleRadius,
                       (height() - trackHeight) / 2.0,
                       qMax(0.0, width() - handleDiameter),
                       trackHeight);
    const qreal handleCenterX = track.left() + track.width() * qBound(0.0, m_visualPosition, 1.0);
    const QRectF handle(handleCenterX - handleRadius,
                        (height() - handleDiameter) / 2.0,
                        handleDiameter,
                        handleDiameter);
    const qreal stateOpacity = isEnabled() ? 1.0 : 0.46;

    painter->save();
    painter->setOpacity(stateOpacity);
    painter->setPen(QPen(m_borderColor, 1.0));
    painter->setBrush(m_surfaceColor);
    painter->drawRoundedRect(track, trackHeight / 2.0, trackHeight / 2.0);

    const qreal fillWidth = qBound(0.0, handleCenterX - track.left(), track.width());
    if (fillWidth > 0.0) {
        QPainterPath trackClip;
        trackClip.addRoundedRect(track.adjusted(1.0, 1.0, -1.0, -1.0),
                                 (trackHeight - 2.0) / 2.0,
                                 (trackHeight - 2.0) / 2.0);
        painter->setClipPath(trackClip);

        const QRectF filled(track.left(), track.top(), fillWidth, track.height());
        painter->setPen(QPen(sliderMixed(m_idleColor, m_borderColor, 0.34), 1.0));
        painter->setBrush(sliderMixed(m_surfaceColor, m_idleColor, 0.28));
        painter->drawRect(filled);

        const QRectF liquid = filled.adjusted(1.0, 1.0, -1.0, -1.0);
        if (liquid.width() > 0.0 && liquid.height() > 0.0) {
            const QColor liquidColor = sliderMixed(m_surfaceColor, m_accentColor, 0.56);
            QLinearGradient liquidGradient(liquid.topLeft(), liquid.bottomLeft());
            liquidGradient.setColorAt(0.0, sliderMixed(liquidColor, QColor(Qt::white), 0.24));
            liquidGradient.setColorAt(0.32, liquidColor);
            liquidGradient.setColorAt(0.72, liquidColor);
            liquidGradient.setColorAt(1.0, sliderMixed(liquidColor, m_borderColor, 0.20));
            painter->setPen(QPen(sliderMixed(liquidColor, m_borderColor, 0.18), 0.75));
            painter->setBrush(liquidGradient);
            painter->drawRoundedRect(liquid, 2.5, 2.5);

            painter->setPen(QPen(QColor(255, 255, 255, 108), 1.0, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(QPointF(liquid.left() + 1.5, liquid.top() + 1.0),
                              QPointF(liquid.right() - 1.5, liquid.top() + 1.0));
        }
        painter->setClipping(false);
    }

    const QColor handleBorder = m_active || m_hovered
        ? m_accentColor
        : sliderMixed(m_borderColor, m_idleColor, 0.26);
    QLinearGradient handleGradient(handle.topLeft(), handle.bottomLeft());
    handleGradient.setColorAt(0.0, sliderMixed(m_surfaceColor, QColor(Qt::white), m_active ? 0.48 : 0.34));
    handleGradient.setColorAt(0.54, sliderMixed(m_surfaceColor, m_accentColor, m_active ? 0.30 : 0.14));
    handleGradient.setColorAt(1.0, sliderMixed(m_surfaceColor, m_borderColor, 0.24));
    painter->setPen(QPen(handleBorder, m_active ? 2.0 : 1.25));
    painter->setBrush(handleGradient);
    painter->drawEllipse(handle.adjusted(1.0, 1.0, -1.0, -1.0));

    painter->setPen(Qt::NoPen);
    QColor centerColor = m_accentColor;
    centerColor.setAlphaF(m_active ? 0.90 : (m_hovered ? 0.72 : 0.52));
    painter->setBrush(centerColor);
    painter->drawEllipse(QRectF(handleCenterX - 2.5, height() / 2.0 - 2.5, 5.0, 5.0));

    painter->setPen(QPen(QColor(255, 255, 255, 122), 1.0, Qt::SolidLine, Qt::RoundCap));
    painter->drawArc(handle.adjusted(4.0, 3.0, -4.0, -5.0), 35 * 16, 110 * 16);
    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmSliderVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setActive, bool, m_active, activeChanged)
FM_SETTER(setHovered, bool, m_hovered, hoveredChanged)
FM_SETTER(setHandleSize, qreal, m_handleSize, geometryChanged)
FM_SETTER(setTrackHeight, qreal, m_trackHeight, geometryChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setIdleColor, const QColor &, m_idleColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)

void FmSliderVisual::setVisualPosition(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_visualPosition, value))
        return;
    m_visualPosition = value;
    emit visualPositionChanged();
    update();
}

#undef FM_SETTER
