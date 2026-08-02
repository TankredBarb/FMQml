#include "FmTextFieldVisual.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {
QColor textFieldMixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmTextFieldVisual::FmTextFieldVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmTextFieldVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 1.0 || height() <= 1.0)
        return;

    const QRectF frame(0.5, 0.5, width() - 1.0, height() - 1.0);
    const qreal radius = qMin(m_radius, frame.height() / 2.0);
    const qreal activation = qBound(0.0, m_activation, 1.0);
    const QColor stateColor = m_error ? m_errorColor : m_accentColor;

    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.46);

    QLinearGradient glass(frame.topLeft(), frame.bottomLeft());
    glass.setColorAt(0.0, textFieldMixed(m_surfaceColor, QColor(Qt::white), 0.20));
    glass.setColorAt(0.48, m_surfaceColor);
    glass.setColorAt(1.0, textFieldMixed(m_surfaceColor, m_borderColor, 0.15));
    const QColor outline = textFieldMixed(m_borderColor, stateColor,
                                          m_error ? 0.72 : (m_focused ? 0.76 : activation * 0.30));
    painter->setPen(QPen(outline, m_focused || m_error ? 1.5 : 1.0));
    painter->setBrush(glass);
    painter->drawRoundedRect(frame, radius, radius);

    QPainterPath clip;
    clip.addRoundedRect(frame.adjusted(1.0, 1.0, -1.0, -1.0),
                        qMax(0.0, radius - 1.0), qMax(0.0, radius - 1.0));
    painter->setClipPath(clip);

    if (activation > 0.0 || m_error) {
        const qreal liquidAmount = m_error ? qMax<qreal>(0.54, activation) : activation;
        const qreal liquidHeight = m_error
                                       ? 1.0 + 2.8 * liquidAmount
                                       : 0.8 + 2.0 * liquidAmount;
        const QRectF liquid(frame.left(), frame.bottom() - liquidHeight + 1.0,
                            frame.width(), liquidHeight);
        QColor liquidTop = stateColor;
        liquidTop.setAlphaF(m_error
                                ? 0.04 + 0.09 * liquidAmount
                                : 0.02 + 0.05 * liquidAmount);
        QColor liquidBottom = stateColor;
        liquidBottom.setAlphaF(m_error
                                   ? 0.16 + 0.20 * liquidAmount
                                   : 0.09 + 0.14 * liquidAmount);
        QLinearGradient liquidGradient(liquid.topLeft(), liquid.bottomLeft());
        liquidGradient.setColorAt(0.0, liquidTop);
        liquidGradient.setColorAt(1.0, liquidBottom);
        painter->setPen(Qt::NoPen);
        painter->setBrush(liquidGradient);
        painter->drawRect(liquid);
    }

    painter->setClipping(false);
    painter->setOpacity(isEnabled() ? 0.30 : 0.14);
    painter->setPen(QPen(QColor(255, 255, 255, 120), 0.7, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(QPointF(frame.left() + radius, frame.top() + 2.2),
                      QPointF(frame.right() - radius, frame.top() + 2.2));
    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmTextFieldVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setFocused, bool, m_focused, focusedChanged)
FM_SETTER(setError, bool, m_error, errorChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)
FM_SETTER(setErrorColor, const QColor &, m_errorColor, colorsChanged)

void FmTextFieldVisual::setRadius(qreal value)
{
    value = qMax(0.0, value);
    if (qFuzzyCompare(m_radius, value))
        return;
    m_radius = value;
    emit radiusChanged();
    update();
}

void FmTextFieldVisual::setActivation(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_activation, value))
        return;
    m_activation = value;
    emit activationChanged();
    update();
}

#undef FM_SETTER
