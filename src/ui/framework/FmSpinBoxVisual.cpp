#include "FmSpinBoxVisual.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {
QColor spinBoxMixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmSpinBoxVisual::FmSpinBoxVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmSpinBoxVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 1.0 || height() <= 1.0)
        return;

    const QRectF frame(0.5, 0.5, width() - 1.0, height() - 1.0);
    const qreal radius = qMin<qreal>(7.0, frame.height() / 2.0);
    const qreal chamberWidth = qMin<qreal>(30.0, frame.width() * 0.30);

    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.46);

    QLinearGradient glass(frame.topLeft(), frame.bottomLeft());
    glass.setColorAt(0.0, spinBoxMixed(m_surfaceColor, QColor(Qt::white), 0.22));
    glass.setColorAt(0.48, m_surfaceColor);
    glass.setColorAt(1.0, spinBoxMixed(m_surfaceColor, m_borderColor, 0.16));
    painter->setPen(QPen(spinBoxMixed(m_borderColor, m_accentColor, m_active ? 0.72 : 0.0),
                         m_active ? 1.4 : 0.9));
    painter->setBrush(glass);
    painter->drawRoundedRect(frame, radius, radius);

    QPainterPath clip;
    clip.addRoundedRect(frame.adjusted(1.0, 1.0, -1.0, -1.0),
                        qMax(0.0, radius - 1.0), qMax(0.0, radius - 1.0));
    painter->setClipPath(clip);

    const QRectF decrease(frame.left(), frame.top(), chamberWidth, frame.height());
    const QRectF increase(frame.right() - chamberWidth, frame.top(), chamberWidth, frame.height());
    const auto drawChamber = [&](const QRectF &rect, qreal activation) {
        activation = qBound(0.0, activation, 1.0);
        const QColor liquid = spinBoxMixed(m_surfaceColor, m_accentColor, 0.14 + activation * 0.48);
        QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
        gradient.setColorAt(0.0, spinBoxMixed(liquid, QColor(Qt::white), 0.24));
        gradient.setColorAt(0.42, liquid);
        gradient.setColorAt(1.0, spinBoxMixed(liquid, m_borderColor, 0.18));
        painter->setPen(Qt::NoPen);
        painter->setBrush(gradient);
        painter->drawRect(rect);
    };
    drawChamber(decrease, m_decreaseActivation);
    drawChamber(increase, m_increaseActivation);

    QColor divider = m_borderColor;
    divider.setAlphaF(0.62);
    painter->setPen(QPen(divider, 0.8));
    painter->drawLine(QPointF(decrease.right(), frame.top() + 3.0),
                      QPointF(decrease.right(), frame.bottom() - 3.0));
    painter->drawLine(QPointF(increase.left(), frame.top() + 3.0),
                      QPointF(increase.left(), frame.bottom() - 3.0));

    painter->setPen(QPen(m_indicatorColor, 1.3, Qt::SolidLine, Qt::RoundCap));
    const QPointF minusCenter = decrease.center();
    painter->drawLine(minusCenter + QPointF(-4.0, 0.0), minusCenter + QPointF(4.0, 0.0));
    const QPointF plusCenter = increase.center();
    painter->drawLine(plusCenter + QPointF(-4.0, 0.0), plusCenter + QPointF(4.0, 0.0));
    painter->drawLine(plusCenter + QPointF(0.0, -4.0), plusCenter + QPointF(0.0, 4.0));

    painter->setClipping(false);
    painter->setOpacity(isEnabled() ? 0.34 : 0.16);
    painter->setPen(QPen(QColor(255, 255, 255, 120), 0.7, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(QPointF(frame.left() + radius, frame.top() + 2.2),
                      QPointF(frame.right() - radius, frame.top() + 2.2));
    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmSpinBoxVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setActive, bool, m_active, stateChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)
FM_SETTER(setIndicatorColor, const QColor &, m_indicatorColor, colorsChanged)

void FmSpinBoxVisual::setDecreaseActivation(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_decreaseActivation, value))
        return;
    m_decreaseActivation = value;
    emit stateChanged();
    update();
}

void FmSpinBoxVisual::setIncreaseActivation(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_increaseActivation, value))
        return;
    m_increaseActivation = value;
    emit stateChanged();
    update();
}

#undef FM_SETTER
