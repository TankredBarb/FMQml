import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 460
    padding: 0
    height: Math.min(mainLayout.implicitHeight, parent ? parent.height * 0.9 : 680)
    visible: propertiesController.visible
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 300; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.92; to: 1.0; duration: 400; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 200; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.95; duration: 200; easing.type: Easing.InCubic }
    }

    background: Item {
        Rectangle {
            id: mainBg
            anchors.fill: parent
            color: Theme.glassSurfaceStrong
            radius: 24
            border.color: Theme.glassBorder
            border.width: 1

            // Premium background depth
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: 23
                color: "transparent"
                border.color: Qt.rgba(1, 1, 1, 0.05)
                border.width: 1
            }
        }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.35)
            shadowBlur: 1.2
            shadowVerticalOffset: 8
            shadowHorizontalOffset: 0
        }
    }

    component PropertyRow : RowLayout {
        property string label: ""
        property string value: ""
        property bool isLink: false
        property color valueColor: Theme.textPrimary
        property bool emphasizeValue: false
        
        spacing: 16
        Layout.fillWidth: true

        Label {
            text: label
            Layout.preferredWidth: 110
            color: Theme.textSecondary
            font.pixelSize: 12
            font.weight: Font.Medium
            opacity: 0.7
        }
        
        Label {
            text: value
            Layout.fillWidth: true
            color: valueColor
            font.pixelSize: 13
            font.weight: emphasizeValue ? Font.DemiBold : Font.Normal
            elide: Text.ElideMiddle
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            maximumLineCount: 2
            
            Behavior on color { ColorAnimation { duration: 200 } }
        }
    }

    // ── Computed convenience ────────────────────────────────────────────────
    readonly property bool multiMode: propertiesController.selectedCount > 1

    contentItem: ColumnLayout {
        id: mainLayout
        spacing: 0

        // ── Header ────────────────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 20

                // ── Single-item icon ──────────────────────────────────────────
                Item {
                    width: 64
                    height: 64
                    visible: !root.multiMode
                    
                    Rectangle {
                        anchors.fill: parent
                        radius: 18
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.2)
                        border.width: 1
                    }
                    
                    Image {
                        anchors.centerIn: parent
                        source: propertiesController.path !== "" ? "image://icon/" + propertiesController.path : ""
                        sourceSize: Qt.size(40, 40)
                        smooth: true
                    }
                }

                // ── Multi-item icon stack ─────────────────────────────────────
                Item {
                    width: 64
                    height: 64
                    visible: root.multiMode

                    // Shadow cards behind
                    Repeater {
                        model: Math.max(0, Math.min(propertiesController.selectedCount - 1, 3))
                        Rectangle {
                            x: (3 - index) * 5
                            y: (3 - index) * 4
                            width: 46
                            height: 46
                            radius: 12
                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                           0.06 + index * 0.03)
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                                  0.12 + index * 0.05)
                            border.width: 1
                        }
                    }

                    // Front card
                    Rectangle {
                        x: 0; y: 0
                        width: 48; height: 48
                        radius: 13
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: propertiesController.selectedCount
                            font.pixelSize: 18
                            font.bold: true
                            color: Theme.accent
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: propertiesController.name
                        font.bold: true
                        font.pixelSize: root.multiMode ? 18 : 22
                        font.letterSpacing: -0.5
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: propertiesController.typeText
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: Theme.textSecondary
                        opacity: 0.6
                    }
                }
            }
        }

        Rectangle { 
            Layout.fillWidth: true; 
            height: 1; 
            color: Theme.border; 
            opacity: 0.15 
            Layout.leftMargin: 24
            Layout.rightMargin: 24
        }

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: contentColumn.implicitHeight
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            clip: true

            ColumnLayout {
                id: contentColumn
                x: 24
                width: scrollView.availableWidth - 48
                spacing: 32
                
                Item { height: 1; Layout.fillWidth: true } // Top padding spacer



                // General Section
                ColumnLayout {
                    spacing: 14
                    Layout.fillWidth: true
                    
                    Label {
                        text: "OVERVIEW"
                        font.bold: true
                        font.pixelSize: 11
                        font.letterSpacing: 1.2
                        color: Theme.accent
                        opacity: 0.9
                        Layout.bottomMargin: 4
                    }

                    // ── Single: show full path; Multi: show parent / "Multiple locations" ──
                    PropertyRow {
                        label: root.multiMode ? "Parent" : "Location"
                        value: propertiesController.path
                        isLink: !root.multiMode
                    }

                    // ── Multi: show per-item breakdown ────────────────────────
                    RowLayout {
                        visible: root.multiMode
                        Layout.fillWidth: true
                        spacing: 16

                        Label {
                            text: "Selection"
                            Layout.preferredWidth: 110
                            color: Theme.textSecondary
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            opacity: 0.7
                        }

                        RowLayout {
                            spacing: 8

                            Rectangle {
                                radius: 6
                                color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.05)
                                implicitWidth: selLabel.implicitWidth + 16
                                implicitHeight: 22
                                Label {
                                    id: selLabel
                                    anchors.centerIn: parent
                                    text: propertiesController.selectedCount + " items"
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    color: Theme.accent
                                }
                            }

                            Rectangle {
                                visible: propertiesController.folderCount > 0 || propertiesController.fileCount > 0
                                radius: 6
                                color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.05)
                                implicitWidth: contLabel.implicitWidth + 16
                                implicitHeight: 22
                                Label {
                                    id: contLabel
                                    anchors.centerIn: parent
                                    text: {
                                        let parts = []
                                        if (propertiesController.fileCount > 0)
                                            parts.push(propertiesController.fileCount + " files inside")
                                        if (propertiesController.folderCount > 0)
                                            parts.push(propertiesController.folderCount + " sub-folders")
                                        return parts.join(", ")
                                    }
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    color: Theme.textSecondary
                                }
                            }
                        }
                    }
                    
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        
                        Label {
                            text: "Total Size"
                            Layout.preferredWidth: 110
                            color: Theme.textSecondary
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            opacity: 0.7
                        }
                        
                        RowLayout {
                            spacing: 10
                            Layout.fillWidth: true
                            
                            Label {
                                text: propertiesController.sizeText
                                color: Theme.textPrimary
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            
                            // Animated Spinner (The "Donut")
                            Item {
                                width: 20
                                height: 20
                                visible: propertiesController.isCalculating
                                
                                Canvas {
                                    id: spinnerCanvas
                                    anchors.fill: parent
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();
                                        
                                        var centerX = width / 2;
                                        var centerY = height / 2;
                                        var radius = (width - 4) / 2;
                                        
                                        var gradient = ctx.createConicalGradient(centerX, centerY, 0);
                                        gradient.addColorStop(0.0, Theme.accent);
                                        gradient.addColorStop(0.7, "transparent");
                                        
                                        ctx.beginPath();
                                        ctx.lineWidth = 2.5;
                                        ctx.strokeStyle = gradient;
                                        ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
                                        ctx.stroke();
                                    }
                                    
                                    RotationAnimation on rotation {
                                        from: 0; to: 360; duration: 800; loops: Animation.Infinite
                                        running: spinnerCanvas.visible
                                    }
                                }
                            }

                            // "Calculating…" label when multiple folders pending
                            Label {
                                visible: root.multiMode && propertiesController.isCalculating
                                text: "calculating…"
                                color: Theme.textSecondary
                                font.pixelSize: 11
                                opacity: 0.7
                            }
                        }
                    }

                    // Single-mode: Contents row for directories
                    RowLayout {
                        visible: !root.multiMode && propertiesController.isDirectory
                        spacing: 16
                        Layout.fillWidth: true
                        Label {
                            text: "Contents"
                            Layout.preferredWidth: 110
                            color: Theme.textSecondary
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            opacity: 0.7
                        }
                        
                        RowLayout {
                            spacing: 8
                            
                            Rectangle {
                                radius: 6
                                color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.05)
                                implicitWidth: fileLabel.implicitWidth + 16
                                implicitHeight: 22
                                Label {
                                    id: fileLabel
                                    anchors.centerIn: parent
                                    text: propertiesController.fileCount + " files"
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                }
                            }
                            
                            Rectangle {
                                radius: 6
                                color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.05)
                                implicitWidth: folderLabel.implicitWidth + 16
                                implicitHeight: 22
                                Label {
                                    id: folderLabel
                                    anchors.centerIn: parent
                                    text: propertiesController.folderCount + " folders"
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                }
                            }
                        }
                    }
                }

                // Details Section — only for single mode
                ColumnLayout {
                    visible: !root.multiMode && propertiesController.extraProperties.length > 0
                    spacing: 14
                    Layout.fillWidth: true

                    Label {
                        text: "FILE DETAILS"
                        font.bold: true
                        font.pixelSize: 11
                        font.letterSpacing: 1.2
                        color: Theme.accent
                        opacity: 0.9
                        Layout.bottomMargin: 4
                    }

                    Repeater {
                        model: propertiesController.extraProperties
                        PropertyRow {
                            label: modelData.label
                            value: modelData.value
                            emphasizeValue: true
                        }
                    }
                }

                // Dates Section
                ColumnLayout {
                    spacing: 14
                    Layout.fillWidth: true

                    Label {
                        text: "TIMESTAMPS"
                        font.bold: true
                        font.pixelSize: 11
                        font.letterSpacing: 1.2
                        color: Theme.accent
                        opacity: 0.9
                        Layout.bottomMargin: 4
                    }

                    // Multi-mode shows "earliest" / "latest" labels
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        Label {
                            text: root.multiMode ? "Oldest created" : "Created"
                            Layout.preferredWidth: 110
                            color: Theme.textSecondary
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            opacity: 0.7
                        }
                        Label {
                            text: propertiesController.created
                            Layout.fillWidth: true
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            elide: Text.ElideMiddle
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            maximumLineCount: 2
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        Label {
                            text: root.multiMode ? "Latest modified" : "Modified"
                            Layout.preferredWidth: 110
                            color: Theme.textSecondary
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            opacity: 0.7
                        }
                        Label {
                            text: propertiesController.modified
                            Layout.fillWidth: true
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            elide: Text.ElideMiddle
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            maximumLineCount: 2
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        Label {
                            text: root.multiMode ? "Latest accessed" : "Accessed"
                            Layout.preferredWidth: 110
                            color: Theme.textSecondary
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            opacity: 0.7
                        }
                        Label {
                            text: propertiesController.accessed
                            Layout.fillWidth: true
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            elide: Text.ElideMiddle
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            maximumLineCount: 2
                        }
                    }
                }

                // Permissions Section — only for single mode
                ColumnLayout {
                    visible: !root.multiMode
                    spacing: 14
                    Layout.fillWidth: true

                    Label {
                        text: "PERMISSIONS"
                        font.bold: true
                        font.pixelSize: 11
                        font.letterSpacing: 1.2
                        color: Theme.accent
                        opacity: 0.9
                        Layout.bottomMargin: 4
                    }

                    RowLayout {
                        spacing: 10
                        Repeater {
                            model: [
                                { name: "Read", icon: "eye" },
                                { name: "Write", icon: "move" },
                                { name: "Execute", icon: "terminal" }
                            ]
                            
                            Rectangle {
                                radius: 10
                                color: Theme.surfaceHover
                                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                                border.width: 1
                                implicitWidth: 100
                                implicitHeight: 36
                                
                                RowLayout {
                                    anchors.centerIn: parent
                                    spacing: 8
                                    Image {
                                        source: "../assets/icons/" + modelData.icon + ".svg"
                                        sourceSize: Qt.size(14, 14)
                                        opacity: 0.7
                                    }
                                    Label {
                                        text: modelData.name
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        color: Theme.textPrimary
                                    }
                                }
                            }
                        }
                    }
                }

                // ── Multi-mode: selected paths list ───────────────────────────
                ColumnLayout {
                    visible: root.multiMode
                    spacing: 14
                    Layout.fillWidth: true

                    Label {
                        text: "SELECTED ITEMS"
                        font.bold: true
                        font.pixelSize: 11
                        font.letterSpacing: 1.2
                        color: Theme.accent
                        opacity: 0.9
                        Layout.bottomMargin: 4
                    }

                    Repeater {
                        model: propertiesController.selectedPaths

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 8
                            color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.04)
                            border.color: Theme.border
                            border.width: 1
                            implicitHeight: pathItemRow.implicitHeight + 16

                            RowLayout {
                                id: pathItemRow
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10

                                Image {
                                    source: "image://icon/" + modelData
                                    sourceSize: Qt.size(20, 20)
                                    smooth: true
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                }

                                Label {
                                    text: modelData.split(/[/\\]/).pop()
                                    Layout.fillWidth: true
                                    color: Theme.textPrimary
                                    font.pixelSize: 12
                                    elide: Text.ElideMiddle
                                }
                            }
                        }
                    }
                }

                Item { height: 12; Layout.fillWidth: true } // Bottom padding spacer
            }
        }

        // Action Footer
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            
            Rectangle { 
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Theme.border
                opacity: 0.1
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                anchors.topMargin: 0
                anchors.bottomMargin: 0
                
                Item { Layout.fillWidth: true }

                Button {
                    id: okBtn
                    text: "Done"
                    onClicked: root.close()
                    
                    contentItem: Label {
                        text: okBtn.text
                        color: "white"
                        font.bold: true
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    background: Rectangle {
                        implicitWidth: 120
                        implicitHeight: 40
                        radius: 12
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: okBtn.hovered ? Theme.accent : Qt.lighter(Theme.accent, 1.1) }
                            GradientStop { position: 1.0; color: Theme.accent }
                        }
                        
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: okBtn.hovered
                            shadowColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4)
                            shadowBlur: 0.8
                            shadowVerticalOffset: 2
                        }
                    }
                }
            }
        }
    }

    onClosed: propertiesController.visible = false
    onVisibleChanged: {
        if (!visible && propertiesController.visible) {
            propertiesController.visible = false
        }
    }
}
