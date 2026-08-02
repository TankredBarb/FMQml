#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmSpinBoxVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal decreaseActivation READ decreaseActivation WRITE setDecreaseActivation NOTIFY stateChanged)
    Q_PROPERTY(qreal increaseActivation READ increaseActivation WRITE setIncreaseActivation NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY stateChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor indicatorColor READ indicatorColor WRITE setIndicatorColor NOTIFY colorsChanged)

public:
    explicit FmSpinBoxVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal decreaseActivation() const { return m_decreaseActivation; }
    qreal increaseActivation() const { return m_increaseActivation; }
    bool active() const { return m_active; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor accentColor() const { return m_accentColor; }
    QColor indicatorColor() const { return m_indicatorColor; }

    void setDecreaseActivation(qreal value);
    void setIncreaseActivation(qreal value);
    void setActive(bool value);
    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setAccentColor(const QColor &value);
    void setIndicatorColor(const QColor &value);

signals:
    void stateChanged();
    void colorsChanged();

private:
    qreal m_decreaseActivation = 0.0;
    qreal m_increaseActivation = 0.0;
    bool m_active = false;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_accentColor;
    QColor m_indicatorColor;
};
