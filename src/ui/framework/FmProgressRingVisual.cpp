#include "FmProgressRingVisual.h"

#include <QConicalGradient>
#include <QPainter>

FmProgressRingVisual::FmProgressRingVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &QQuickItem::enabledChanged, this, &QQuickItem::update);
}

void FmProgressRingVisual::paint(QPainter *painter)
{
    const qreal size = qMin(width(), height());
    if (size <= m_lineWidth)
        return;

    const QRectF ring((width() - size) / 2.0 + m_lineWidth / 2.0,
                      (height() - size) / 2.0 + m_lineWidth / 2.0,
                      size - m_lineWidth,
                      size - m_lineWidth);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->save();
    painter->setOpacity(isEnabled() ? 1.0 : 0.46);
    painter->setBrush(Qt::NoBrush);

    if (m_comet) {
        if (m_trackColor.alpha() > 0) {
            const qreal trackWidth = qMax<qreal>(1.0, m_lineWidth * 0.82);
            painter->setPen(QPen(m_trackColor, trackWidth, Qt::SolidLine, Qt::RoundCap));
            painter->drawEllipse(ring);
        }

        QConicalGradient gradient(ring.center(), 70.0);
        const QColor head = m_accentColor;
        QColor mid = m_secondaryAccentColor.isValid() ? m_secondaryAccentColor : m_accentColor;
        mid.setAlphaF(mid.alphaF() * 0.62);
        QColor tail = mid;
        tail.setAlphaF(m_accentColor.alphaF() * 0.15);
        QColor fade = mid;
        fade.setAlphaF(0.0);

        gradient.setColorAt(0.00, head);
        gradient.setColorAt(0.08, head);
        gradient.setColorAt(0.34, mid);
        gradient.setColorAt(0.58, tail);
        gradient.setColorAt(0.76, fade);
        gradient.setColorAt(0.94, fade);
        gradient.setColorAt(1.00, head);

        painter->setPen(QPen(QBrush(gradient), m_lineWidth, Qt::SolidLine, Qt::RoundCap));
        painter->drawArc(ring, 90 * 16, 260 * 16);
        painter->restore();
        return;
    }

    painter->setPen(QPen(m_trackColor, m_lineWidth, Qt::SolidLine, Qt::RoundCap));
    painter->drawEllipse(ring);

    const qreal clampedProgress = qBound(0.0, m_progress, 1.0);
    if (clampedProgress > 0.0) {
        painter->setPen(QPen(m_accentColor, m_lineWidth, Qt::SolidLine, Qt::RoundCap));
        painter->drawArc(ring, 90 * 16, -qRound(clampedProgress * 360.0 * 16.0));
    }
    painter->restore();
}

void FmProgressRingVisual::setProgress(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_progress, value))
        return;
    m_progress = value;
    emit progressChanged();
    update();
}

void FmProgressRingVisual::setLineWidth(qreal value)
{
    value = qMax(0.0, value);
    if (qFuzzyCompare(m_lineWidth, value))
        return;
    m_lineWidth = value;
    emit lineWidthChanged();
    update();
}

void FmProgressRingVisual::setComet(bool value)
{
    if (m_comet == value)
        return;
    m_comet = value;
    emit cometChanged();
    update();
}

#define FM_COLOR_SETTER(Name, Member) \
    void FmProgressRingVisual::Name(const QColor &value) { if (Member == value) return; Member = value; emit colorsChanged(); update(); }

FM_COLOR_SETTER(setTrackColor, m_trackColor)
FM_COLOR_SETTER(setAccentColor, m_accentColor)
FM_COLOR_SETTER(setSecondaryAccentColor, m_secondaryAccentColor)

#undef FM_COLOR_SETTER
