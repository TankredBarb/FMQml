#include "FmRubberBandVisual.h"

#include <QPainter>

FmRubberBandVisual::FmRubberBandVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void FmRubberBandVisual::paint(QPainter *painter)
{
    if (width() <= m_lineWidth || height() <= m_lineWidth || m_lineWidth <= 0.0)
        return;

    const qreal inset = m_lineWidth / 2.0;
    const QRectF frame(inset, inset, width() - m_lineWidth, height() - m_lineWidth);
    const qreal radius = qMin(m_radius, qMin(frame.width(), frame.height()) / 2.0);
    QPen pen(m_strokeColor, m_lineWidth, Qt::CustomDashLine, Qt::RoundCap, Qt::RoundJoin);
    pen.setDashPattern({m_dashLength / m_lineWidth, m_gapLength / m_lineWidth});

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(pen);
    painter->drawRoundedRect(frame, radius, radius);
}

#define FM_SETTER(Name, Type, Member) \
    void FmRubberBandVisual::Name(Type value) { if (Member == value) return; Member = value; emit visualChanged(); update(); }

FM_SETTER(setStrokeColor, const QColor &, m_strokeColor)
FM_SETTER(setRadius, qreal, m_radius)
FM_SETTER(setLineWidth, qreal, m_lineWidth)
FM_SETTER(setDashLength, qreal, m_dashLength)
FM_SETTER(setGapLength, qreal, m_gapLength)

#undef FM_SETTER
