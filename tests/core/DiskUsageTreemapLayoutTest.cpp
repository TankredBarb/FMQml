#include "DiskUsageTreemapLayout.h"

#include <QTextStream>

namespace {
int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}
}

int main()
{
    const QRectF bounds(0.0, 0.0, 800.0, 500.0);
    const auto cells = layoutDiskUsageTreemap({50.0, 30.0, 20.0, 0.0}, bounds);
    if (cells.size() != 3)
        return fail("zero-sized entries should not create treemap cells");

    qreal area = 0.0;
    for (const auto &cell : cells) {
        if (!bounds.contains(cell.rect))
            return fail("treemap cell escaped its bounds");
        const qreal cellArea = cell.rect.width() * cell.rect.height();
        const qreal expectedArea = bounds.width() * bounds.height() * cell.weight / 100.0;
        if (qAbs(cellArea - expectedArea) > 0.01)
            return fail("treemap cell area should be proportional to its weight");
        area += cellArea;
    }
    if (qAbs(area - bounds.width() * bounds.height()) > 0.01)
        return fail("treemap cells should cover the complete bounds");

    const auto single = layoutDiskUsageTreemap({42.0}, bounds);
    if (single.size() != 1 || single.first().rect != bounds)
        return fail("a single entry should occupy the complete bounds");

    const auto readable = layoutReadableDiskUsageTreemap(
        {1000.0, 800.0, 600.0, 5.0, 4.0, 3.0, 2.0, 1.0}, bounds, 76.0, 44.0);
    if (readable.size() >= 8)
        return fail("unreadable tail entries should be removed");
    for (const auto &cell : readable) {
        if (cell.rect.width() < 76.0 || cell.rect.height() < 44.0)
            return fail("every retained treemap cell should fit its label");
    }

    const auto equalReadable = layoutReadableDiskUsageTreemap(
        QList<qreal>(32, 1.0), bounds, 76.0, 44.0);
    if (equalReadable.size() <= 1)
        return fail("an unreadable equal-sized set should be reduced gradually");
    for (const auto &cell : equalReadable) {
        if (cell.rect.width() < 76.0 || cell.rect.height() < 44.0)
            return fail("gradually reduced cells should all fit their labels");
    }

    QList<qreal> rankedWeights(64, 0.72);
    rankedWeights[0] = 1.0;
    const auto rankedReadable = layoutReadableDiskUsageTreemap(
        rankedWeights, QRectF(0.0, 0.0, 820.0, 280.0), 76.0, 44.0);
    if (rankedReadable.size() < 16)
        return fail("ranked layout should retain many readable cells in the dialog viewport");
    for (const auto &cell : rankedReadable) {
        if (cell.rect.width() < 76.0 || cell.rect.height() < 44.0)
            return fail("every ranked cell should fit its label");
    }
    return 0;
}
