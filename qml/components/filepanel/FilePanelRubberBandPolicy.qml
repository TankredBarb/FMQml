import QtQuick

QtObject {
    id: root

    property real listRowHeight: 38

    function contentPoint(view, contentArea, x, y) {
        if (!view || !contentArea) {
            return { x: x, y: y }
        }
        const mapped = contentArea.mapToItem(view, x, y)
        return {
            x: mapped.x + (view.contentX || 0),
            y: mapped.y + (view.contentY || 0)
        }
    }

    function visibleRows(view, count, contentArea, listView) {
        if (count <= 0 || !view) {
            return []
        }

        if (view === listView) {
            let first = view.indexAt(Math.max(1, view.contentX + 1), view.contentY + 1)
            if (first < 0) {
                first = 0
            }
            first = Math.max(0, first - 8)
            const listRows = []
            for (let i = first; i < count; ++i) {
                const item = view.itemAtIndex(i)
                if (!item) {
                    if (listRows.length > 0) {
                        break
                    }
                    continue
                }
                const point = item.mapToItem(contentArea, 0, 0)
                if (point.y > contentArea.height) {
                    break
                }
                if (point.y + item.height >= 0) {
                    listRows.push(i)
                }
            }
            return listRows
        }

        const columns = Math.max(1, Math.floor(view.width / Math.max(1, view.cellWidth)))
        const startRow = Math.max(0, Math.floor(view.contentY / Math.max(1, view.cellHeight)) - 1)
        const endRow = Math.ceil((view.contentY + view.height) / Math.max(1, view.cellHeight)) + 1
        const start = Math.max(0, startRow * columns)
        const end = Math.min(count - 1, ((endRow + 1) * columns) - 1)
        const rows = []
        for (let i = start; i <= end; ++i) {
            rows.push(i)
        }
        return rows
    }

    function selectionRows(view, count, listView, bandTop, bandBottom) {
        if (count <= 0 || !view) {
            return []
        }

        if (view === listView) {
            const rowHeight = Math.max(1, root.listRowHeight)
            const start = Math.max(0, Math.floor(bandTop / rowHeight) - 1)
            const end = Math.min(count - 1, Math.ceil(bandBottom / rowHeight) + 1)
            const listRows = []
            for (let i = start; i <= end; ++i) {
                listRows.push(i)
            }
            return listRows
        }

        const columns = Math.max(1, Math.floor(view.width / Math.max(1, view.cellWidth)))
        const startRow = Math.max(0, Math.floor(bandTop / Math.max(1, view.cellHeight)) - 1)
        const endRow = Math.ceil(bandBottom / Math.max(1, view.cellHeight)) + 1
        const start = Math.max(0, startRow * columns)
        const end = Math.min(count - 1, ((endRow + 1) * columns) - 1)
        const rows = []
        for (let i = start; i <= end; ++i) {
            rows.push(i)
        }
        return rows
    }

    function itemRect(view, row, listView) {
        if (view === listView) {
            const rowHeight = Math.max(1, root.listRowHeight)
            return { x: 0, y: row * rowHeight, width: view.width, height: rowHeight }
        }

        const columns = Math.max(1, Math.floor(view.width / Math.max(1, view.cellWidth)))
        return {
            x: (row % columns) * view.cellWidth,
            y: Math.floor(row / columns) * view.cellHeight,
            width: view.cellWidth,
            height: view.cellHeight
        }
    }

    function selectsItem(viewKind, itemX, itemY, itemWidth, itemHeight,
                         bandLeft, bandTop, bandRight, bandBottom,
                         nameColumnWidth, gridIconSize) {
        let targetX = itemX
        let targetY = itemY
        let targetWidth = itemWidth
        let targetHeight = itemHeight

        if (viewKind === "list") {
            targetX = itemX + 12
            targetY = itemY + 4
            targetWidth = Math.max(0, nameColumnWidth - 20)
            targetHeight = Math.max(0, itemHeight - 8)
        } else if (viewKind === "grid") {
            const visualWidth = Math.min(itemWidth - 18, Math.max(gridIconSize + 34, 72))
            const visualHeight = Math.min(itemHeight - 12, gridIconSize + 54)
            targetX = itemX + Math.max(8, (itemWidth - visualWidth) / 2)
            targetY = itemY + 6
            targetWidth = visualWidth
            targetHeight = visualHeight
        } else if (viewKind === "brief") {
            targetX = itemX + 10
            targetY = itemY + 3
            targetWidth = Math.max(0, itemWidth - 20)
            targetHeight = Math.max(0, itemHeight - 6)
            const overlapLeft = Math.max(targetX, bandLeft)
            const overlapTop = Math.max(targetY, bandTop)
            const overlapRight = Math.min(targetX + targetWidth, bandRight)
            const overlapBottom = Math.min(targetY + targetHeight, bandBottom)
            const overlapWidth = Math.max(0, overlapRight - overlapLeft)
            const overlapHeight = Math.max(0, overlapBottom - overlapTop)
            const overlapArea = overlapWidth * overlapHeight
            const targetArea = targetWidth * targetHeight
            return targetArea > 0 && overlapArea >= Math.min(targetArea * 0.18, 24 * targetHeight)
        } else {
            targetX = itemX + 16
            targetY = itemY + 4
            targetWidth = Math.max(0, itemWidth - 32)
            targetHeight = Math.max(0, itemHeight - 8)
        }

        const centerX = targetX + targetWidth / 2
        const centerY = targetY + targetHeight / 2
        return targetWidth > 0
                && targetHeight > 0
                && centerX >= bandLeft
                && centerX <= bandRight
                && centerY >= bandTop
                && centerY <= bandBottom
    }
}
