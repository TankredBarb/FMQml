#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmProgressRingVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)

public:
    explicit FmProgressRingVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal progress() const { return m_progress; }
    qreal lineWidth() const { return m_lineWidth; }
    QColor trackColor() const { return m_trackColor; }
    QColor accentColor() const { return m_accentColor; }

    void setProgress(qreal value);
    void setLineWidth(qreal value);
    void setTrackColor(const QColor &value);
    void setAccentColor(const QColor &value);

signals:
    void progressChanged();
    void lineWidthChanged();
    void colorsChanged();

private:
    qreal m_progress = 0.0;
    qreal m_lineWidth = 2.4;
    QColor m_trackColor;
    QColor m_accentColor;
};
