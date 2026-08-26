#pragma once

#include "DiskUsageTreemapLayout.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQml>

class DiskUsageTreemapItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QAbstractItemModel *model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int hoveredIndex READ hoveredIndex WRITE setHoveredIndex NOTIFY hoveredIndexChanged)
    Q_PROPERTY(int displayedCount READ displayedCount NOTIFY displayedCountChanged)
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY displayedCountChanged)
    Q_PROPERTY(QColor folderColor READ folderColor WRITE setFolderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor fileColor READ fileColor WRITE setFileColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor secondaryTextColor READ secondaryTextColor WRITE setSecondaryTextColor NOTIFY colorsChanged)
    Q_PROPERTY(QVariantList tilePalette READ tilePalette WRITE setTilePalette NOTIFY colorsChanged)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY fontPixelSizeChanged)

public:
    explicit DiskUsageTreemapItem(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    QAbstractItemModel *model() const { return m_model; }
    int hoveredIndex() const { return m_hoveredIndex; }
    int displayedCount() const { return m_cells.size(); }
    int hiddenCount() const { return m_model ? qMax(0, m_model->rowCount() - m_cells.size()) : 0; }
    QColor folderColor() const { return m_folderColor; }
    QColor fileColor() const { return m_fileColor; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor textColor() const { return m_textColor; }
    QColor secondaryTextColor() const { return m_secondaryTextColor; }
    QVariantList tilePalette() const { return m_tilePalette; }
    int fontPixelSize() const { return m_fontPixelSize; }

    void setModel(QAbstractItemModel *model);
    void setHoveredIndex(int index);
    void setFolderColor(const QColor &color);
    void setFileColor(const QColor &color);
    void setSurfaceColor(const QColor &color);
    void setBorderColor(const QColor &color);
    void setTextColor(const QColor &color);
    void setSecondaryTextColor(const QColor &color);
    void setTilePalette(const QVariantList &palette);
    void setFontPixelSize(int size);

    Q_INVOKABLE QVariantMap entryAt(qreal x, qreal y) const;

signals:
    void modelChanged();
    void hoveredIndexChanged();
    void displayedCountChanged();
    void colorsChanged();
    void fontPixelSizeChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void rebuildLayout();
    int role(const QByteArray &name) const;
    QVariant value(int row, const QByteArray &name) const;
    QColor cellColor(int row, int position, bool directory, bool hovered) const;

    QPointer<QAbstractItemModel> m_model;
    QList<DiskUsageTreemapCell> m_cells;
    int m_hoveredIndex = -1;
    QColor m_folderColor;
    QColor m_fileColor;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_textColor;
    QColor m_secondaryTextColor;
    QVariantList m_tilePalette;
    int m_fontPixelSize = 13;
    qreal m_smallestVisibleBytes = 0.0;
    qreal m_largestVisibleBytes = 0.0;
};
