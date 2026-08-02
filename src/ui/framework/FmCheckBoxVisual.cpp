#include "FmCheckBoxVisual.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {
QColor checkBoxMixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmCheckBoxVisual::FmCheckBoxVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmCheckBoxVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 0.0 || height() <= 0.0)
        return;

    const qreal boxSize = qMin<qreal>(20.0, qMin(width(), height()));
    const QRectF box((width() - boxSize) / 2.0,
                     (height() - boxSize) / 2.0,
                     boxSize,
                     boxSize);
    const QRectF inner = box.adjusted(1.5, 1.5, -1.5, -1.5);
    const qreal fillProgress = qBound(0.0, m_progress, 1.0);

    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.46);

    QLinearGradient glass(box.topLeft(), box.bottomLeft());
    glass.setColorAt(0.0, checkBoxMixed(m_surfaceColor, QColor(Qt::white), 0.24));
    glass.setColorAt(0.48, m_surfaceColor);
    glass.setColorAt(1.0, checkBoxMixed(m_surfaceColor, m_borderColor, 0.16));
    QColor outline = (m_active || m_hovered || fillProgress > 0.0)
        ? checkBoxMixed(m_borderColor, m_accentColor, m_active ? 0.68 : 0.42)
        : m_borderColor;
    painter->setPen(QPen(outline, m_active ? 1.4 : 0.85));
    painter->setBrush(glass);
    painter->drawRoundedRect(box, 5.0, 5.0);

    if (fillProgress > 0.0) {
        QPainterPath clip;
        clip.addRoundedRect(inner, 3.8, 3.8);
        painter->setClipPath(clip);

        const qreal liquidHeight = inner.height() * fillProgress;
        const QRectF liquid(inner.left(), inner.bottom() - liquidHeight,
                            inner.width(), liquidHeight);
        const QColor liquidColor = checkBoxMixed(m_surfaceColor, m_accentColor, 0.52);
        QLinearGradient liquidGradient(liquid.topLeft(), liquid.bottomLeft());
        liquidGradient.setColorAt(0.0, checkBoxMixed(liquidColor, QColor(Qt::white), 0.25));
        liquidGradient.setColorAt(0.30, liquidColor);
        liquidGradient.setColorAt(1.0, checkBoxMixed(liquidColor, m_borderColor, 0.18));
        painter->setPen(Qt::NoPen);
        painter->setBrush(liquidGradient);
        painter->drawRect(liquid);

        painter->setPen(QPen(QColor(255, 255, 255, 78), 0.75, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(QPointF(inner.left() + 2.0, liquid.top() + 1.0),
                          QPointF(inner.right() - 2.0, liquid.top() + 1.0));
        painter->setClipping(false);
    }

    if (fillProgress > 0.30 && m_checkState != 0) {
        painter->setOpacity((isEnabled() ? 1.0 : 0.46) * qBound(0.0, (fillProgress - 0.30) / 0.70, 1.0));
        QColor shadow = m_accentColor;
        shadow.setAlphaF(0.20);
        QColor markColor = m_markColor;
        markColor.setAlphaF(0.82);
        if (m_checkState == 1) {
            const QPointF left(box.left() + 5.0, box.center().y());
            const QPointF right(box.right() - 5.0, box.center().y());
            painter->setPen(QPen(shadow, 1.7, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(left + QPointF(0.0, 0.3), right + QPointF(0.0, 0.3));
            painter->setPen(QPen(markColor, 1.1, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(left, right);
        } else {
            QPainterPath mark;
            mark.moveTo(box.left() + 4.7, box.center().y() + 0.2);
            mark.lineTo(box.left() + 8.4, box.bottom() - 5.2);
            mark.lineTo(box.right() - 4.1, box.top() + 5.0);
            painter->setPen(QPen(shadow, 1.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(mark.translated(0.0, 0.3));
            painter->setPen(QPen(markColor, 1.15, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(mark);
        }
    }

    painter->setOpacity(isEnabled() ? 0.34 : 0.16);
    painter->setPen(QPen(QColor(255, 255, 255, 120), 0.7, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(QPointF(box.left() + 4.2, box.top() + 3.2),
                      QPointF(box.right() - 4.2, box.top() + 3.2));
    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmCheckBoxVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setCheckState, int, m_checkState, checkStateChanged)
FM_SETTER(setActive, bool, m_active, activeChanged)
FM_SETTER(setHovered, bool, m_hovered, hoveredChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setAccentColor, const QColor &, m_accentColor, colorsChanged)
FM_SETTER(setMarkColor, const QColor &, m_markColor, colorsChanged)

void FmCheckBoxVisual::setProgress(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_progress, value))
        return;
    m_progress = value;
    emit progressChanged();
    update();
}

#undef FM_SETTER
