#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmMenuSeparatorVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor highlightColor READ highlightColor WRITE setHighlightColor NOTIFY colorsChanged)

public:
    explicit FmMenuSeparatorVisual(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    QColor lineColor() const { return m_lineColor; }
    QColor highlightColor() const { return m_highlightColor; }
    void setLineColor(const QColor &value);
    void setHighlightColor(const QColor &value);

signals:
    void colorsChanged();

private:
    QColor m_lineColor;
    QColor m_highlightColor;
};
