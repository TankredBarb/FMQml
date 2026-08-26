#include "DiskUsageTreemapLayout.h"

#include <algorithm>
#include <utility>

namespace {
void layoutRange(QList<DiskUsageTreemapCell> &cells, int begin, int end,
                 const QRectF &bounds, qreal totalWeight)
{
    if (begin >= end || bounds.isEmpty() || totalWeight <= 0.0)
        return;
    if (end - begin == 1) {
        cells[begin].rect = bounds;
        return;
    }

    const qreal target = totalWeight / 2.0;
    qreal firstWeight = 0.0;
    int split = begin;
    while (split < end - 1) {
        const qreal candidate = firstWeight + cells[split].weight;
        if (firstWeight > 0.0 && qAbs(target - firstWeight) < qAbs(target - candidate))
            break;
        firstWeight = candidate;
        ++split;
    }
    if (split == begin) {
        firstWeight = cells[begin].weight;
        split = begin + 1;
    }

    const qreal ratio = qBound(0.0, firstWeight / totalWeight, 1.0);
    QRectF first = bounds;
    QRectF second = bounds;
    if (bounds.width() >= bounds.height()) {
        first.setWidth(bounds.width() * ratio);
        second.setLeft(first.right());
    } else {
        first.setHeight(bounds.height() * ratio);
        second.setTop(first.bottom());
    }
    layoutRange(cells, begin, split, first, firstWeight);
    layoutRange(cells, split, end, second, totalWeight - firstWeight);
}
}

QList<DiskUsageTreemapCell> layoutDiskUsageTreemap(const QList<qreal> &weights,
                                                   const QRectF &bounds)
{
    QList<DiskUsageTreemapCell> cells;
    cells.reserve(weights.size());
    for (int index = 0; index < weights.size(); ++index) {
        if (weights[index] > 0.0)
            cells.append({index, weights[index], {}});
    }
    std::stable_sort(cells.begin(), cells.end(), [](const auto &left, const auto &right) {
        return left.weight > right.weight;
    });
    qreal totalWeight = 0.0;
    for (const auto &cell : std::as_const(cells))
        totalWeight += cell.weight;
    layoutRange(cells, 0, cells.size(), bounds, totalWeight);
    return cells;
}

QList<DiskUsageTreemapCell> layoutReadableDiskUsageTreemap(const QList<qreal> &weights,
                                                           const QRectF &bounds,
                                                           qreal minimumWidth,
                                                           qreal minimumHeight)
{
    QList<qreal> visibleWeights = weights;
    QList<DiskUsageTreemapCell> cells;
    for (int pass = 0; pass < weights.size(); ++pass) {
        cells = layoutDiskUsageTreemap(visibleWeights, bounds);
        QList<int> unreadable;
        for (const auto &cell : std::as_const(cells)) {
            if (cell.rect.width() < minimumWidth || cell.rect.height() < minimumHeight)
                unreadable.append(cell.index);
        }
        if (unreadable.isEmpty() || cells.size() == 1)
            break;
        if (unreadable.size() == cells.size()) {
            unreadable.clear();
            for (int index = cells.size() / 2; index < cells.size(); ++index)
                unreadable.append(cells[index].index);
        }
        for (int index : std::as_const(unreadable))
            visibleWeights[index] = 0.0;
    }
    return cells;
}
