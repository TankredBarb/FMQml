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
    Q_PROPERTY(bool comet READ comet WRITE setComet NOTIFY cometChanged)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor secondaryAccentColor READ secondaryAccentColor WRITE setSecondaryAccentColor NOTIFY colorsChanged)

public:
    explicit FmProgressRingVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal progress() const { return m_progress; }
    qreal lineWidth() const { return m_lineWidth; }
    bool comet() const { return m_comet; }
    QColor trackColor() const { return m_trackColor; }
    QColor accentColor() const { return m_accentColor; }
    QColor secondaryAccentColor() const { return m_secondaryAccentColor; }

    void setProgress(qreal value);
    void setLineWidth(qreal value);
    void setComet(bool value);
    void setTrackColor(const QColor &value);
    void setAccentColor(const QColor &value);
    void setSecondaryAccentColor(const QColor &value);

signals:
    void progressChanged();
    void lineWidthChanged();
    void cometChanged();
    void colorsChanged();

private:
    qreal m_progress = 0.0;
    qreal m_lineWidth = 2.4;
    bool m_comet = false;
    QColor m_trackColor;
    QColor m_accentColor;
    QColor m_secondaryAccentColor;
};
