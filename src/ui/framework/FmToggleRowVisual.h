#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmToggleRowVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY stateChanged)
    Q_PROPERTY(bool hovered READ hovered WRITE setHovered NOTIFY stateChanged)
    Q_PROPERTY(bool pressed READ pressed WRITE setPressed NOTIFY stateChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY geometryChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor hoverColor READ hoverColor WRITE setHoverColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)

public:
    explicit FmToggleRowVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    bool checked() const { return m_checked; }
    bool hovered() const { return m_hovered; }
    bool pressed() const { return m_pressed; }
    qreal radius() const { return m_radius; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor hoverColor() const { return m_hoverColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor accentColor() const { return m_accentColor; }

    void setChecked(bool value);
    void setHovered(bool value);
    void setPressed(bool value);
    void setRadius(qreal value);
    void setSurfaceColor(const QColor &value);
    void setHoverColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setAccentColor(const QColor &value);

signals:
    void stateChanged();
    void geometryChanged();
    void colorsChanged();

private:
    bool m_checked = false;
    bool m_hovered = false;
    bool m_pressed = false;
    qreal m_radius = 6.0;
    QColor m_surfaceColor;
    QColor m_hoverColor;
    QColor m_borderColor;
    QColor m_accentColor;
};
