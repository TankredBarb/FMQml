#include "FmScrollBarVisual.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {
QColor mixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}

void drawChevron(QPainter *painter, const QRectF &area, bool upward, const QColor &color)
{
    const QPointF center = area.center();
    const qreal direction = upward ? -1.0 : 1.0;
    QPainterPath path;
    path.moveTo(center.x() - 3.5, center.y() - direction * 1.75);
    path.lineTo(center.x(), center.y() + direction * 1.75);
    path.lineTo(center.x() + 3.5, center.y() - direction * 1.75);
    painter->setPen(QPen(color, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}
}

FmScrollBarVisual::FmScrollBarVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void FmScrollBarVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool horizontal = m_orientation == Qt::Horizontal;
    const qreal logicalWidth = horizontal ? height() : width();
    const qreal logicalHeight = horizontal ? width() : height();
    if (horizontal) {
        painter->translate(0.0, height());
        painter->rotate(-90.0);
    }

    if (m_flat) {
        constexpr qreal thumbWidth = 4.0;
        const qreal trackHeight = logicalHeight;
        const qreal thumbHeight = qMax(24.0, trackHeight * qBound(0.0, m_visualSize, 1.0));
        const qreal travel = qMax(0.0, trackHeight - thumbHeight);
        const qreal positionRange = qMax(0.0, 1.0 - qBound(0.0, m_visualSize, 1.0));
        const qreal positionRatio = positionRange > 0.0
            ? qBound(0.0, m_visualPosition / positionRange, 1.0)
            : 0.0;
        QColor thumbColor = m_active ? m_accentColor : m_idleColor;
        thumbColor.setAlphaF(m_active ? 0.78 : 0.38);
        painter->setPen(Qt::NoPen);
        painter->setBrush(thumbColor);
        painter->drawRoundedRect(QRectF((logicalWidth - thumbWidth) / 2.0,
                                        travel * positionRatio,
                                        thumbWidth,
                                        qMin(thumbHeight, trackHeight)),
                                 thumbWidth / 2.0,
                                 thumbWidth / 2.0);
        return;
    }

    constexpr qreal arrowExtent = 14.0;
    constexpr qreal trackWidth = 7.0;
    const qreal trackHeight = qMax(0.0, logicalHeight - arrowExtent * 2.0);
    const QRectF track((logicalWidth - trackWidth) / 2.0, arrowExtent, trackWidth, trackHeight);

    painter->setPen(QPen(m_borderColor, 1.0));
    painter->setBrush(m_surfaceColor);
    painter->drawRoundedRect(track, trackWidth / 2.0, trackWidth / 2.0);

    if (trackHeight > 0.0) {
        const qreal thumbHeight = qMax(28.0, trackHeight * qBound(0.0, m_visualSize, 1.0));
        const qreal travel = qMax(0.0, trackHeight - thumbHeight);
        const qreal positionRange = qMax(0.0, 1.0 - qBound(0.0, m_visualSize, 1.0));
        const qreal positionRatio = positionRange > 0.0
            ? qBound(0.0, m_visualPosition / positionRange, 1.0)
            : 0.0;
        const qreal thumbY = track.top() + travel * positionRatio;
        const QRectF thumb(track.left(), thumbY, trackWidth, qMin(thumbHeight, trackHeight));

        painter->setPen(QPen(mixed(m_idleColor, m_borderColor, 0.34), 1.0));
        painter->setBrush(mixed(m_surfaceColor, m_idleColor, 0.28));
        painter->drawRoundedRect(thumb, trackWidth / 2.0, trackWidth / 2.0);

        if (m_active && thumb.width() > 2.0 && thumb.height() > 2.0) {
            const QRectF liquid = thumb.adjusted(1.0, 1.0, -1.0, -1.0);
            const QColor liquidColor = mixed(m_surfaceColor, m_accentColor, 0.56);
            QLinearGradient gradient(liquid.topLeft(), liquid.topRight());
            gradient.setColorAt(0.0, mixed(liquidColor, m_borderColor, 0.24));
            gradient.setColorAt(0.45, liquidColor);
            gradient.setColorAt(1.0, mixed(liquidColor, QColor(Qt::white), 0.28));
            painter->setPen(Qt::NoPen);
            painter->setBrush(gradient);
            painter->drawRoundedRect(liquid, 2.5, 2.5);

            painter->setPen(QPen(QColor(255, 255, 255, 90), 1.0, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(QPointF(liquid.left() + 1.5, liquid.top() + 2.0),
                              QPointF(liquid.left() + 1.5, liquid.bottom() - 2.0));
        }
    }

    const QColor decreaseColor = m_decreaseHovered ? m_accentColor : m_idleColor;
    const QColor increaseColor = m_increaseHovered ? m_accentColor : m_idleColor;
    drawChevron(painter, QRectF(0.0, 0.0, logicalWidth, arrowExtent), true, decreaseColor);
    drawChevron(painter, QRectF(0.0, logicalHeight - arrowExtent, logicalWidth, arrowExtent), false, increaseColor);
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmScrollBarVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setActive, bool, m_active, activeChanged)
FM_SETTER(setFlat, bool, m_flat, flatChanged)
FM_SETTER(setOrientation, Qt::Orientation, m_orientation, orientationChanged)
FM_SETTER(setDecreaseHovered, bool, m_decreaseHovered, decreaseHoveredChanged)
FM_SETTER(setIncreaseHovered, bool, m_increaseHovered, increaseHoveredChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setIdleColor, const QColor &, m_idleColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)

void FmScrollBarVisual::setVisualPosition(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_visualPosition, value)) return;
    m_visualPosition = value;
    emit visualPositionChanged();
    update();
}

void FmScrollBarVisual::setVisualSize(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_visualSize, value)) return;
    m_visualSize = value;
    emit visualSizeChanged();
    update();
}

#undef FM_SETTER
