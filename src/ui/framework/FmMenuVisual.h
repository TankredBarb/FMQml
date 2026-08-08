#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmMenuVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor NOTIFY colorsChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY stateChanged)

public:
    explicit FmMenuVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor accentColor() const { return m_accentColor; }
    QColor shadowColor() const { return m_shadowColor; }
    bool dark() const { return m_dark; }

    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setAccentColor(const QColor &value);
    void setShadowColor(const QColor &value);
    void setDark(bool value);

signals:
    void colorsChanged();
    void stateChanged();

private:
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_accentColor;
    QColor m_shadowColor;
    bool m_dark = false;
};
