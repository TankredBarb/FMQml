import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 440
    padding: 0
    height: Math.min(mainLayout.implicitHeight, parent ? parent.height * 0.95 : 680)
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
            radius: 20
            border.color: Theme.glassBorder
            border.width: 1

            // Premium background depth
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: 19
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
        
        spacing: 12
        Layout.fillWidth: true

        Label {
            text: label
            Layout.preferredWidth: 95
            Layout.alignment: Qt.AlignTop
            color: Theme.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            opacity: 0.6
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
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.02)
        border.color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.1)
        border.width: 1
        radius: 12
        implicitHeight: cardLayout.implicitHeight + 24
        
        default property alias content: cardContent.data
        
        ColumnLayout {
            id: cardLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 8
            
            Label {
                visible: parent.parent.title !== ""
                text: parent.parent.title
                font.bold: true
                font.pixelSize: 10
                font.letterSpacing: 1.0
                color: Theme.accent
                opacity: 0.8
                Layout.bottomMargin: 2
            }
            
            ColumnLayout {
                id: cardContent
                Layout.fillWidth: true
                spacing: 6
            }
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
            Layout.preferredHeight: 80
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                spacing: 16

                // ── Single-item icon ──────────────────────────────────────────
                Item {
                    width: 48
                    height: 48
                    visible: !root.multiMode
                    
                    Rectangle {
                        anchors.fill: parent
                        radius: 12
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                        border.width: 1
                    }
                    
                    Image {
                        anchors.centerIn: parent
                        source: propertiesController.path !== "" ? "image://icon/" + propertiesController.path : ""
                        sourceSize: Qt.size(32, 32)
                        smooth: true
                    }
                }

                // ── Multi-item icon stack ─────────────────────────────────────
                Item {
                    width: 48
                    height: 48
                    visible: root.multiMode

                    // Shadow cards behind
                    Repeater {
                        model: Math.max(0, Math.min(propertiesController.selectedCount - 1, 3))
                        Rectangle {
                            x: (3 - index) * 4
                            y: (3 - index) * 3
                            width: 36
                            height: 36
                            radius: 9
                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                           0.05 + index * 0.02)
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                                  0.10 + index * 0.03)
                            border.width: 1
                        }
                    }

                    // Front card
                    Rectangle {
                        x: 0; y: 0
                        width: 38; height: 38
                        radius: 10
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.25)
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: propertiesController.selectedCount
                            font.pixelSize: 14
                            font.bold: true
                            color: Theme.accent
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label {
                        text: propertiesController.name
                        font.bold: true
                        font.pixelSize: root.multiMode ? 16 : 18
                        font.letterSpacing: -0.3
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: propertiesController.typeText
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: Theme.textSecondary
                        opacity: 0.5
                    }
                }
            }
        }

        Rectangle { 
            Layout.fillWidth: true; 
            height: 1; 
            color: Theme.border; 
            opacity: 0.1
            Layout.leftMargin: 20
            Layout.rightMargin: 20
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
                            Layout.preferredWidth: 95
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            opacity: 0.6
                        }

                        RowLayout {
                            spacing: 6

                            Rectangle {
                                radius: 6
                                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                                border.width: 1
                                implicitWidth: selLabel.implicitWidth + 12
                                implicitHeight: 20
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
                                radius: 6
                                color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.04)
                                border.color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.1)
                                border.width: 1
                                implicitWidth: contLabel.implicitWidth + 12
                                implicitHeight: 20
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
                            Layout.preferredWidth: 95
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            opacity: 0.6
                        }
                        
                        RowLayout {
                            spacing: 8
                            Layout.fillWidth: true
                            
                            Label {
                                text: propertiesController.sizeText
                                color: Theme.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            
                            Item {
                                width: 14
                                height: 14
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
                                        ctx.lineWidth = 2.0;
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
                                opacity: 0.5
                            }
                        }
                    }

                    RowLayout {
                        visible: !root.multiMode && propertiesController.isDirectory
                        spacing: 12
                        Layout.fillWidth: true
                        Label {
                            text: "Contents"
                            Layout.preferredWidth: 95
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            opacity: 0.6
                        }
                        
                        RowLayout {
                            spacing: 6
                            
                            Rectangle {
                                radius: 6
                                color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.04)
                                border.color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.1)
                                border.width: 1
                                implicitWidth: fileLabel.implicitWidth + 12
                                implicitHeight: 20
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
                                radius: 6
                                color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.04)
                                border.color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.1)
                                border.width: 1
                                implicitWidth: folderLabel.implicitWidth + 12
                                implicitHeight: 20
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
                                radius: 8
                                color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.03)
                                border.color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.08)
                                border.width: 1
                                Layout.fillWidth: true
                                implicitHeight: 30
                                
                                RowLayout {
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Image {
                                        source: "../assets/icons/" + modelData.icon + ".svg"
                                        sourceSize: Qt.size(12, 12)
                                        opacity: 0.5
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
                            radius: 8
                            color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.03)
                            border.color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.06)
                            border.width: 1
                            implicitHeight: 32

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8

                                Image {
                                    source: "image://icon/" + modelData
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
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            
            Rectangle { 
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Theme.border
                opacity: 0.1
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
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
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    background: Rectangle {
                        implicitWidth: 100
                        implicitHeight: 34
                        radius: 10
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
