#include "DiskUsageTreemapItem.h"

#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>

#include <cmath>
#include <utility>

namespace {
QColor mixed(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}
}

DiskUsageTreemapItem::DiskUsageTreemapItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void DiskUsageTreemapItem::setModel(QAbstractItemModel *model)
{
    if (m_model == model)
        return;
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);
    m_model = model;
    if (m_model) {
        const auto refresh = [this] { rebuildLayout(); };
        connect(m_model, &QAbstractItemModel::modelReset, this, refresh);
        connect(m_model, &QAbstractItemModel::rowsInserted, this, refresh);
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, refresh);
        connect(m_model, &QAbstractItemModel::layoutChanged, this, refresh);
        connect(m_model, &QAbstractItemModel::dataChanged, this, refresh);
        connect(m_model, &QObject::destroyed, this, [this] {
            m_model = nullptr;
            rebuildLayout();
            emit modelChanged();
        });
    }
    rebuildLayout();
    emit modelChanged();
}

void DiskUsageTreemapItem::rebuildLayout()
{
    QList<qreal> weights;
    if (m_model) {
        weights.reserve(m_model->rowCount());
        qreal largestBytes = 0.0;
        for (int row = 0; row < m_model->rowCount(); ++row)
            largestBytes = qMax(largestBytes, value(row, "size").toDouble());
        for (int row = 0; row < m_model->rowCount(); ++row) {
            const qreal bytes = qMax<qreal>(0.0, value(row, "size").toDouble());
            const qreal relativeSize = largestBytes > 0.0 ? std::sqrt(bytes / largestBytes) : 0.0;
            weights.append(bytes > 0.0 ? 0.72 + relativeSize * 0.28 : 0.0);
        }
    }
    const qreal minimumWidth = qMax<qreal>(76.0, m_fontPixelSize * 5.5);
    const qreal minimumHeight = qMax<qreal>(44.0, m_fontPixelSize * 3.2);
    m_cells = layoutReadableDiskUsageTreemap(weights,
                                             QRectF(0.0, 0.0, width(), height()),
                                             minimumWidth, minimumHeight);
    m_smallestVisibleBytes = 0.0;
    m_largestVisibleBytes = 0.0;
    for (const auto &cell : std::as_const(m_cells)) {
        const qreal bytes = qMax<qreal>(0.0, value(cell.index, "size").toDouble());
        if (bytes <= 0.0)
            continue;
        if (m_smallestVisibleBytes <= 0.0 || bytes < m_smallestVisibleBytes)
            m_smallestVisibleBytes = bytes;
        m_largestVisibleBytes = qMax(m_largestVisibleBytes, bytes);
    }
    m_hoveredIndex = -1;
    emit hoveredIndexChanged();
    emit displayedCountChanged();
    update();
}

int DiskUsageTreemapItem::role(const QByteArray &name) const
{
    if (!m_model)
        return -1;
    const auto roles = m_model->roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        if (it.value() == name)
            return it.key();
    }
    return -1;
}

QVariant DiskUsageTreemapItem::value(int row, const QByteArray &name) const
{
    const int roleId = role(name);
    return m_model && roleId >= 0 ? m_model->data(m_model->index(row, 0), roleId) : QVariant{};
}

QColor DiskUsageTreemapItem::cellColor(int row, int position, bool directory, bool hovered) const
{
    QColor base = directory ? m_folderColor : m_fileColor;
    if (!m_tilePalette.isEmpty()) {
        const QString path = value(row, "path").toString();
        const qsizetype paletteIndex = qHash(path, 0) % static_cast<quint64>(m_tilePalette.size());
        const QColor paletteColor = m_tilePalette[paletteIndex].value<QColor>();
        if (paletteColor.isValid())
            base = mixed(paletteColor, base, directory ? 0.12 : 0.24);
    }
    const qreal bytes = qMax<qreal>(0.0, value(row, "size").toDouble());
    qreal sizeIntensity = 1.0;
    if (m_smallestVisibleBytes > 0.0 && m_largestVisibleBytes > m_smallestVisibleBytes) {
        const qreal minimumLog = std::log1p(m_smallestVisibleBytes);
        const qreal maximumLog = std::log1p(m_largestVisibleBytes);
        sizeIntensity = qBound(0.0, (std::log1p(bytes) - minimumLog)
                                      / (maximumLog - minimumLog), 1.0);
    }
    if (m_surfaceColor.isValid()) {
        const qreal emphasizedIntensity = std::pow(sizeIntensity, 1.35);
        base = mixed(m_surfaceColor, base, 0.16 + emphasizedIntensity * 0.84);
    }
    const qreal variation = 0.01 + (position % 2) * 0.012;
    base = mixed(base, (position % 2) ? QColor(Qt::white) : m_borderColor, variation);
    return hovered ? mixed(base, QColor(Qt::white), 0.18) : base;
}

void DiskUsageTreemapItem::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    QFont nameFont = painter->font();
    nameFont.setPixelSize(m_fontPixelSize);
    nameFont.setWeight(QFont::DemiBold);
    QFont detailFont = nameFont;
    detailFont.setPixelSize(qMax(9, m_fontPixelSize - 2));
    detailFont.setWeight(QFont::Normal);

    for (int position = 0; position < m_cells.size(); ++position) {
        const auto &cell = m_cells[position];
        const QRectF rect = cell.rect.adjusted(2.0, 2.0, -2.0, -2.0);
        if (rect.width() < 1.0 || rect.height() < 1.0)
            continue;
        const bool directory = value(cell.index, "isDirectory").toBool();
        const bool hovered = cell.index == m_hoveredIndex;
        const QColor base = cellColor(cell.index, position, directory, hovered);
        QLinearGradient fill(rect.topLeft(), rect.bottomLeft());
        fill.setColorAt(0.0, mixed(base, QColor(Qt::white), 0.13));
        fill.setColorAt(1.0, mixed(base, m_borderColor, 0.12));
        painter->setPen(QPen(hovered ? mixed(m_borderColor, QColor(Qt::white), 0.28) : m_borderColor,
                             hovered ? 1.8 : 1.0));
        painter->setBrush(fill);
        painter->drawRoundedRect(rect, 7.0, 7.0);

        const QRectF textRect = rect.adjusted(10.0, 8.0, -10.0, -8.0);
        if (textRect.width() < 52.0 || textRect.height() < 24.0)
            continue;
        painter->setFont(nameFont);
        painter->setPen(m_textColor);
        const QFontMetricsF nameMetrics(nameFont);
        const QString name = nameMetrics.elidedText(value(cell.index, "name").toString(),
                                                     Qt::ElideMiddle, textRect.width());
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop, name);
        if (textRect.height() >= 45.0) {
            painter->setFont(detailFont);
            painter->setPen(m_secondaryTextColor);
            const QString detail = value(cell.index, "sizeDetailText").toString()
                + QStringLiteral("  ·  ") + value(cell.index, "percentOfRootText").toString();
            painter->drawText(textRect.adjusted(0.0, nameMetrics.height() + 3.0, 0.0, 0.0),
                              Qt::AlignLeft | Qt::AlignTop, detail);
        }
    }
}

QVariantMap DiskUsageTreemapItem::entryAt(qreal x, qreal y) const
{
    for (const auto &cell : m_cells) {
        if (!cell.rect.adjusted(2.0, 2.0, -2.0, -2.0).contains(x, y))
            continue;
        const QRectF visibleRect = cell.rect.adjusted(2.0, 2.0, -2.0, -2.0);
        return {{QStringLiteral("index"), cell.index},
                {QStringLiteral("rectX"), visibleRect.x()},
                {QStringLiteral("rectY"), visibleRect.y()},
                {QStringLiteral("rectWidth"), visibleRect.width()},
                {QStringLiteral("rectHeight"), visibleRect.height()},
                {QStringLiteral("path"), value(cell.index, "path")},
                {QStringLiteral("name"), value(cell.index, "name")},
                {QStringLiteral("sizeText"), value(cell.index, "sizeDetailText")},
                {QStringLiteral("percentText"), value(cell.index, "percentOfRootText")},
                {QStringLiteral("isDirectory"), value(cell.index, "isDirectory")},
                {QStringLiteral("fileCount"), value(cell.index, "fileCount")},
                {QStringLiteral("folderCount"), value(cell.index, "folderCount")}};
    }
    return {};
}

void DiskUsageTreemapItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    rebuildLayout();
}

#define FM_COLOR_SETTER(Name, Member) \
    void DiskUsageTreemapItem::Name(const QColor &color) { if (Member == color) return; Member = color; emit colorsChanged(); update(); }
FM_COLOR_SETTER(setFolderColor, m_folderColor)
FM_COLOR_SETTER(setFileColor, m_fileColor)
FM_COLOR_SETTER(setSurfaceColor, m_surfaceColor)
FM_COLOR_SETTER(setBorderColor, m_borderColor)
FM_COLOR_SETTER(setTextColor, m_textColor)
FM_COLOR_SETTER(setSecondaryTextColor, m_secondaryTextColor)
#undef FM_COLOR_SETTER

void DiskUsageTreemapItem::setTilePalette(const QVariantList &palette)
{
    if (m_tilePalette == palette)
        return;
    m_tilePalette = palette;
    emit colorsChanged();
    update();
}

void DiskUsageTreemapItem::setHoveredIndex(int index)
{
    if (m_hoveredIndex == index)
        return;
    m_hoveredIndex = index;
    emit hoveredIndexChanged();
    update();
}

void DiskUsageTreemapItem::setFontPixelSize(int size)
{
    size = qMax(1, size);
    if (m_fontPixelSize == size)
        return;
    m_fontPixelSize = size;
    emit fontPixelSizeChanged();
    rebuildLayout();
}
