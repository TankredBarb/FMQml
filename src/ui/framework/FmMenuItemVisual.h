#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmMenuItemVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal activation READ activation WRITE setActivation NOTIFY stateChanged)
    Q_PROPERTY(bool pressed READ pressed WRITE setPressed NOTIFY stateChanged)
    Q_PROPERTY(QColor hoverColor READ hoverColor WRITE setHoverColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor pressedColor READ pressedColor WRITE setPressedColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)

public:
    explicit FmMenuItemVisual(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    qreal activation() const { return m_activation; }
    bool pressed() const { return m_pressed; }
    QColor hoverColor() const { return m_hoverColor; }
    QColor pressedColor() const { return m_pressedColor; }
    QColor accentColor() const { return m_accentColor; }

    void setActivation(qreal value);
    void setPressed(bool value);
    void setHoverColor(const QColor &value);
    void setPressedColor(const QColor &value);
    void setAccentColor(const QColor &value);

signals:
    void stateChanged();
    void colorsChanged();

private:
    qreal m_activation = 0.0;
    bool m_pressed = false;
    QColor m_hoverColor;
    QColor m_pressedColor;
    QColor m_accentColor;
};
