#pragma once

#include <QList>
#include <QRectF>

struct DiskUsageTreemapCell {
    int index = -1;
    qreal weight = 0.0;
    QRectF rect;
};

QList<DiskUsageTreemapCell> layoutDiskUsageTreemap(const QList<qreal> &weights,
                                                   const QRectF &bounds);
QList<DiskUsageTreemapCell> layoutReadableDiskUsageTreemap(const QList<qreal> &weights,
                                                           const QRectF &bounds,
                                                           qreal minimumWidth,
                                                           qreal minimumHeight);
