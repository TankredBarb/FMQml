#include "FmProgressBarVisual.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {
QColor progressMixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

FmProgressBarVisual::FmProgressBarVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmProgressBarVisual::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (width() <= 0.0 || height() <= 0.0)
        return;

    const qreal trackHeight = qMin<qreal>(9.0, height());
    const QRectF track(0.5,
                       (height() - trackHeight) / 2.0,
                       qMax(0.0, width() - 1.0),
                       trackHeight);

    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.46);
    painter->setPen(QPen(m_borderColor, 1.0));
    painter->setBrush(m_surfaceColor);
    painter->drawRoundedRect(track, trackHeight / 2.0, trackHeight / 2.0);

    const QRectF innerTrack = track.adjusted(1.0, 1.0, -1.0, -1.0);
    if (innerTrack.width() <= 0.0 || innerTrack.height() <= 0.0) {
        painter->restore();
        return;
    }

    QPainterPath clip;
    clip.addRoundedRect(innerTrack, innerTrack.height() / 2.0, innerTrack.height() / 2.0);
    painter->setClipPath(clip);

    QRectF filled;
    if (m_indeterminate) {
        const qreal segmentWidth = qMax(28.0, innerTrack.width() * 0.34);
        const qreal travel = innerTrack.width() + segmentWidth;
        filled = QRectF(innerTrack.left() - segmentWidth + travel * qBound(0.0, m_phase, 1.0),
                        innerTrack.top(), segmentWidth, innerTrack.height());
    } else {
        filled = QRectF(innerTrack.left(), innerTrack.top(),
                        innerTrack.width() * qBound(0.0, m_progress, 1.0),
                        innerTrack.height());
    }

    if (filled.width() > 0.0) {
        const QColor liquidColor = progressMixed(m_surfaceColor, m_liquidColor, 0.56);
        QLinearGradient liquidGradient(filled.topLeft(), filled.bottomLeft());
        liquidGradient.setColorAt(0.0, progressMixed(liquidColor, QColor(Qt::white), 0.24));
        liquidGradient.setColorAt(0.32, liquidColor);
        liquidGradient.setColorAt(0.72, liquidColor);
        liquidGradient.setColorAt(1.0, progressMixed(liquidColor, m_borderColor, 0.20));

        painter->setPen(QPen(progressMixed(liquidColor, m_borderColor, 0.18), 0.75));
        painter->setBrush(liquidGradient);
        painter->drawRoundedRect(filled, innerTrack.height() / 2.0, innerTrack.height() / 2.0);

        painter->setPen(QPen(QColor(255, 255, 255, 108), 1.0, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(QPointF(filled.left() + 2.0, filled.top() + 1.0),
                          QPointF(filled.right() - 2.0, filled.top() + 1.0));
    }

    painter->setClipping(false);
    painter->restore();
}

#define FM_SETTER(Name, Type, Member, Signal) \
    void FmProgressBarVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

FM_SETTER(setIndeterminate, bool, m_indeterminate, indeterminateChanged)
FM_SETTER(setSurfaceColor, const QColor &, m_surfaceColor, colorsChanged)
FM_SETTER(setBorderColor, const QColor &, m_borderColor, colorsChanged)
FM_SETTER(setIdleColor, const QColor &, m_idleColor, colorsChanged)
FM_SETTER(setLiquidColor, const QColor &, m_liquidColor, colorsChanged)

void FmProgressBarVisual::setProgress(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_progress, value))
        return;
    m_progress = value;
    emit progressChanged();
    update();
}

void FmProgressBarVisual::setPhase(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_phase, value))
        return;
    m_phase = value;
    emit phaseChanged();
    update();
}

#undef FM_SETTER
