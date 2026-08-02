#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmComboBoxVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal activation READ activation WRITE setActivation NOTIFY activationChanged)
    Q_PROPERTY(qreal arrowPosition READ arrowPosition WRITE setArrowPosition NOTIFY arrowPositionChanged)
    Q_PROPERTY(bool popupSurface READ popupSurface WRITE setPopupSurface NOTIFY popupSurfaceChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor indicatorColor READ indicatorColor WRITE setIndicatorColor NOTIFY colorsChanged)

public:
    explicit FmComboBoxVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal activation() const { return m_activation; }
    qreal arrowPosition() const { return m_arrowPosition; }
    bool popupSurface() const { return m_popupSurface; }
    bool active() const { return m_active; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor accentColor() const { return m_accentColor; }
    QColor indicatorColor() const { return m_indicatorColor; }

    void setActivation(qreal value);
    void setArrowPosition(qreal value);
    void setPopupSurface(bool value);
    void setActive(bool value);
    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setAccentColor(const QColor &value);
    void setIndicatorColor(const QColor &value);

signals:
    void activationChanged();
    void arrowPositionChanged();
    void popupSurfaceChanged();
    void activeChanged();
    void colorsChanged();

private:
    qreal m_activation = 0.0;
    qreal m_arrowPosition = 0.0;
    bool m_popupSurface = false;
    bool m_active = false;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_accentColor;
    QColor m_indicatorColor;
};
