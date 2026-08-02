#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmButtonVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal activation READ activation WRITE setActivation NOTIFY stateChanged)
    Q_PROPERTY(bool primary READ primary WRITE setPrimary NOTIFY stateChanged)
    Q_PROPERTY(bool destructive READ destructive WRITE setDestructive NOTIFY stateChanged)
    Q_PROPERTY(bool flat READ flat WRITE setFlat NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY stateChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)

public:
    explicit FmButtonVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal activation() const { return m_activation; }
    bool primary() const { return m_primary; }
    bool destructive() const { return m_destructive; }
    bool flat() const { return m_flat; }
    bool active() const { return m_active; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor accentColor() const { return m_accentColor; }

    void setActivation(qreal value);
    void setPrimary(bool value);
    void setDestructive(bool value);
    void setFlat(bool value);
    void setActive(bool value);
    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setAccentColor(const QColor &value);

signals:
    void stateChanged();
    void colorsChanged();

private:
    qreal m_activation = 0.0;
    bool m_primary = false;
    bool m_destructive = false;
    bool m_flat = false;
    bool m_active = false;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_accentColor;
};
