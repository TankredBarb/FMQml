#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmIconButtonVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal activation READ activation WRITE setActivation NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY stateChanged)
    Q_PROPERTY(bool focused READ focused WRITE setFocused NOTIFY stateChanged)
    Q_PROPERTY(bool showIdleSurface READ showIdleSurface WRITE setShowIdleSurface NOTIFY stateChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)

public:
    explicit FmIconButtonVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal activation() const { return m_activation; }
    bool active() const { return m_active; }
    bool focused() const { return m_focused; }
    bool showIdleSurface() const { return m_showIdleSurface; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor accentColor() const { return m_accentColor; }

    void setActivation(qreal value);
    void setActive(bool value);
    void setFocused(bool value);
    void setShowIdleSurface(bool value);
    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setAccentColor(const QColor &value);

signals:
    void stateChanged();
    void colorsChanged();

private:
    qreal m_activation = 0.0;
    bool m_active = false;
    bool m_focused = false;
    bool m_showIdleSurface = false;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_accentColor;
};
