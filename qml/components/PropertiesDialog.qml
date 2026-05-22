import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 420
    padding: 0
    height: Math.min(mainLayout.implicitHeight, parent ? parent.height * 0.95 : 640)
    visible: propertiesController.visible
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 150; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.97; duration: 120; easing.type: Easing.InCubic }
    }

    background: Rectangle {
        color: Theme.surface
        radius: 12
        border.color: Theme.border
        border.width: 1
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.glassShadow
            shadowBlur: 20
            shadowVerticalOffset: 8
        }
    }

    component PropertyRow : RowLayout {
        property string label: ""
        property string value: ""
        property bool isLink: false
        property color valueColor: Theme.textPrimary
        property bool emphasizeValue: false
        
        spacing: 12
        Layout.fillWidth: true

        Label {
            text: label
            Layout.preferredWidth: 90
            Layout.alignment: Qt.AlignTop
            color: Theme.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            elide: Text.ElideRight
        }
        
        Label {
            text: value
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            color: isLink ? Theme.accent : valueColor
            font.pixelSize: 12
            font.weight: emphasizeValue ? Font.DemiBold : Font.Normal
            elide: Text.ElideMiddle
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            maximumLineCount: 2
            
            Behavior on color { ColorAnimation { duration: 200 } }
        }
    }

    component SectionCard : Rectangle {
        property string title: ""
        Layout.fillWidth: true
        color: Theme.surfaceHover
        border.color: Theme.border
        border.width: 1
        radius: 8
        implicitHeight: cardLayout.implicitHeight + 20
        
        default property alias content: cardContent.data
        
        ColumnLayout {
            id: cardLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            spacing: 6
            
            Label {
                visible: parent.parent.title !== ""
                text: parent.parent.title
                font.pixelSize: 9
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
                color: Theme.accent
                Layout.bottomMargin: 2
            }
            
            ColumnLayout {
                id: cardContent
                Layout.fillWidth: true
                spacing: 4
            }
        }
    }

    // ── Computed convenience ────────────────────────────────────────────────
    readonly property bool multiMode: propertiesController.selectedCount > 1

    contentItem: ColumnLayout {
        id: mainLayout
        spacing: 0

        // ── Header ────────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "transparent"
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 12

                // ── Single-item icon ──────────────────────────────────────────
                Item {
                    width: 36
                    height: 36
                    visible: !root.multiMode
                    Layout.alignment: Qt.AlignVCenter
                    
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: Theme.surfaceHover
                        border.color: Theme.border
                        border.width: 1
                    }
                    
                    Image {
                        anchors.centerIn: parent
                        source: propertiesController.path !== "" ? "image://icon/" + encodeURIComponent(propertiesController.path) : ""
                        sourceSize: Qt.size(24, 24)
                        smooth: true
                    }
                }

                // ── Multi-item icon stack ─────────────────────────────────────
                Item {
                    width: 36
                    height: 36
                    visible: root.multiMode
                    Layout.alignment: Qt.AlignVCenter

                    // Front card
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1)
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.2)
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: propertiesController.selectedCount
                            font.pixelSize: 13
                            font.weight: Font.Bold
                            color: Theme.accent
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 1
                    Label {
                        text: propertiesController.name
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: propertiesController.typeText
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }
                }

                Button {
                    id: closeBtn
                    flat: true
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: root.close()
                    
                    contentItem: Label {
                        text: "✕"
                        font.pixelSize: 14
                        color: closeBtn.hovered ? Theme.accent : Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 14
                        color: closeBtn.pressed ? Theme.surfaceActive : (closeBtn.hovered ? Theme.surfaceHover : "transparent")
                    }
                }
            }
        }

        Rectangle { 
            Layout.fillWidth: true; 
            height: 1; 
            color: Theme.border; 
            opacity: 0.4
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
                x: 16
                width: scrollView.availableWidth - 32
                spacing: 12
                
                Item { height: 4; Layout.fillWidth: true } // Top padding spacer

                // Overview Card
                SectionCard {
                    title: "OVERVIEW"
                    
                    PropertyRow {
                        label: root.multiMode ? "Parent" : "Location"
                        value: propertiesController.path
                        isLink: !root.multiMode
                    }

                    // Multi-mode breakdown
                    RowLayout {
                        visible: root.multiMode
                        Layout.fillWidth: true
                        spacing: 12

                        Label {
                            text: "Selection"
                            Layout.preferredWidth: 90
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            spacing: 6

                            Rectangle {
                                radius: 4
                                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                                border.width: 1
                                implicitWidth: selLabel.implicitWidth + 10
                                implicitHeight: 18
                                Label {
                                    id: selLabel
                                    anchors.centerIn: parent
                                    text: propertiesController.selectedCount + " items"
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    color: Theme.accent
                                }
                            }

                            Rectangle {
                                visible: propertiesController.folderCount > 0 || propertiesController.fileCount > 0
                                radius: 4
                                color: Theme.surface
                                border.color: Theme.border
                                border.width: 1
                                implicitWidth: contLabel.implicitWidth + 10
                                implicitHeight: 18
                                Label {
                                    id: contLabel
                                    anchors.centerIn: parent
                                    text: {
                                        let parts = []
                                        if (propertiesController.fileCount > 0)
                                            parts.push(propertiesController.fileCount + " files")
                                        if (propertiesController.folderCount > 0)
                                            parts.push(propertiesController.folderCount + " folders")
                                        return parts.join(", ")
                                    }
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                    color: Theme.textSecondary
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        
                        Label {
                            text: "Total Size"
                            Layout.preferredWidth: 90
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        
                        RowLayout {
                            spacing: 8
                            Layout.fillWidth: true
                            
                            Label {
                                text: propertiesController.sizeText
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                            
                            Item {
                                width: 12
                                height: 12
                                visible: propertiesController.isCalculating
                                
                                Canvas {
                                    id: spinnerCanvas
                                    anchors.fill: parent
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();
                                        
                                        var centerX = width / 2;
                                        var centerY = height / 2;
                                        var radius = (width - 3) / 2;
                                        
                                        var gradient = ctx.createConicalGradient(centerX, centerY, 0);
                                        gradient.addColorStop(0.0, Theme.accent);
                                        gradient.addColorStop(0.7, "transparent");
                                        
                                        ctx.beginPath();
                                        ctx.lineWidth = 1.5;
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

                            Label {
                                visible: root.multiMode && propertiesController.isCalculating
                                text: "calculating…"
                                color: Theme.textSecondary
                                font.pixelSize: 10
                            }
                        }
                    }

                    RowLayout {
                        visible: !root.multiMode && propertiesController.isDirectory
                        spacing: 12
                        Layout.fillWidth: true
                        Label {
                            text: "Contents"
                            Layout.preferredWidth: 90
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        
                        RowLayout {
                            spacing: 6
                            
                            Rectangle {
                                radius: 4
                                color: Theme.surface
                                border.color: Theme.border
                                border.width: 1
                                implicitWidth: fileLabel.implicitWidth + 10
                                implicitHeight: 18
                                Label {
                                    id: fileLabel
                                    anchors.centerIn: parent
                                    text: propertiesController.fileCount + " files"
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                }
                            }
                            
                            Rectangle {
                                radius: 4
                                color: Theme.surface
                                border.color: Theme.border
                                border.width: 1
                                implicitWidth: folderLabel.implicitWidth + 10
                                implicitHeight: 18
                                Label {
                                    id: folderLabel
                                    anchors.centerIn: parent
                                    text: propertiesController.folderCount + " folders"
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                }
                            }
                        }
                    }
                }

                // File Details Card
                SectionCard {
                    title: "FILE DETAILS"
                    visible: !root.multiMode && propertiesController.extraProperties.length > 0

                    Repeater {
                        model: propertiesController.extraProperties
                        PropertyRow {
                            label: modelData.label
                            value: modelData.value
                            emphasizeValue: true
                        }
                    }
                }

                // Timestamps Card
                SectionCard {
                    title: "TIMESTAMPS"

                    PropertyRow {
                        label: root.multiMode ? "Oldest created" : "Created"
                        value: propertiesController.created
                    }

                    PropertyRow {
                        label: root.multiMode ? "Latest modified" : "Modified"
                        value: propertiesController.modified
                    }

                    PropertyRow {
                        label: root.multiMode ? "Latest accessed" : "Accessed"
                        value: propertiesController.accessed
                    }
                }

                // Permissions Card
                SectionCard {
                    title: "PERMISSIONS"
                    visible: !root.multiMode

                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true
                        Repeater {
                            model: [
                                { name: "Read", icon: "eye" },
                                { name: "Write", icon: "move" },
                                { name: "Execute", icon: "terminal" }
                            ]
                            
                            Rectangle {
                                radius: 6
                                color: Theme.surface
                                border.color: Theme.border
                                border.width: 1
                                Layout.fillWidth: true
                                implicitHeight: 28
                                
                                RowLayout {
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Image {
                                        source: "../assets/icons/" + modelData.icon + ".svg"
                                        sourceSize: Qt.size(12, 12)
                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            colorization: 1.0
                                            colorizationColor: Theme.textSecondary
                                        }
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

                // Selected Items Card
                SectionCard {
                    title: "SELECTED ITEMS"
                    visible: root.multiMode

                    Repeater {
                        model: propertiesController.selectedPaths

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 6
                            color: Theme.surface
                            border.color: Theme.border
                            border.width: 1
                            implicitHeight: 28

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8

                                Image {
                                    source: "image://icon/" + encodeURIComponent(modelData)
                                    sourceSize: Qt.size(16, 16)
                                    smooth: true
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                }

                                Label {
                                    text: modelData.split(/[/\\]/).pop()
                                    Layout.fillWidth: true
                                    color: Theme.textPrimary
                                    font.pixelSize: 11
                                    elide: Text.ElideMiddle
                                }
                            }
                        }
                    }
                }

                Item { height: 4; Layout.fillWidth: true } // Bottom padding spacer
            }
        }

        // Action Footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "transparent"
            
            Rectangle { 
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Theme.border
                opacity: 0.4
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                
                Item { Layout.fillWidth: true }

                Button {
                    id: okBtn
                    text: "Done"
                    onClicked: root.close()
                    
                    contentItem: Label {
                        text: okBtn.text
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    background: Rectangle {
                        implicitWidth: 100
                        implicitHeight: 34
                        radius: 6
                        color: okBtn.pressed ? Qt.darker(Theme.accent, 1.1) : (okBtn.hovered ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
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
