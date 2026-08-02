#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmTabBarVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal highlightX READ highlightX WRITE setHighlightX NOTIFY geometryChanged)
    Q_PROPERTY(qreal highlightWidth READ highlightWidth WRITE setHighlightWidth NOTIFY geometryChanged)
    Q_PROPERTY(qreal hoverX READ hoverX WRITE setHoverX NOTIFY geometryChanged)
    Q_PROPERTY(qreal hoverWidth READ hoverWidth WRITE setHoverWidth NOTIFY geometryChanged)
    Q_PROPERTY(qreal hoverAmount READ hoverAmount WRITE setHoverAmount NOTIFY geometryChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)

public:
    explicit FmTabBarVisual(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    qreal highlightX() const { return m_highlightX; }
    qreal highlightWidth() const { return m_highlightWidth; }
    qreal hoverX() const { return m_hoverX; }
    qreal hoverWidth() const { return m_hoverWidth; }
    qreal hoverAmount() const { return m_hoverAmount; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor accentColor() const { return m_accentColor; }

    void setHighlightX(qreal value);
    void setHighlightWidth(qreal value);
    void setHoverX(qreal value);
    void setHoverWidth(qreal value);
    void setHoverAmount(qreal value);
    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setAccentColor(const QColor &value);

signals:
    void geometryChanged();
    void colorsChanged();

private:
    qreal m_highlightX = 4.0;
    qreal m_highlightWidth = 0.0;
    qreal m_hoverX = 4.0;
    qreal m_hoverWidth = 0.0;
    qreal m_hoverAmount = 0.0;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_accentColor;
};
