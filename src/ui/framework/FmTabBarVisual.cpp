#include "FmTabBarVisual.h"

#include <QLinearGradient>
#include <QPainter>

namespace {
QColor mixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmTabBarVisual::FmTabBarVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void FmTabBarVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 1.0 || height() <= 1.0)
        return;

    const QRectF frame(0.5, 0.5, width() - 1.0, height() - 1.0);
    const qreal radius = qMin<qreal>(9.0, frame.height() / 2.0);

    QLinearGradient base(frame.topLeft(), frame.bottomLeft());
    base.setColorAt(0.0, mixed(m_surfaceColor, QColor(Qt::white), 0.10));
    base.setColorAt(0.52, m_surfaceColor);
    base.setColorAt(1.0, mixed(m_surfaceColor, m_borderColor, 0.12));
    painter->setPen(QPen(m_borderColor, 0.9));
    painter->setBrush(base);
    painter->drawRoundedRect(frame, radius, radius);

    if (m_hoverAmount > 0.001 && m_hoverWidth > 0.0) {
        QColor hover = m_accentColor;
        hover.setAlphaF(0.055 * qBound(0.0, m_hoverAmount, 1.0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(hover);
        painter->drawRoundedRect(QRectF(m_hoverX, 4.0, m_hoverWidth, height() - 8.0), 7.0, 7.0);
    }

    if (m_highlightWidth <= 0.0)
        return;

    const QRectF highlight(m_highlightX, 4.0, m_highlightWidth, height() - 8.0);
    QColor fill = m_accentColor;
    fill.setAlphaF(0.15);
    QLinearGradient liquid(highlight.topLeft(), highlight.bottomLeft());
    liquid.setColorAt(0.0, mixed(fill, QColor(Qt::white), 0.20));
    liquid.setColorAt(0.50, fill);
    liquid.setColorAt(1.0, mixed(fill, m_borderColor, 0.12));
    QColor outline = m_accentColor;
    outline.setAlphaF(0.30);
    painter->setPen(QPen(outline, 1.0));
    painter->setBrush(liquid);
    painter->drawRoundedRect(highlight, 7.0, 7.0);
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmTabBarVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setHighlightX, qreal, m_highlightX, geometryChanged)
FM_SETTER(setHighlightWidth, qreal, m_highlightWidth, geometryChanged)
FM_SETTER(setHoverX, qreal, m_hoverX, geometryChanged)
FM_SETTER(setHoverWidth, qreal, m_hoverWidth, geometryChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)

void FmTabBarVisual::setHoverAmount(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_hoverAmount, value))
        return;
    m_hoverAmount = value;
    emit geometryChanged();
    update();
}

#undef FM_SETTER
