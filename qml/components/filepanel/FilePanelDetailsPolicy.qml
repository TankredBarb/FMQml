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

        p.preferredColWidthName = 220; p.colWidthSize = 90; p.colWidthType = 130; p.colWidthDate = 150
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

        if (!p.columnsManuallyResized) {
            const columns = root.visibleDetailColumns()
            let preferredOther = 0
            let minOther = 0
            for (let i = 0; i < columns.length; ++i) {
                preferredOther += root.columnPreferredWidth(columns[i])
                minOther += root.columnMinWidth(columns[i])
            }

            const desiredNameWidth = p.nameColumnManuallyResized
                                   ? Math.max(p.colMinWidthName, p.preferredColWidthName)
                                   : p.preferredColWidthName
            const targetOther = Math.max(minOther, Math.min(preferredOther, available - desiredNameWidth))
            const shrinkRange = Math.max(1, preferredOther - minOther)
            const shrinkRatio = preferredOther <= targetOther ? 0 : (preferredOther - targetOther) / shrinkRange

            for (let j = 0; j < columns.length; ++j) {
                const column = columns[j]
                const preferred = root.columnPreferredWidth(column)
                const minimum = root.columnMinWidth(column)
                const width = Math.round(preferred - (preferred - minimum) * shrinkRatio)
                root.setDetailColumnWidth(column, Math.max(minimum, width))
            }
        }

        if (p.nameColumnManuallyResized) {
            p.colWidthName = Math.max(p.colMinWidthName, p.preferredColWidthName)
        } else {
            const space = available - p.totalOtherColumnsWidth
            p.colWidthName = Math.max(p.colMinWidthName, space)
        }
    }
}
