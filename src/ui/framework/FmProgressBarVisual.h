#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmProgressBarVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(bool indeterminate READ indeterminate WRITE setIndeterminate NOTIFY indeterminateChanged)
    Q_PROPERTY(qreal phase READ phase WRITE setPhase NOTIFY phaseChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor idleColor READ idleColor WRITE setIdleColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor liquidColor READ liquidColor WRITE setLiquidColor NOTIFY colorsChanged)

public:
    explicit FmProgressBarVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal progress() const { return m_progress; }
    bool indeterminate() const { return m_indeterminate; }
    qreal phase() const { return m_phase; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor idleColor() const { return m_idleColor; }
    QColor liquidColor() const { return m_liquidColor; }

    void setProgress(qreal value);
    void setIndeterminate(bool value);
    void setPhase(qreal value);
    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setIdleColor(const QColor &value);
    void setLiquidColor(const QColor &value);

signals:
    void progressChanged();
    void indeterminateChanged();
    void phaseChanged();
    void colorsChanged();

private:
    qreal m_progress = 0.0;
    bool m_indeterminate = false;
    qreal m_phase = 0.0;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_idleColor;
    QColor m_liquidColor;
};
