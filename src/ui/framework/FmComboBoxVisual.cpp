#include "FmComboBoxVisual.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {
QColor comboBoxMixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmComboBoxVisual::FmComboBoxVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmComboBoxVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 1.0 || height() <= 1.0)
        return;

    const QRectF frame(0.5, 0.5, width() - 1.0, height() - 1.0);
    const qreal radius = qMin<qreal>(7.0, frame.height() / 2.0);
    const qreal activation = qBound(0.0, m_activation, 1.0);

    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.46);

    QLinearGradient glass(frame.topLeft(), frame.bottomLeft());
    glass.setColorAt(0.0, comboBoxMixed(m_surfaceColor, QColor(Qt::white), m_popupSurface ? 0.13 : 0.22));
    glass.setColorAt(0.48, m_surfaceColor);
    glass.setColorAt(1.0, comboBoxMixed(m_surfaceColor, m_borderColor, 0.16));
    const QColor outline = comboBoxMixed(m_borderColor, m_accentColor,
                                          m_active ? 0.76 : activation * 0.38);
    painter->setPen(QPen(outline, m_active ? 1.5 : 1.0));
    painter->setBrush(glass);
    painter->drawRoundedRect(frame, radius, radius);

    QPainterPath clip;
    clip.addRoundedRect(frame.adjusted(1.0, 1.0, -1.0, -1.0),
                        qMax(0.0, radius - 1.0), qMax(0.0, radius - 1.0));
    painter->setClipPath(clip);

    if (m_popupSurface) {
        QColor accentTop = m_accentColor;
        accentTop.setAlphaF(0.12);
        QColor accentMiddle = m_accentColor;
        accentMiddle.setAlphaF(0.035);
        QColor accentClear = m_accentColor;
        accentClear.setAlphaF(0.0);
        QLinearGradient accentWash(frame.topLeft(), frame.bottomLeft());
        accentWash.setColorAt(0.0, accentTop);
        accentWash.setColorAt(0.34, accentMiddle);
        accentWash.setColorAt(0.78, accentClear);
        painter->setPen(Qt::NoPen);
        painter->setBrush(accentWash);
        painter->drawRect(frame);
    }

    if (!m_popupSurface) {
        const qreal wellWidth = qMin<qreal>(30.0, frame.width() * 0.30);
        const QRectF well(frame.right() - wellWidth, frame.top(), wellWidth, frame.height());
        const QColor liquid = comboBoxMixed(m_surfaceColor, m_accentColor, 0.18 + activation * 0.40);
        QLinearGradient liquidGradient(well.topLeft(), well.bottomLeft());
        liquidGradient.setColorAt(0.0, comboBoxMixed(liquid, QColor(Qt::white), 0.24));
        liquidGradient.setColorAt(0.40, liquid);
        liquidGradient.setColorAt(1.0, comboBoxMixed(liquid, m_borderColor, 0.18));
        painter->setPen(Qt::NoPen);
        painter->setBrush(liquidGradient);
        painter->drawRect(well);

        QColor divider = comboBoxMixed(m_borderColor, m_accentColor, activation * 0.44);
        divider.setAlphaF(0.62);
        painter->setPen(QPen(divider, 0.8));
        painter->drawLine(QPointF(well.left(), frame.top() + 3.0),
                          QPointF(well.left(), frame.bottom() - 3.0));

        const QPointF center(well.center().x(), well.center().y());
        QPainterPath arrow;
        arrow.moveTo(-4.0, -1.6);
        arrow.lineTo(0.0, 2.2);
        arrow.lineTo(4.0, -1.6);
        QTransform transform;
        transform.translate(center.x(), center.y());
        transform.rotate(180.0 * qBound(0.0, m_arrowPosition, 1.0));
        painter->setPen(QPen(m_indicatorColor, 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(transform.map(arrow));
    }

    painter->setClipping(false);
    painter->setOpacity(isEnabled() ? 0.34 : 0.16);
    painter->setPen(QPen(QColor(255, 255, 255, 120), 0.7, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(QPointF(frame.left() + radius, frame.top() + 2.2),
                      QPointF(frame.right() - radius, frame.top() + 2.2));
    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmComboBoxVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setPopupSurface, bool, m_popupSurface, popupSurfaceChanged)
FM_SETTER(setActive, bool, m_active, activeChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)
FM_SETTER(setIndicatorColor, const QColor &, m_indicatorColor, colorsChanged)

void FmComboBoxVisual::setActivation(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_activation, value))
        return;
    m_activation = value;
    emit activationChanged();
    update();
}

void FmComboBoxVisual::setArrowPosition(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_arrowPosition, value))
        return;
    m_arrowPosition = value;
    emit arrowPositionChanged();
    update();
}

#undef FM_SETTER
