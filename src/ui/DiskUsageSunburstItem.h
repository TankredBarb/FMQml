#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QVariantMap>
#include <QtQml>

class DiskUsageSunburstItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantMap rootNode READ rootNode WRITE setRootNode NOTIFY rootNodeChanged)
    Q_PROPERTY(int hoveredIndex READ hoveredIndex WRITE setHoveredIndex NOTIFY hoveredIndexChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor secondaryTextColor READ secondaryTextColor WRITE setSecondaryTextColor NOTIFY colorsChanged)
    Q_PROPERTY(QVariantList ringPalette READ ringPalette WRITE setRingPalette NOTIFY colorsChanged)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY fontPixelSizeChanged)

public:
    explicit DiskUsageSunburstItem(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    QVariantMap rootNode() const { return m_rootNode; }
    int hoveredIndex() const { return m_hoveredIndex; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor textColor() const { return m_textColor; }
    QColor secondaryTextColor() const { return m_secondaryTextColor; }
    QVariantList ringPalette() const { return m_ringPalette; }
    int fontPixelSize() const { return m_fontPixelSize; }

    void setRootNode(const QVariantMap &rootNode);
    void setHoveredIndex(int index);
    void setSurfaceColor(const QColor &color);
    void setBorderColor(const QColor &color);
    void setTextColor(const QColor &color);
    void setSecondaryTextColor(const QColor &color);
    void setRingPalette(const QVariantList &palette);
    void setFontPixelSize(int size);

    Q_INVOKABLE QVariantMap entryAt(qreal x, qreal y) const;

signals:
    void rootNodeChanged();
    void hoveredIndexChanged();
    void colorsChanged();
    void fontPixelSizeChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    struct Sector {
        QVariantMap node;
        QRectF legendRect;
        int parentIndex = -1;
        int depth = 0;
        int paletteIndex = 0;
        qreal startDegrees = 0.0;
        qreal spanDegrees = 0.0;
        qreal innerRadius = 0.0;
        qreal outerRadius = 0.0;
    };

    void rebuildLayout();
    void appendChildren(const QVariantList &children, int depth, int paletteIndex, int parentIndex,
                        qreal startDegrees, qreal spanDegrees);
    bool isInHoveredSubtree(int index) const;
    QColor sectorColor(const Sector &sector, bool hovered) const;

    QVariantMap m_rootNode;
    QList<Sector> m_sectors;
    int m_hoveredIndex = -1;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_textColor;
    QColor m_secondaryTextColor;
    QVariantList m_ringPalette;
    int m_fontPixelSize = 13;
    QPointF m_center;
    qreal m_centerRadius = 0.0;
    qreal m_ringWidth = 0.0;
    QRectF m_legendBounds;
};
