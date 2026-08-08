#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmRubberBandVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor strokeColor READ strokeColor WRITE setStrokeColor NOTIFY visualChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY visualChanged)
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY visualChanged)
    Q_PROPERTY(qreal dashLength READ dashLength WRITE setDashLength NOTIFY visualChanged)
    Q_PROPERTY(qreal gapLength READ gapLength WRITE setGapLength NOTIFY visualChanged)

public:
    explicit FmRubberBandVisual(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    QColor strokeColor() const { return m_strokeColor; }
    qreal radius() const { return m_radius; }
    qreal lineWidth() const { return m_lineWidth; }
    qreal dashLength() const { return m_dashLength; }
    qreal gapLength() const { return m_gapLength; }

    void setStrokeColor(const QColor &value);
    void setRadius(qreal value);
    void setLineWidth(qreal value);
    void setDashLength(qreal value);
    void setGapLength(qreal value);

signals:
    void visualChanged();

private:
    QColor m_strokeColor;
    qreal m_radius = 6.0;
    qreal m_lineWidth = 1.25;
    qreal m_dashLength = 5.0;
    qreal m_gapLength = 6.0;
};
