import QtQuick

QtObject {
    id: root

    property var panel: null

    function columnPreferredWidth(column) {
        if (column === "Size") return 90
        if (column === "Type") return 130
        if (column === "Date") return 150
        if (column === "DateCreated") return 150
        if (column === "Extension") return 70
        if (column === "Attributes") return 70
        if (column === "Resolution") return 100
        if (column === "Duration") return 80
        if (column === "Artist") return 140
        if (column === "Album") return 140
        if (column === "Bitrate") return 80
        return 80
    }

    function columnMinWidth(column) {
        if (column === "Size") return 72
        if (column === "Type") return 92
        if (column === "Date") return 118
        if (column === "DateCreated") return 118
        if (column === "Extension") return 54
        if (column === "Attributes") return 56
        if (column === "Resolution") return 76
        if (column === "Duration") return 62
        if (column === "Artist") return 96
        if (column === "Album") return 96
        if (column === "Bitrate") return 62
        return 60
    }

    function configuredColumnWidth(column) {
        const p = root.panel
        if (!p || !p.columnsManuallyResized) {
            return root.columnPreferredWidth(column)
        }

        if (column === "Size") return p.colWidthSize
        if (column === "Type") return p.colWidthType
        if (column === "Date") return p.colWidthDate
        if (column === "DateCreated") return p.colWidthDateCreated
        if (column === "Extension") return p.colWidthExtension
        if (column === "Attributes") return p.colWidthAttributes
        if (column === "Resolution") return p.colWidthResolution
        if (column === "Duration") return p.colWidthDuration
        if (column === "Artist") return p.colWidthArtist
        if (column === "Album") return p.colWidthAlbum
        if (column === "Bitrate") return p.colWidthBitrate
        return root.columnPreferredWidth(column)
    }

    function visibleDetailColumns() {
        if (!root.panel) {
            return []
        }

        const columns = []
        if (root.panel.colShowSize) columns.push("Size")
        if (root.panel.colShowType) columns.push("Type")
        if (root.panel.colShowDate) columns.push("Date")
        if (root.panel.colShowDateCreated) columns.push("DateCreated")
        if (root.panel.colShowExtension) columns.push("Extension")
        if (root.panel.colShowAttributes) columns.push("Attributes")
        if (root.panel.colShowResolution) columns.push("Resolution")
        if (root.panel.colShowDuration) columns.push("Duration")
        if (root.panel.colShowArtist) columns.push("Artist")
        if (root.panel.colShowAlbum) columns.push("Album")
        if (root.panel.colShowBitrate) columns.push("Bitrate")
        return columns
    }

    function fitDetailsColumns(available) {
        const p = root.panel
        const availableWidth = Math.max(0, root.numberValue(available, 0))
        const result = {
            widths: { Name: availableWidth },
            visible: {}
        }

        if (!p) {
            return result
        }

        if (p.columnsManuallyResized) {
            result.widths.Name = Math.max(0, p.colWidthName)
            const manualColumns = root.visibleDetailColumns()
            for (let manualIndex = 0; manualIndex < manualColumns.length; ++manualIndex) {
                const manualColumn = manualColumns[manualIndex]
                result.visible[manualColumn] = true
                result.widths[manualColumn] = Math.max(
                            root.columnMinWidth(manualColumn),
                            root.configuredColumnWidth(manualColumn))
            }
            return result
        }

        const nameMin = Math.min(p.colMinWidthName, availableWidth)
        const nameBase = Math.max(p.colMinWidthName, p.preferredColWidthName)
        const columns = root.visibleDetailColumns()

        function minTotalFor(list) {
            let total = nameMin
            for (let i = 0; i < list.length; ++i) {
                total += root.columnMinWidth(list[i])
            }
            return total
        }

        while (columns.length > 0 && minTotalFor(columns) > availableWidth) {
            columns.pop()
        }

        const ordered = ["Name"].concat(columns)
        const base = ({ Name: nameBase })
        const minimum = ({ Name: nameMin })
        let baseTotal = base.Name
        let minTotal = minimum.Name

        for (let i = 0; i < columns.length; ++i) {
            const column = columns[i]
            const minWidth = root.columnMinWidth(column)
            const baseWidth = Math.max(minWidth, root.configuredColumnWidth(column))
            base[column] = baseWidth
            minimum[column] = minWidth
            baseTotal += baseWidth
            minTotal += minWidth
            result.visible[column] = true
        }

        if (availableWidth <= 0) {
            result.widths.Name = 0
            return result
        }

        if (availableWidth >= baseTotal) {
            const extraPerColumn = (availableWidth - baseTotal) / Math.max(1, ordered.length)
            for (let j = 0; j < ordered.length; ++j) {
                const growColumn = ordered[j]
                result.widths[growColumn] = base[growColumn] + extraPerColumn
            }
        } else {
            const shrinkRange = Math.max(1, baseTotal - minTotal)
            const shrinkRatio = Math.max(0, Math.min(1, (baseTotal - availableWidth) / shrinkRange))
            for (let k = 0; k < ordered.length; ++k) {
                const shrinkColumn = ordered[k]
                result.widths[shrinkColumn] = base[shrinkColumn]
                        - (base[shrinkColumn] - minimum[shrinkColumn]) * shrinkRatio
            }
        }

        let used = 0
        for (let m = 0; m < ordered.length; ++m) {
            used += result.widths[ordered[m]]
        }
        result.widths.Name = Math.max(0, result.widths.Name + availableWidth - used)
        return result
    }

    function setDetailColumnWidth(column, width) {
        if (!root.panel) {
            return
        }

        if (column === "Size") root.panel.colWidthSize = width
        else if (column === "Type") root.panel.colWidthType = width
        else if (column === "Date") root.panel.colWidthDate = width
        else if (column === "DateCreated") root.panel.colWidthDateCreated = width
        else if (column === "Extension") root.panel.colWidthExtension = width
        else if (column === "Attributes") root.panel.colWidthAttributes = width
        else if (column === "Resolution") root.panel.colWidthResolution = width
        else if (column === "Duration") root.panel.colWidthDuration = width
        else if (column === "Artist") root.panel.colWidthArtist = width
        else if (column === "Album") root.panel.colWidthAlbum = width
        else if (column === "Bitrate") root.panel.colWidthBitrate = width
    }

    function boolValue(value, fallback) {
        return value === undefined || value === null ? fallback : !!value
    }

    function numberValue(value, fallback) {
        return value === undefined || value === null || isNaN(Number(value)) ? fallback : Number(value)
    }

    function resetColumnsToDefaults() {
        const p = root.panel
        if (!p) {
            return
        }

        p.preferredColWidthName = 220; p.colWidthName = 220; p.colWidthSize = 90; p.colWidthType = 130; p.colWidthDate = 150
        p.colWidthDateCreated = 150; p.colWidthExtension = 70; p.colWidthAttributes = 70
        p.colWidthResolution = 100; p.colWidthDuration = 80; p.colWidthArtist = 140
        p.colWidthAlbum = 140; p.colWidthBitrate = 80
        p.colShowSize = true; p.colShowType = true; p.colShowDate = true
        p.colShowDateCreated = false; p.colShowExtension = false; p.colShowAttributes = false
        p.colShowResolution = false; p.colShowDuration = false; p.colShowArtist = false
        p.colShowAlbum = false; p.colShowBitrate = false
        p.nameColumnManuallyResized = false
        p.columnsManuallyResized = false
        p.showZebraStriping = true
        p.showGridlines = true
        p.updateNameColumnWidth()
    }

    function detailsVisualState() {
        const p = root.panel
        if (!p) {
            return ({})
        }

        return {
            colShowSize: p.colShowSize,
            colShowType: p.colShowType,
            colShowDate: p.colShowDate,
            colShowDateCreated: p.colShowDateCreated,
            colShowExtension: p.colShowExtension,
            colShowAttributes: p.colShowAttributes,
            colShowResolution: p.colShowResolution,
            colShowDuration: p.colShowDuration,
            colShowArtist: p.colShowArtist,
            colShowAlbum: p.colShowAlbum,
            colShowBitrate: p.colShowBitrate,
            nameColumnManuallyResized: p.nameColumnManuallyResized,
            columnsManuallyResized: p.columnsManuallyResized,
            colWidthName: p.colWidthName,
            preferredColWidthName: p.preferredColWidthName,
            colWidthSize: p.colWidthSize,
            colWidthType: p.colWidthType,
            colWidthDate: p.colWidthDate,
            colWidthDateCreated: p.colWidthDateCreated,
            colWidthExtension: p.colWidthExtension,
            colWidthAttributes: p.colWidthAttributes,
            colWidthResolution: p.colWidthResolution,
            colWidthDuration: p.colWidthDuration,
            colWidthArtist: p.colWidthArtist,
            colWidthAlbum: p.colWidthAlbum,
            colWidthBitrate: p.colWidthBitrate,
            showZebraStriping: p.showZebraStriping,
            showGridlines: p.showGridlines
        }
    }

    function restoreDetailsVisualState(state) {
        const p = root.panel
        if (!p || !state) {
            return
        }

        p.colShowSize = root.boolValue(state.colShowSize, true)
        p.colShowType = root.boolValue(state.colShowType, true)
        p.colShowDate = root.boolValue(state.colShowDate, true)
        p.colShowDateCreated = root.boolValue(state.colShowDateCreated, false)
        p.colShowExtension = root.boolValue(state.colShowExtension, false)
        p.colShowAttributes = root.boolValue(state.colShowAttributes, false)
        p.colShowResolution = root.boolValue(state.colShowResolution, false)
        p.colShowDuration = root.boolValue(state.colShowDuration, false)
        p.colShowArtist = root.boolValue(state.colShowArtist, false)
        p.colShowAlbum = root.boolValue(state.colShowAlbum, false)
        p.colShowBitrate = root.boolValue(state.colShowBitrate, false)
        p.nameColumnManuallyResized = root.boolValue(state.nameColumnManuallyResized, false)
        p.columnsManuallyResized = root.boolValue(state.columnsManuallyResized, false)
        p.preferredColWidthName = root.numberValue(state.preferredColWidthName, p.preferredColWidthName)
        p.colWidthName = root.numberValue(state.colWidthName, p.preferredColWidthName)
        if (p.columnsManuallyResized) {
            p.colWidthSize = root.numberValue(state.colWidthSize, p.colWidthSize)
            p.colWidthType = root.numberValue(state.colWidthType, p.colWidthType)
            p.colWidthDate = root.numberValue(state.colWidthDate, p.colWidthDate)
            p.colWidthDateCreated = root.numberValue(state.colWidthDateCreated, p.colWidthDateCreated)
            p.colWidthExtension = root.numberValue(state.colWidthExtension, p.colWidthExtension)
            p.colWidthAttributes = root.numberValue(state.colWidthAttributes, p.colWidthAttributes)
            p.colWidthResolution = root.numberValue(state.colWidthResolution, p.colWidthResolution)
            p.colWidthDuration = root.numberValue(state.colWidthDuration, p.colWidthDuration)
            p.colWidthArtist = root.numberValue(state.colWidthArtist, p.colWidthArtist)
            p.colWidthAlbum = root.numberValue(state.colWidthAlbum, p.colWidthAlbum)
            p.colWidthBitrate = root.numberValue(state.colWidthBitrate, p.colWidthBitrate)
        }
        p.showZebraStriping = root.boolValue(state.showZebraStriping, true)
        p.showGridlines = root.boolValue(state.showGridlines, true)
        p.updateNameColumnWidth()
    }

    function updateNameColumnWidth(available, force) {
        const p = root.panel
        if (!p) {
            return
        }

        if (force !== true && p.resizeOptimized) {
            p.pendingAutoNameColumnWidthUpdate = true
            return
        }

        p.pendingAutoNameColumnWidthUpdate = false

        p.colWidthName = root.fitDetailsColumns(available).widths.Name
    }
}
