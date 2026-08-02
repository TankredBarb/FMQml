#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmTextFieldVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal activation READ activation WRITE setActivation NOTIFY activationChanged)
    Q_PROPERTY(bool focused READ focused WRITE setFocused NOTIFY focusedChanged)
    Q_PROPERTY(bool error READ error WRITE setError NOTIFY errorChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor NOTIFY colorsChanged)

public:
    explicit FmTextFieldVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal activation() const { return m_activation; }
    bool focused() const { return m_focused; }
    bool error() const { return m_error; }
    qreal radius() const { return m_radius; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor accentColor() const { return m_accentColor; }
    QColor errorColor() const { return m_errorColor; }

    void setActivation(qreal value);
    void setFocused(bool value);
    void setError(bool value);
    void setRadius(qreal value);
    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setAccentColor(const QColor &value);
    void setErrorColor(const QColor &value);

signals:
    void activationChanged();
    void focusedChanged();
    void errorChanged();
    void radiusChanged();
    void colorsChanged();

private:
    qreal m_activation = 0.0;
    bool m_focused = false;
    bool m_error = false;
    qreal m_radius = 7.0;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_accentColor;
    QColor m_errorColor;
};
