#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmScrollBarVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal visualPosition READ visualPosition WRITE setVisualPosition NOTIFY visualPositionChanged)
    Q_PROPERTY(qreal visualSize READ visualSize WRITE setVisualSize NOTIFY visualSizeChanged)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(bool flat READ flat WRITE setFlat NOTIFY flatChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool decreaseHovered READ decreaseHovered WRITE setDecreaseHovered NOTIFY decreaseHoveredChanged)
    Q_PROPERTY(bool increaseHovered READ increaseHovered WRITE setIncreaseHovered NOTIFY increaseHoveredChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor idleColor READ idleColor WRITE setIdleColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)

public:
    explicit FmScrollBarVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal visualPosition() const { return m_visualPosition; }
    qreal visualSize() const { return m_visualSize; }
    Qt::Orientation orientation() const { return m_orientation; }
    bool flat() const { return m_flat; }
    bool active() const { return m_active; }
    bool decreaseHovered() const { return m_decreaseHovered; }
    bool increaseHovered() const { return m_increaseHovered; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor idleColor() const { return m_idleColor; }
    QColor accentColor() const { return m_accentColor; }

    void setVisualPosition(qreal value);
    void setVisualSize(qreal value);
    void setOrientation(Qt::Orientation value);
    void setFlat(bool value);
    void setActive(bool value);
    void setDecreaseHovered(bool value);
    void setIncreaseHovered(bool value);
    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setIdleColor(const QColor &value);
    void setAccentColor(const QColor &value);

signals:
    void visualPositionChanged();
    void visualSizeChanged();
    void orientationChanged();
    void flatChanged();
    void activeChanged();
    void decreaseHoveredChanged();
    void increaseHoveredChanged();
    void colorsChanged();

private:
    qreal m_visualPosition = 0.0;
    qreal m_visualSize = 1.0;
    Qt::Orientation m_orientation = Qt::Vertical;
    bool m_flat = false;
    bool m_active = false;
    bool m_decreaseHovered = false;
    bool m_increaseHovered = false;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_idleColor;
    QColor m_accentColor;
};
