#include "FmMenuVisual.h"

#include <QLinearGradient>
#include <QPainter>

namespace {
QColor menuMixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmMenuVisual::FmMenuVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void FmMenuVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 2.0 || height() <= 2.0)
        return;

    const QRectF shadowFrame(1.5, 2.5, width() - 3.0, height() - 3.5);
    const QRectF frame(0.5, 0.5, width() - 1.0, height() - 2.0);
    const qreal radius = qMin<qreal>(9.0, frame.height() / 2.0);

    painter->save();
    QColor shadow = m_shadowColor;
    shadow.setAlphaF(shadow.alphaF() * (m_dark ? 0.28 : 0.10));
    painter->setPen(Qt::NoPen);
    painter->setBrush(shadow);
    painter->drawRoundedRect(shadowFrame, radius, radius);

    QLinearGradient surface(frame.topLeft(), frame.bottomLeft());
    surface.setColorAt(0.0, menuMixed(m_surfaceColor, m_accentColor, m_dark ? 0.055 : 0.035));
    surface.setColorAt(0.42, m_surfaceColor);
    surface.setColorAt(1.0, menuMixed(m_surfaceColor, m_borderColor, 0.08));

    QColor outline = m_borderColor;
    outline.setAlphaF(outline.alphaF() * (m_dark ? 0.40 : 0.28));
    painter->setPen(QPen(outline, 1.0));
    painter->setBrush(surface);
    painter->drawRoundedRect(frame, radius, radius);

    QColor highlight = menuMixed(m_surfaceColor, QColor(Qt::white), m_dark ? 0.16 : 0.34);
    highlight.setAlphaF(m_dark ? 0.10 : 0.18);
    painter->setPen(QPen(highlight, 0.8, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(QPointF(frame.left() + 6.0, frame.top() + 1.5),
                      QPointF(frame.right() - 6.0, frame.top() + 1.5));
    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmMenuVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)
FM_SETTER(setShadowColor, const QColor &, m_shadowColor, colorsChanged)
FM_SETTER(setDark, bool, m_dark, stateChanged)

#undef FM_SETTER
