#include "DiskUsageSunburstItem.h"

#include "DriveUtils.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>

namespace {
QColor mixedSunburstColor(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}

QPainterPath sectorPath(const QPointF &center, qreal innerRadius, qreal outerRadius,
                        qreal startDegrees, qreal spanDegrees)
{
    const QRectF outer(center.x() - outerRadius, center.y() - outerRadius,
                       outerRadius * 2.0, outerRadius * 2.0);
    const QRectF inner(center.x() - innerRadius, center.y() - innerRadius,
                       innerRadius * 2.0, innerRadius * 2.0);
    QPainterPath path;
    path.arcMoveTo(outer, startDegrees);
    path.arcTo(outer, startDegrees, spanDegrees);
    path.arcTo(inner, startDegrees + spanDegrees, -spanDegrees);
    path.closeSubpath();
    return path;
}
}

DiskUsageSunburstItem::DiskUsageSunburstItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void DiskUsageSunburstItem::rebuildLayout()
{
    QVariantMap hoveredNode;
    int hoveredDepth = 0;
    if (m_hoveredIndex >= 0 && m_hoveredIndex < m_sectors.size()) {
        hoveredNode = m_sectors.at(m_hoveredIndex).node;
        hoveredDepth = m_sectors.at(m_hoveredIndex).depth;
    }
    m_sectors.clear();
    const bool showLegend = width() >= height() * 1.34 && width() - height() >= 210.0;
    const qreal chartWidth = showLegend ? qMin(width() * 0.62, height()) : width();
    const qreal radius = qMax<qreal>(0.0, qMin(chartWidth, height()) / 2.0 - 8.0);
    m_center = showLegend ? QPointF(radius + 8.0, height() / 2.0)
                          : QPointF(width() / 2.0, height() / 2.0);
    m_centerRadius = radius * 0.23;
    m_ringWidth = radius > m_centerRadius ? (radius - m_centerRadius) / 3.0 : 0.0;
    m_legendBounds = showLegend
        ? QRectF(m_center.x() + radius + 24.0, 14.0,
                 qMax<qreal>(0.0, width() - m_center.x() - radius - 38.0), height() - 28.0)
        : QRectF();
    appendChildren(m_rootNode.value(QStringLiteral("children")).toList(), 1, 0, -1, 0.0, 360.0);
    if (!m_legendBounds.isEmpty()) {
        const qreal rowHeight = qMax<qreal>(34.0, m_fontPixelSize * 2.55);
        qreal y = m_legendBounds.top() + 30.0;
        for (Sector &sector : m_sectors) {
            if (sector.depth != 1 || y + rowHeight > m_legendBounds.bottom())
                continue;
            sector.legendRect = QRectF(m_legendBounds.left(), y, m_legendBounds.width(), rowHeight);
            y += rowHeight + 3.0;
        }
    }
    int nextHoveredIndex = -1;
    for (int index = 0; index < m_sectors.size() && !hoveredNode.isEmpty(); ++index) {
        const Sector &sector = m_sectors.at(index);
        const QString hoveredPath = hoveredNode.value(QStringLiteral("path")).toString();
        const bool sameIdentity = !hoveredPath.isEmpty()
            ? sector.node.value(QStringLiteral("path")).toString() == hoveredPath
            : sector.depth == hoveredDepth
                && sector.node.value(QStringLiteral("name")) == hoveredNode.value(QStringLiteral("name"))
                && sector.node.value(QStringLiteral("aggregate")) == hoveredNode.value(QStringLiteral("aggregate"));
        if (sameIdentity) {
            nextHoveredIndex = index;
            break;
        }
    }
    if (m_hoveredIndex != nextHoveredIndex) {
        m_hoveredIndex = nextHoveredIndex;
        emit hoveredIndexChanged();
    }
    update();
}

void DiskUsageSunburstItem::appendChildren(const QVariantList &children, int depth, int paletteIndex,
                                           int parentIndex,
                                           qreal startDegrees, qreal spanDegrees)
{
    if (depth > 3 || children.isEmpty() || m_ringWidth <= 0.0)
        return;
    QList<QVariantMap> visibleChildren;
    visibleChildren.reserve(children.size());
    qreal total = 0.0;
    for (const QVariant &value : children) {
        const QVariantMap child = value.toMap();
        const qreal size = qMax<qreal>(0.0, child.value(QStringLiteral("size")).toDouble());
        if (size <= 0.0)
            continue;
        visibleChildren.append(child);
        total += size;
    }
    if (total <= 0.0)
        return;

    std::sort(visibleChildren.begin(), visibleChildren.end(), [](const QVariantMap &left, const QVariantMap &right) {
        return left.value(QStringLiteral("size")).toLongLong()
            > right.value(QStringLiteral("size")).toLongLong();
    });
    const qreal middleRadius = m_centerRadius + (depth - 0.5) * m_ringWidth;
    const qreal minimumSpan = qRadiansToDegrees(52.0 / qMax<qreal>(1.0, middleRadius));
    QList<QVariantMap> readableChildren;
    for (const QVariantMap &child : std::as_const(visibleChildren)) {
        const qreal childSpan = spanDegrees * child.value(QStringLiteral("size")).toDouble() / total;
        const bool preserveTopLevelFolder = depth == 1
            && child.value(QStringLiteral("isDirectory")).toBool()
            && !child.value(QStringLiteral("aggregate")).toBool();
        if (childSpan >= minimumSpan || preserveTopLevelFolder)
            readableChildren.append(child);
    }
    const qreal minimumTopLevelFolderWeight = depth == 1 ? total * 0.0125 : 0.0;
    const auto visualWeight = [minimumTopLevelFolderWeight](const QVariantMap &child) {
        const qreal size = child.value(QStringLiteral("size")).toDouble();
        const bool topLevelFolder = minimumTopLevelFolderWeight > 0.0
            && child.value(QStringLiteral("isDirectory")).toBool()
            && !child.value(QStringLiteral("aggregate")).toBool();
        return topLevelFolder ? qMax(size, minimumTopLevelFolderWeight) : size;
    };
    qreal displayedTotal = 0.0;
    for (const QVariantMap &child : std::as_const(readableChildren))
        displayedTotal += visualWeight(child);
    if (displayedTotal <= 0.0)
        return;

    qreal cursor = startDegrees;
    for (int index = 0; index < readableChildren.size(); ++index) {
        const QVariantMap node = readableChildren.at(index);
        const qreal size = qMax<qreal>(0.0, visualWeight(node));
        const qreal childSpan = index == readableChildren.size() - 1
            ? startDegrees + spanDegrees - cursor : spanDegrees * size / displayedTotal;
        const int childPaletteIndex = depth == 1 ? index : paletteIndex + index + 2;
        const int sectorIndex = m_sectors.size();
        m_sectors.append({node, {}, parentIndex, depth, childPaletteIndex, cursor, childSpan,
                          m_centerRadius + (depth - 1) * m_ringWidth,
                          m_centerRadius + depth * m_ringWidth});
        appendChildren(node.value(QStringLiteral("children")).toList(), depth + 1,
                       childPaletteIndex, sectorIndex, cursor, childSpan);
        cursor += childSpan;
    }
}

bool DiskUsageSunburstItem::isInHoveredSubtree(int index) const
{
    if (m_hoveredIndex < 0 || m_hoveredIndex >= m_sectors.size())
        return false;
    while (index >= 0 && index < m_sectors.size()) {
        if (index == m_hoveredIndex)
            return true;
        index = m_sectors.at(index).parentIndex;
    }
    return false;
}

QColor DiskUsageSunburstItem::sectorColor(const Sector &sector, bool hovered) const
{
    QColor base = m_borderColor;
    if (!m_ringPalette.isEmpty())
        base = m_ringPalette.at(sector.paletteIndex % m_ringPalette.size()).value<QColor>();
    const qreal parentBytes = sector.parentIndex >= 0 && sector.parentIndex < m_sectors.size()
        ? m_sectors.at(sector.parentIndex).node.value(QStringLiteral("size")).toDouble()
        : m_rootNode.value(QStringLiteral("size")).toDouble();
    const qreal sizeShare = parentBytes > 0.0
        ? qBound(0.0, sector.node.value(QStringLiteral("size")).toDouble() / parentBytes, 1.0)
        : 0.0;
    const qreal intensity = qBound(0.34,
                                   0.42 + std::sqrt(sizeShare) * 0.46
                                       - (sector.depth - 1) * 0.035,
                                   0.88);
    base = mixedSunburstColor(m_surfaceColor, base, intensity);
    return hovered ? mixedSunburstColor(base, QColor(Qt::white), 0.20) : base;
}

void DiskUsageSunburstItem::paint(QPainter *painter)
{
    if (m_rootNode.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    QFont labelFont = painter->font();
    labelFont.setPixelSize(qMax(9, m_fontPixelSize - 1));
    labelFont.setWeight(QFont::DemiBold);

    const bool hasHoveredSector = m_hoveredIndex >= 0 && m_hoveredIndex < m_sectors.size();
    for (int index = 0; index < m_sectors.size(); ++index) {
        const Sector &sector = m_sectors.at(index);
        const bool directlyHovered = index == m_hoveredIndex;
        const bool branchHovered = isInHoveredSubtree(index);
        const QPainterPath path = sectorPath(m_center, sector.innerRadius + 1.5,
                                             sector.outerRadius - 1.5,
                                             sector.startDegrees, sector.spanDegrees);
        QColor fillColor = sectorColor(sector, branchHovered);
        if (hasHoveredSector && !branchHovered)
            fillColor = mixedSunburstColor(m_surfaceColor, fillColor, 0.34);
        painter->setPen(QPen(m_borderColor, directlyHovered ? 2.0 : (branchHovered ? 1.5 : 1.0)));
        painter->setBrush(fillColor);
        painter->drawPath(path);

        const qreal middleRadius = (sector.innerRadius + sector.outerRadius) / 2.0;
        const qreal availableArc = qDegreesToRadians(qAbs(sector.spanDegrees)) * middleRadius;
        if (availableArc < 46.0 || m_ringWidth < 24.0)
            continue;
        const qreal middleDegrees = sector.startDegrees + sector.spanDegrees / 2.0;
        const qreal angle = qDegreesToRadians(-middleDegrees);
        const QPointF position = m_center + QPointF(std::cos(angle), std::sin(angle)) * middleRadius;
        const qreal labelWidth = qMin(availableArc - 10.0, 164.0);
        const qreal labelHeight = qMin<qreal>(m_ringWidth - 8.0, m_fontPixelSize + 12.0);
        qreal rotation = 90.0 - middleDegrees;
        while (rotation <= -180.0)
            rotation += 360.0;
        while (rotation > 180.0)
            rotation -= 360.0;
        if (rotation < -90.0)
            rotation += 180.0;
        else if (rotation > 90.0)
            rotation -= 180.0;

        painter->save();
        painter->setClipPath(path, Qt::IntersectClip);
        painter->translate(position);
        painter->rotate(rotation);
        const QRectF labelRect(-labelWidth / 2.0, -labelHeight / 2.0,
                               labelWidth, labelHeight);
        painter->setFont(labelFont);
        painter->setPen(hasHoveredSector && !branchHovered
                            ? mixedSunburstColor(m_surfaceColor, m_textColor, 0.46)
                            : m_textColor);
        const QString label = QFontMetricsF(labelFont).elidedText(
            sector.node.value(QStringLiteral("name")).toString(), Qt::ElideMiddle,
            qMax<qreal>(0.0, labelRect.width() - 12.0));
        painter->drawText(labelRect.adjusted(6.0, 0.0, -6.0, 0.0), Qt::AlignCenter, label);
        painter->restore();
    }

    painter->setPen(QPen(m_borderColor, 1.0));
    painter->setBrush(m_surfaceColor);
    painter->drawEllipse(m_center, m_centerRadius - 1.5, m_centerRadius - 1.5);
    if (!m_rootNode.isEmpty()) {
        QFont rootFont = labelFont;
        rootFont.setPixelSize(m_fontPixelSize);
        painter->setFont(rootFont);
        painter->setPen(m_textColor);
        const QRectF nameRect(m_center.x() - m_centerRadius + 8.0,
                              m_center.y() - m_fontPixelSize - 3.0,
                              (m_centerRadius - 8.0) * 2.0, m_fontPixelSize + 4.0);
        painter->drawText(nameRect, Qt::AlignCenter, QFontMetricsF(rootFont).elidedText(
                              m_rootNode.value(QStringLiteral("name")).toString(),
                              Qt::ElideMiddle, nameRect.width()));
        rootFont.setPixelSize(qMax(9, m_fontPixelSize - 2));
        rootFont.setWeight(QFont::Normal);
        painter->setFont(rootFont);
        painter->setPen(m_secondaryTextColor);
        painter->drawText(nameRect.translated(0.0, m_fontPixelSize + 2.0), Qt::AlignCenter,
                          DriveUtils::formatSize(m_rootNode.value(QStringLiteral("size")).toLongLong()));
    }

    if (!m_legendBounds.isEmpty()) {
        painter->setPen(QPen(m_borderColor, 1.0));
        painter->drawLine(QPointF(m_legendBounds.left() - 12.0, m_legendBounds.top()),
                          QPointF(m_legendBounds.left() - 12.0, m_legendBounds.bottom()));
        QFont legendTitleFont = labelFont;
        legendTitleFont.setPixelSize(m_fontPixelSize);
        painter->setFont(legendTitleFont);
        painter->setPen(m_secondaryTextColor);
        painter->drawText(QRectF(m_legendBounds.left(), m_legendBounds.top(),
                                 m_legendBounds.width(), 22.0),
                          Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Largest branches"));
        const qreal rootSize = m_rootNode.value(QStringLiteral("size")).toDouble();
        for (int index = 0; index < m_sectors.size(); ++index) {
            const Sector &sector = m_sectors.at(index);
            if (sector.legendRect.isEmpty())
                continue;
            const bool branchHovered = isInHoveredSubtree(index);
            if (branchHovered) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(mixedSunburstColor(m_surfaceColor, sectorColor(sector, false), 0.16));
                painter->drawRoundedRect(sector.legendRect, 6.0, 6.0);
            }
            const QRectF markerRect(sector.legendRect.left() + 4.0,
                                    sector.legendRect.center().y() - 6.0, 12.0, 12.0);
            painter->setPen(QPen(m_borderColor, 1.0));
            painter->setBrush(sectorColor(sector, branchHovered));
            painter->drawEllipse(markerRect);
            const qreal detailWidth = qMin<qreal>(128.0, sector.legendRect.width() * 0.38);
            const QRectF nameRect(sector.legendRect.left() + 24.0, sector.legendRect.top(),
                                  qMax<qreal>(20.0, sector.legendRect.width() - detailWidth - 30.0),
                                  sector.legendRect.height());
            painter->setFont(labelFont);
            painter->setPen(m_textColor);
            painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                              QFontMetricsF(labelFont).elidedText(
                                  sector.node.value(QStringLiteral("name")).toString(),
                                  Qt::ElideMiddle, nameRect.width()));
            QFont detailFont = labelFont;
            detailFont.setPixelSize(qMax(9, m_fontPixelSize - 2));
            detailFont.setWeight(QFont::Normal);
            painter->setFont(detailFont);
            painter->setPen(m_secondaryTextColor);
            const qreal percent = rootSize > 0.0
                ? sector.node.value(QStringLiteral("size")).toDouble() * 100.0 / rootSize : 0.0;
            painter->drawText(QRectF(sector.legendRect.right() - detailWidth,
                                     sector.legendRect.top(), detailWidth - 4.0,
                                     sector.legendRect.height()),
                              Qt::AlignRight | Qt::AlignVCenter,
                              QStringLiteral("%1 · %2%").arg(
                                  DriveUtils::formatSize(sector.node.value(QStringLiteral("size")).toLongLong()),
                                  QString::number(percent, 'f', percent < 10.0 ? 1 : 0)));
        }
    }
}

QVariantMap DiskUsageSunburstItem::entryAt(qreal x, qreal y) const
{
    for (int index = 0; index < m_sectors.size(); ++index) {
        const Sector &sector = m_sectors.at(index);
        if (!sector.legendRect.contains(x, y))
            continue;
        QVariantMap result = sector.node;
        result.insert(QStringLiteral("index"), index);
        result.insert(QStringLiteral("sizeText"), DriveUtils::formatSize(
                          sector.node.value(QStringLiteral("size")).toLongLong()));
        return result;
    }
    const QPointF delta = QPointF(x, y) - m_center;
    const qreal radius = std::hypot(delta.x(), delta.y());
    if (radius < m_centerRadius) {
        QVariantMap result = m_rootNode;
        result.insert(QStringLiteral("aggregate"), true);
        result.insert(QStringLiteral("sizeText"), DriveUtils::formatSize(
                          m_rootNode.value(QStringLiteral("size")).toLongLong()));
        return result;
    }
    qreal angle = qRadiansToDegrees(std::atan2(-delta.y(), delta.x()));
    if (angle < 0.0)
        angle += 360.0;
    for (int index = m_sectors.size() - 1; index >= 0; --index) {
        const Sector &sector = m_sectors.at(index);
        if (radius < sector.innerRadius || radius > sector.outerRadius)
            continue;
        const qreal relative = angle >= sector.startDegrees
            ? angle - sector.startDegrees : angle + 360.0 - sector.startDegrees;
        if (relative > sector.spanDegrees)
            continue;
        QVariantMap result = sector.node;
        result.insert(QStringLiteral("index"), index);
        result.insert(QStringLiteral("sizeText"), DriveUtils::formatSize(
                          sector.node.value(QStringLiteral("size")).toLongLong()));
        return result;
    }
    return {};
}

void DiskUsageSunburstItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    rebuildLayout();
}

void DiskUsageSunburstItem::setRootNode(const QVariantMap &rootNode)
{
    if (m_rootNode == rootNode)
        return;
    m_rootNode = rootNode;
    emit rootNodeChanged();
    rebuildLayout();
}

void DiskUsageSunburstItem::setHoveredIndex(int index)
{
    if (m_hoveredIndex == index)
        return;
    m_hoveredIndex = index;
    emit hoveredIndexChanged();
    update();
}

#define FM_COLOR_SETTER(Name, Member) \
    void DiskUsageSunburstItem::Name(const QColor &color) { if (Member == color) return; Member = color; emit colorsChanged(); update(); }
FM_COLOR_SETTER(setSurfaceColor, m_surfaceColor)
FM_COLOR_SETTER(setBorderColor, m_borderColor)
FM_COLOR_SETTER(setTextColor, m_textColor)
FM_COLOR_SETTER(setSecondaryTextColor, m_secondaryTextColor)
#undef FM_COLOR_SETTER

void DiskUsageSunburstItem::setRingPalette(const QVariantList &palette)
{
    if (m_ringPalette == palette)
        return;
    m_ringPalette = palette;
    emit colorsChanged();
    update();
}

void DiskUsageSunburstItem::setFontPixelSize(int size)
{
    size = qMax(1, size);
    if (m_fontPixelSize == size)
        return;
    m_fontPixelSize = size;
    emit fontPixelSizeChanged();
    update();
}
