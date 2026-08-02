#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmSwitchVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool hovered READ hovered WRITE setHovered NOTIFY hoveredChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor idleColor READ idleColor WRITE setIdleColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)

public:
    explicit FmSwitchVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal position() const { return m_position; }
    bool active() const { return m_active; }
    bool hovered() const { return m_hovered; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor idleColor() const { return m_idleColor; }
    QColor accentColor() const { return m_accentColor; }

    void setPosition(qreal value);
    void setActive(bool value);
    void setHovered(bool value);
    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setIdleColor(const QColor &value);
    void setAccentColor(const QColor &value);

signals:
    void positionChanged();
    void activeChanged();
    void hoveredChanged();
    void colorsChanged();

private:
    qreal m_position = 0.0;
    bool m_active = false;
    bool m_hovered = false;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_idleColor;
    QColor m_accentColor;
};
