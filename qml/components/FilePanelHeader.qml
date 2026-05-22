import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Rectangle {
    id: headerRoot

    required property var controller
    required property var panel

    height: 32
    color: themeController.isDark
        ? Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.5)
        : Qt.rgba(Theme.bg.r,      Theme.bg.g,      Theme.bg.b,      0.65)

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
        opacity: 0.7
    }
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Qt.rgba(1, 1, 1, themeController.isDark ? 0.07 : 0.55)
    }

    // ── Column Picker popup ───────────────────────────────────────────────────
    ColumnPickerMenu {
        id: columnPicker
        panel: headerRoot.panel
        x: Math.min(headerRoot.width - width - 4, Math.max(4, _pickerX))
        y: headerRoot.height + 2
        property real _pickerX: 0
    }

    // Right-click on header = open picker
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        propagateComposedEvents: true
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                columnPicker._pickerX = mapToItem(headerRoot, mouse.x, 0).x
                columnPicker.open()
            }
        }
    }

    // ── Header Layout ────────────────────────────────────────────────────────
    // We use an Item container and manually calculate X positions for each column.
    // To prevent the entire table from shifting infinitely to the right, we use
    // compensated resizing: dragging a boundary right expands the left column and
    // shrinks the right column.

    function applyResize(colNameStr, dx) {
        const p = headerRoot.panel
        const minWidth = 60

        if (colNameStr === "Name") {
            p.preferredColWidthName = Math.max(180, p.colWidthName + dx)
            p.nameColumnManuallyResized = true
        }
        else if (colNameStr === "Size") p.colWidthSize = Math.max(minWidth, p.colWidthSize + dx)
        else if (colNameStr === "Type") p.colWidthType = Math.max(minWidth, p.colWidthType + dx)
        else if (colNameStr === "Date") p.colWidthDate = Math.max(minWidth, p.colWidthDate + dx)
        else if (colNameStr === "DateCreated") p.colWidthDateCreated = Math.max(minWidth, p.colWidthDateCreated + dx)
        else if (colNameStr === "Extension") p.colWidthExtension = Math.max(minWidth, p.colWidthExtension + dx)
        else if (colNameStr === "Attributes") p.colWidthAttributes = Math.max(minWidth, p.colWidthAttributes + dx)
        else if (colNameStr === "Resolution") p.colWidthResolution = Math.max(minWidth, p.colWidthResolution + dx)
        else if (colNameStr === "Duration") p.colWidthDuration = Math.max(minWidth, p.colWidthDuration + dx)
        else if (colNameStr === "Artist") p.colWidthArtist = Math.max(minWidth, p.colWidthArtist + dx)
        else if (colNameStr === "Album") p.colWidthAlbum = Math.max(minWidth, p.colWidthAlbum + dx)
        else if (colNameStr === "Bitrate") p.colWidthBitrate = Math.max(minWidth, p.colWidthBitrate + dx)
    }

    Item {
        id: headerContainer
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        clip: true

        // ── Sticky Name Column Background ────────────────────────────────────
        Rectangle {
            id: stickyHeaderBg
            x: headerRoot.panel.horizontalScrollActive && headerRoot.panel.horizontalScrollX > 12 ? headerRoot.panel.horizontalScrollX - 12 : 0
            width: headerRoot.panel.colWidthName + 12
            height: parent.height
            z: 2
            visible: headerRoot.panel.horizontalScrollActive && headerRoot.panel.horizontalScrollX > 12
            color: themeController.isDark ? Theme.surface : Theme.bg

            // Vertical divider on the right edge
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.border
                opacity: 0.6
            }

            // Drop shadow-like gradient on the right of the sticky column
            Rectangle {
                anchors.left: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 4
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: themeController.isDark ? Qt.rgba(0,0,0,0.25) : Qt.rgba(0,0,0,0.08) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            // Mouse shield to block clicks from hitting headers scrolled underneath
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                propagateComposedEvents: false
            }
        }

        // ── Name ─────────────────────────────────────────────────────────────
        HeaderCol {
            id: colName
            x: headerRoot.panel.horizontalScrollActive && headerRoot.panel.horizontalScrollX > 12 ? headerRoot.panel.horizontalScrollX - 12 : 0
            colWidth: headerRoot.panel.colWidthName
            resizable: true
            z: 3
            active: headerRoot.controller.directoryModel.sortRole === 0
            sortOrder: headerRoot.controller.directoryModel.sortOrder
            label: "Name"
            onHeaderClicked: setSort(0)
            onResized: (dx) => applyResize("Name", dx)
        }

        // ── Size ─────────────────────────────────────────────────────────────
        HeaderCol {
            id: colSize
            x: headerRoot.panel.colWidthName
            colWidth: headerRoot.panel.colWidthSize
            visible: headerRoot.panel.colShowSize
            resizable: true
            active: headerRoot.controller.directoryModel.sortRole === 1
            sortOrder: headerRoot.controller.directoryModel.sortOrder
            label: "Size"
            alignCenter: true
            onHeaderClicked: setSort(1)
            onResized: (dx) => applyResize("Size", dx)
        }

        // ── Type ─────────────────────────────────────────────────────────────
        HeaderCol {
            id: colType
            x: colSize.x + (colSize.visible ? colSize.width : 0)
            colWidth: headerRoot.panel.colWidthType
            visible: headerRoot.panel.colShowType
            resizable: true
            active: headerRoot.controller.directoryModel.sortRole === 2
            sortOrder: headerRoot.controller.directoryModel.sortOrder
            label: "Type"
            alignCenter: true
            onHeaderClicked: setSort(2)
            onResized: (dx) => applyResize("Type", dx)
        }

        // ── Date Modified ─────────────────────────────────────────────────────
        HeaderCol {
            id: colDate
            x: colType.x + (colType.visible ? colType.width : 0)
            colWidth: headerRoot.panel.colWidthDate
            visible: headerRoot.panel.colShowDate
            resizable: true
            active: headerRoot.controller.directoryModel.sortRole === 3
            sortOrder: headerRoot.controller.directoryModel.sortOrder
            label: "Date Modified"
            alignCenter: true
            onHeaderClicked: setSort(3)
            onResized: (dx) => applyResize("Date", dx)
        }

        // ── Date Created ──────────────────────────────────────────────────────
        HeaderCol {
            id: colDateCreated
            x: colDate.x + (colDate.visible ? colDate.width : 0)
            colWidth: headerRoot.panel.colWidthDateCreated
            visible: headerRoot.panel.colShowDateCreated
            resizable: true
            active: headerRoot.controller.directoryModel.sortRole === 4
            sortOrder: headerRoot.controller.directoryModel.sortOrder
            label: "Date Created"
            alignCenter: true
            onHeaderClicked: setSort(4)
            onResized: (dx) => applyResize("DateCreated", dx)
        }

        // ── Extension ─────────────────────────────────────────────────────────
        HeaderCol {
            id: colExtension
            x: colDateCreated.x + (colDateCreated.visible ? colDateCreated.width : 0)
            colWidth: headerRoot.panel.colWidthExtension
            visible: headerRoot.panel.colShowExtension
            resizable: true
            active: headerRoot.controller.directoryModel.sortRole === 5
            sortOrder: headerRoot.controller.directoryModel.sortOrder
            label: "Ext"
            alignCenter: true
            onHeaderClicked: setSort(5)
            onResized: (dx) => applyResize("Extension", dx)
        }

        // ── Attributes ────────────────────────────────────────────────────────
        HeaderCol {
            id: colAttributes
            x: colExtension.x + (colExtension.visible ? colExtension.width : 0)
            colWidth: headerRoot.panel.colWidthAttributes
            visible: headerRoot.panel.colShowAttributes
            resizable: true
            active: false
            label: "Attrs"
            alignCenter: true
            onResized: (dx) => applyResize("Attributes", dx)
        }

        // ── Resolution ────────────────────────────────────────────────────────
        HeaderCol {
            id: colResolution
            x: colAttributes.x + (colAttributes.visible ? colAttributes.width : 0)
            colWidth: headerRoot.panel.colWidthResolution
            visible: headerRoot.panel.colShowResolution
            resizable: true
            active: false
            label: "Resolution"
            alignCenter: true
            onResized: (dx) => applyResize("Resolution", dx)
        }

        // ── Duration ──────────────────────────────────────────────────────────
        HeaderCol {
            id: colDuration
            x: colResolution.x + (colResolution.visible ? colResolution.width : 0)
            colWidth: headerRoot.panel.colWidthDuration
            visible: headerRoot.panel.colShowDuration
            resizable: true
            active: false
            label: "Duration"
            alignCenter: true
            onResized: (dx) => applyResize("Duration", dx)
        }

        // ── Artist ────────────────────────────────────────────────────────────
        HeaderCol {
            id: colArtist
            x: colDuration.x + (colDuration.visible ? colDuration.width : 0)
            colWidth: headerRoot.panel.colWidthArtist
            visible: headerRoot.panel.colShowArtist
            resizable: true
            active: false
            label: "Artist"
            alignCenter: true
            onResized: (dx) => applyResize("Artist", dx)
        }

        // ── Album ─────────────────────────────────────────────────────────────
        HeaderCol {
            id: colAlbum
            x: colArtist.x + (colArtist.visible ? colArtist.width : 0)
            colWidth: headerRoot.panel.colWidthAlbum
            visible: headerRoot.panel.colShowAlbum
            resizable: true
            active: false
            label: "Album"
            alignCenter: true
            onResized: (dx) => applyResize("Album", dx)
        }

        // ── Bitrate ───────────────────────────────────────────────────────────
        HeaderCol {
            id: colBitrate
            x: colAlbum.x + (colAlbum.visible ? colAlbum.width : 0)
            colWidth: headerRoot.panel.colWidthBitrate
            visible: headerRoot.panel.colShowBitrate
            resizable: true
            active: false
            label: "Bitrate"
            alignCenter: true
            onResized: (dx) => applyResize("Bitrate", dx)
        }
    }

    // ── Sort helper ───────────────────────────────────────────────────────────
    function setSort(role) {
        const model = headerRoot.controller.directoryModel
        if (model.sortRole === role) {
            model.sortOrder = (model.sortOrder === Qt.AscendingOrder
                               ? Qt.DescendingOrder : Qt.AscendingOrder)
        } else {
            model.sortRole  = role
            model.sortOrder = Qt.AscendingOrder
        }
    }

    // ── HeaderCol component ───────────────────────────────────────────────────
    // Width = colWidth (same as delegate). Resize handle is INSIDE, at right edge.
    component HeaderCol : Item {
        id: hcol

        property real colWidth: 100
        property bool resizable: true
        property bool active: false
        property int  sortOrder: Qt.AscendingOrder
        property string label: ""
        property bool alignRight: false
        property bool alignCenter: false

        signal headerClicked()
        signal resized(real dx)   // incremental delta-x

        width:  hcol.colWidth
        height: headerRoot.height
        clip:   true

        // ── Click / hover background ──────────────────────────────────────
        Rectangle {
            anchors.fill: parent
            anchors.rightMargin: hcol.resizable ? 8 : 0
            color: clickMa.pressed
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                : (clickMa.containsMouse
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.07)
                    : "transparent")
            Behavior on color { ColorAnimation { duration: 80 } }
        }

        MouseArea {
            id: clickMa
            anchors.fill: parent
            anchors.rightMargin: hcol.resizable ? 8 : 0
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton) {
                    columnPicker._pickerX = mapToItem(headerRoot, mouse.x, 0).x
                    columnPicker.open()
                } else {
                    hcol.headerClicked()
                }
            }
        }

        // ── Label + sort arrow ────────────────────────────────────────────
        RowLayout {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left:  parent.left
            anchors.right: parent.right
            anchors.leftMargin:  hcol.alignCenter ? 4 : (hcol.alignRight ? 4  : 10)
            anchors.rightMargin: hcol.alignCenter ? 4 : (hcol.alignRight ? (hcol.resizable ? 16 : 10) : (hcol.resizable ? 12 : 4))
            spacing: 4
            layoutDirection: hcol.alignRight ? Qt.RightToLeft : Qt.LeftToRight

            Text {
                text: hcol.label
                color: hcol.active ? Theme.accent : Theme.textSecondary
                font.pixelSize: 11
                font.weight: hcol.active ? 600 : 400
                Layout.fillWidth: true
                horizontalAlignment: hcol.alignCenter ? Text.AlignHCenter : (hcol.alignRight ? Text.AlignRight : Text.AlignLeft)
                elide: Text.ElideRight
                Behavior on color { ColorAnimation { duration: 100 } }
            }

            Canvas {
                id: sortChevron
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                opacity: hcol.active ? 1.0 : 0.0
                
                Behavior on opacity { NumberAnimation { duration: 150 } }
                
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = Theme.accent;
                    ctx.lineWidth = 1.5;
                    ctx.lineCap = "round";
                    ctx.lineJoin = "round";
                    ctx.beginPath();
                    if (hcol.sortOrder === Qt.AscendingOrder) {
                        ctx.moveTo(1, 5);
                        ctx.lineTo(4, 2);
                        ctx.lineTo(7, 5);
                    } else {
                        ctx.moveTo(1, 3);
                        ctx.lineTo(4, 6);
                        ctx.lineTo(7, 3);
                    }
                    ctx.stroke();
                }
                
                Connections {
                    target: hcol
                    function onSortOrderChanged() { sortChevron.requestPaint(); }
                    function onActiveChanged() { sortChevron.requestPaint(); }
                }
            }
        }

        // Active column underline
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: hcol.resizable ? 8 : 0
            height: 2
            radius: 1
            color: Theme.accent
            visible: hcol.active
            opacity: 0.8
        }

        // ── Resize handle — last 8px, inside the column ───────────────────
        Item {
            id: resizerItem
            anchors.right: parent.right
            width: 8
            height: parent.height
            visible: hcol.resizable

            HoverHandler {
                id: resizerHover
                cursorShape: Qt.SizeHorCursor
            }

            DragHandler {
                id: dragHandler
                target: null // Do not move the item itself
                xAxis.enabled: true
                yAxis.enabled: false
                
                property real _lastTranslation: 0
                
                onActiveChanged: {
                    if (active) {
                        _lastTranslation = 0
                    }
                }
                
                onTranslationChanged: {
                    if (active) {
                        let dx = translation.x - _lastTranslation
                        if (dx !== 0) {
                            hcol.resized(dx)
                            _lastTranslation = translation.x
                        }
                    }
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 1
                height: parent.height - 8
                radius: 0.5
                color: resizerHover.hovered || dragHandler.active
                       ? Theme.accent : Theme.border
                opacity: resizerHover.hovered || dragHandler.active ? 0.85 : (headerRoot.panel.showGridlines ? 0.55 : 0.35)
                Behavior on color   { ColorAnimation { duration: 100 } }
                Behavior on opacity { NumberAnimation { duration: 100 } }
            }
        }
    }
}
