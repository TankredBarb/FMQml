#include "FmMenuSeparatorVisual.h"

#include <QPainter>

FmMenuSeparatorVisual::FmMenuSeparatorVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void FmMenuSeparatorVisual::paint(QPainter *painter)
{
    if (width() <= 14.0 || height() <= 1.0)
        return;

    const qreal center = qRound(height() / 2.0) - 0.5;
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(m_lineColor, 1.0, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(QPointF(7.0, center), QPointF(width() - 7.0, center));
    painter->setPen(QPen(m_highlightColor, 1.0, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(QPointF(12.0, center + 1.0), QPointF(width() - 12.0, center + 1.0));
}

#define FM_SETTER(Name, Member) \
    void FmMenuSeparatorVisual::Name(const QColor &value) { if (Member == value) return; Member = value; emit colorsChanged(); update(); }

FM_SETTER(setLineColor, m_lineColor)
FM_SETTER(setHighlightColor, m_highlightColor)

#undef FM_SETTER
