import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../style"
import "dialogs"
import "filepanel"
import "common"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent ? parent.width - 32 : 1100, 1100)
    height: Math.min(parent ? parent.height - 32 : 720, 720)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property var appRoot: null
    property var stagedOverrides: ({})
    property string activeEditRoleId: ""
    property color dialogAccent: Theme.accent
    property string activeGroup: "File panels"

    function resetStagedMap() {
        var rawMap = ({})
        if (typeof appSettings !== "undefined" && appSettings) {
            rawMap = appSettings.textColorOverrides
        }
        
        // Ensure all MVP roles have at least a dummy structure
        var roles = [
            "fileNameText", "folderNameText", "fileExtensionText", "fileSecondaryText", "filePathText",
            "sidebarText", "thisPcText", "statusText", "dialogSecondaryText", "commandPaletteText"
        ]
        
        var clone = {}
        for (var i = 0; i < roles.length; ++i) {
            var role = roles[i]
            if (rawMap && rawMap[role]) {
                clone[role] = {
                    enabled: !!rawMap[role].enabled,
                    color: rawMap[role].color || ""
                }
            } else {
                clone[role] = { enabled: false, color: "" }
            }
        }
        
        // Copy other unknown roles to preserve them
        if (rawMap) {
            for (var key in rawMap) {
                if (roles.indexOf(key) < 0) {
                    clone[key] = rawMap[key]
                }
            }
        }
        
        stagedOverrides = clone
    }

    function saveStagedMap() {
        if (typeof appSettings !== "undefined" && appSettings) {
            appSettings.saveTextColorOverrides(stagedOverrides)
        }
    }

    function resolvePreviewColor(roleId, fallbackColor) {
        if (stagedOverrides && stagedOverrides[roleId]) {
            var entry = stagedOverrides[roleId]
            if (entry.enabled && entry.color) {
                return entry.color
            }
        }
        return fallbackColor
    }

    function checkContrastWarning(roleId, fallbackColor) {
        var textColorStr = resolvePreviewColor(roleId, fallbackColor)
        var textColor = Qt.color(textColorStr)
        
        var bgNormal = Theme.panelSurface
        var bgSelected = Theme.itemSelectedFill
        
        if (roleId === "sidebarText") {
            bgNormal = Theme.surface
        } else if (roleId === "thisPcText") {
            bgNormal = Theme.panelSurfaceSoft
        }
        
        var ratioNormal = Theme.contrastRatio(bgNormal, textColor)
        var ratioSelected = Theme.contrastRatio(bgSelected, textColor)
        
        if (ratioNormal < 4.5 && ratioSelected < 4.5) {
            return "Low contrast on normal & selected rows"
        } else if (ratioNormal < 4.5) {
            return "Low contrast on normal background"
        } else if (ratioSelected < 4.5) {
            return "Low contrast on selection highlight"
        }
        return ""
    }

    function openPicker(roleId) {
        activeEditRoleId = roleId
        var current = resolvePreviewColor(roleId, "#ffffff")
        colorPicker.selectedColor = current
        colorPicker.open()
    }

    function updateRoleColor(roleId, colorHex) {
        if (!stagedOverrides[roleId]) {
            stagedOverrides[roleId] = { enabled: false, color: "" }
        }
        stagedOverrides[roleId].color = colorHex
        stagedOverrides[roleId].enabled = true
        stagedOverrides = Object.assign({}, stagedOverrides)
    }

    function updateRoleEnabled(roleId, enabled) {
        if (!stagedOverrides[roleId]) {
            stagedOverrides[roleId] = { enabled: false, color: "" }
        }
        stagedOverrides[roleId].enabled = enabled
        stagedOverrides = Object.assign({}, stagedOverrides)
    }

    function resetRoleToDefault(roleId) {
        if (!stagedOverrides[roleId]) {
            stagedOverrides[roleId] = { enabled: false, color: "" }
        }
        stagedOverrides[roleId].enabled = false
        stagedOverrides = Object.assign({}, stagedOverrides)
    }

    onOpened: {
        resetStagedMap()
    }

    background: DialogShell {
        accentColor: root.dialogAccent
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
        shadowBlur: 16
        shadowVerticalOffset: 5
    }

    ColorDialog {
        id: colorPicker
        title: "Choose Text Color"
        onAccepted: {
            if (root.activeEditRoleId.length > 0) {
                root.updateRoleColor(root.activeEditRoleId, selectedColor.toString())
            }
        }
    }

    DialogHeader {
        id: dialogHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/theme.svg"
        iconTint: root.dialogAccent
        accentColor: root.dialogAccent
        title: "Custom Text Colors"
        subtitle: "Customize specific text elements. Overrides win over the selected theme."
        closeText: "x"
        onCloseRequested: root.close()
    }

    DialogFooter {
        id: dialogFooter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // Font Context display
            RowLayout {
                spacing: 8
                Label {
                    text: "Font context: " + (typeof appSettings !== "undefined" && appSettings ? appSettings.resolvedFontFamily : "Default") + " (" + (typeof appSettings !== "undefined" && appSettings ? appSettings.fontScale : 100) + "%)"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.textSecondary
                }
                
                Button {
                    flat: true
                    text: "[Open Font Settings]"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    contentItem: Label {
                        text: parent.text
                        color: root.dialogAccent
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        font.underline: true
                    }
                    background: null
                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                    onClicked: {
                        root.close()
                        if (root.appRoot && root.appRoot.openSettingsDialog) {
                            root.appRoot.openSettingsDialog()
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            DialogActionButton {
                text: "Reset All"
                highlighted: false
                secondaryTextColor: Theme.danger
                onClicked: {
                    var roles = [
                        "fileNameText", "folderNameText", "fileExtensionText", "fileSecondaryText", "filePathText",
                        "sidebarText", "thisPcText", "statusText", "dialogSecondaryText", "commandPaletteText"
                    ]
                    for (var i = 0; i < roles.length; ++i) {
                        root.resetRoleToDefault(roles[i])
                    }
                }
            }

            DialogActionButton {
                text: "Cancel"
                highlighted: false
                secondaryTextColor: Theme.textSecondary
                onClicked: root.close()
            }

            DialogActionButton {
                text: "Apply"
                highlighted: false
                secondaryTextColor: root.dialogAccent
                onClicked: root.saveStagedMap()
            }

            DialogActionButton {
                text: "Done"
                highlighted: true
                primaryColor: root.dialogAccent
                onClicked: {
                    root.saveStagedMap()
                    root.close()
                }
            }
        }
    }

    // Main Content
    RowLayout {
        anchors.top: dialogHeader.bottom
        anchors.bottom: dialogFooter.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 16

        // Left Panel: Group tabs and Role list
        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: 460
            spacing: 10

            // Group Tabs
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: ["File panels", "Navigation", "App chrome and workflows"]
                    delegate: Button {
                        id: tabBtn
                        Layout.fillWidth: true
                        implicitHeight: 30
                        flat: true
                        
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: root.activeGroup === modelData
                                   ? Theme.withAlpha(root.dialogAccent, 0.12)
                                   : (tabBtn.hovered ? Theme.withAlpha(Theme.textPrimary, 0.04) : "transparent")
                            border.color: root.activeGroup === modelData ? root.dialogAccent : "transparent"
                            border.width: 1
                        }
                        
                        contentItem: Label {
                            text: modelData
                            color: root.activeGroup === modelData ? Theme.textPrimary : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            font.bold: root.activeGroup === modelData
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: root.activeGroup = modelData
                    }
                }
            }

            // Role List Scroll Area
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: typeof appSettings !== "undefined" ? appSettings.rolesMetadata() : []
                        delegate: Rectangle {
                            id: roleCard
                            Layout.fillWidth: true
                            implicitHeight: roleRowLayout.implicitHeight + 16
                            radius: Theme.radiusSm
                            color: Theme.panelSurfaceSoft
                            border.color: Theme.panelBorder
                            border.width: 1
                            visible: modelData.group === root.activeGroup
                            
                            // To hide height correctly when not in active group
                            height: visible ? implicitHeight : 0
                            
                            RowLayout {
                                id: roleRowLayout
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    RowLayout {
                                        spacing: 6
                                        Label {
                                            text: modelData.name
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeLabel
                                            font.bold: true
                                            color: Theme.textPrimary
                                        }
                                        
                                        // Contrast Warning badge
                                        property string warningMsg: root.checkContrastWarning(modelData.id, modelData.fallbackToken === "textPrimary" ? Theme.textPrimary : Theme.textSecondary)
                                        Rectangle {
                                            visible: parent.warningMsg.length > 0 && root.resolvePreviewColor(modelData.id, "#000").length > 0
                                            color: Theme.withAlpha(Theme.warning, 0.15)
                                            border.color: Theme.warning
                                            radius: Theme.radiusXs
                                            implicitHeight: 18
                                            implicitWidth: warningLabel.implicitWidth + 8
                                            ToolTip.visible: warningHover.hovered
                                            ToolTip.text: parent.warningMsg
                                            
                                            Label {
                                                id: warningLabel
                                                anchors.centerIn: parent
                                                text: "!"
                                                color: Theme.warning
                                                font.bold: true
                                                font.pixelSize: Theme.fontSizeMicro
                                            }
                                            HoverHandler { id: warningHover }
                                        }
                                    }

                                    Label {
                                        text: modelData.description
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: Theme.textSecondary
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }

                                // Toggle enabled
                                CheckBox {
                                    checked: root.stagedOverrides[modelData.id] ? !!root.stagedOverrides[modelData.id].enabled : false
                                    onToggled: root.updateRoleEnabled(modelData.id, checked)
                                }

                                // Color swatch button
                                Button {
                                    id: swatchBtn
                                    implicitWidth: 32
                                    implicitHeight: 24
                                    enabled: root.stagedOverrides[modelData.id] ? !!root.stagedOverrides[modelData.id].enabled : false
                                    
                                    background: Rectangle {
                                        radius: Theme.radiusXs
                                        color: root.resolvePreviewColor(modelData.id, modelData.fallbackToken === "textPrimary" ? Theme.textPrimary : Theme.textSecondary)
                                        border.color: swatchBtn.hovered ? Theme.accent : Theme.border
                                        border.width: 1
                                        opacity: swatchBtn.enabled ? 1.0 : 0.28
                                    }
                                    onClicked: root.openPicker(modelData.id)
                                }

                                // Reset role button
                                Button {
                                    text: "Reset"
                                    flat: true
                                    implicitHeight: 24
                                    implicitWidth: 44
                                    contentItem: Label {
                                        text: parent.text
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCaption
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: root.resetRoleToDefault(modelData.id)
                                }
                            }
                        }
                    }
                }
            }
        }

        // Divider
        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: Theme.panelBorder
        }

        // Right Panel: Live Preview Panels
        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: "LIVE PREVIEW"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                font.bold: true
                color: Theme.textPrimary
            }

            // Preview Scroll Area
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: 14

                    // 1. File Panel Details Preview
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: "File panels (Details and Brief View)"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                        }

                        // Normal Details Row
                        Rectangle {
                            Layout.fillWidth: true
                            height: 38
                            radius: Theme.radiusSm
                            color: Theme.panelSurface
                            border.color: Theme.panelBorder
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 8
                                
                                Rectangle { width: 14; height: 14; color: Theme.success; radius: 3 } // folder icon
                                Label {
                                    text: "Documents"
                                    color: root.resolvePreviewColor("folderNameText", Theme.textPrimary)
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeBody
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: "Folder"
                                    color: root.resolvePreviewColor("fileSecondaryText", Theme.textSecondary)
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLabel
                                }
                            }
                        }

                        // Selected file Row
                        Rectangle {
                            Layout.fillWidth: true
                            height: 38
                            radius: Theme.radiusSm
                            color: Theme.itemSelectedFill
                            border.color: Theme.itemSelectedBorder
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 8
                                
                                Rectangle { width: 14; height: 14; color: Theme.categoryInfo; radius: 3 } // file icon
                                RowLayout {
                                    spacing: 0
                                    Label {
                                        text: "report_summary"
                                        color: root.resolvePreviewColor("fileNameText", Theme.textPrimary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeBody
                                        font.bold: true
                                    }
                                    Label {
                                        text: ".pdf"
                                        color: root.resolvePreviewColor("fileExtensionText", Theme.textSecondary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeBody
                                    }
                                }
                                Item { Layout.fillWidth: true }
                                Label {
                                    text: "2.4 MB"
                                    color: root.resolvePreviewColor("fileSecondaryText", Theme.textSecondary)
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLabel
                                }
                            }
                        }
                    }

                    // 2. Sidebar Navigation Preview
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: "Navigation (Sidebar)"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 64
                            radius: Theme.radiusSm
                            color: Theme.surface
                            border.color: Theme.panelBorder
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 14
                                
                                ColumnLayout {
                                    spacing: 4
                                    Label {
                                        text: "Downloads"
                                        color: root.resolvePreviewColor("sidebarText", Theme.textPrimary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.bold: true
                                    }
                                    Label {
                                        text: "Folders / Tree view label"
                                        color: root.resolvePreviewColor("sidebarText", Theme.textPrimary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                    }
                                }
                            }
                        }
                    }

                    // 3. This PC Drive Preview
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: "This PC (Storage Tiles)"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 72
                            radius: Theme.radiusMd
                            color: Theme.panelSurfaceSoft
                            border.color: Theme.panelBorder
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12
                                
                                Rectangle { width: 32; height: 32; color: Theme.accent; radius: 6 } // Drive icon
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        text: "Local Disk (C:)"
                                        color: root.resolvePreviewColor("thisPcText", Theme.textPrimary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.bold: true
                                    }
                                    Label {
                                        text: "284 GB free of 953 GB"
                                        color: root.resolvePreviewColor("thisPcText", Theme.textSecondary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                    }
                                }
                            }
                        }
                    }

                    // 4. Status Bar and Command Palette Preview
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: "App Chrome (Status bar & Command Palette)"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                        }

                        // Command palette preview
                        Rectangle {
                            Layout.fillWidth: true
                            height: 56
                            radius: Theme.radiusLg
                            color: Theme.panelSurfaceStrong
                            border.color: Theme.accent
                            border.width: 1
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 8
                                
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        text: "Customize text colors"
                                        color: root.resolvePreviewColor("commandPaletteText", Theme.textPrimary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.bold: true
                                    }
                                    Label {
                                        text: "Open the custom colors editor"
                                        color: root.resolvePreviewColor("commandPaletteText", Theme.textSecondary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                    }
                                }
                                
                                // Shortcut key badge
                                Rectangle {
                                    width: 32
                                    height: 20
                                    radius: Theme.radiusXs
                                    color: Theme.withAlpha(Theme.surface, 0.6)
                                    border.color: Theme.border
                                    Label {
                                        anchors.centerIn: parent
                                        text: "F1"
                                        color: root.resolvePreviewColor("commandPaletteText", Theme.textSecondary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                    }
                                }
                            }
                        }

                        // Status bar preview
                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            radius: Theme.radiusXs
                            color: Theme.panelSurfaceStrong
                            border.color: Theme.panelBorder
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                Label {
                                    text: "Selected 1 item of 15"
                                    color: root.resolvePreviewColor("statusText", Theme.textSecondary)
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeMicro
                                }
                            }
                        }
                    }

                    // 5. Properties Dialog Secondary Preview
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: "Dialog Metadata & Path Summaries"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 60
                            radius: Theme.radiusSm
                            color: Theme.panelSurface
                            border.color: Theme.panelBorder
                            
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 4
                                
                                RowLayout {
                                    Label {
                                        text: "Parent Path:"
                                        color: root.resolvePreviewColor("dialogSecondaryText", Theme.textSecondary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCaption
                                    }
                                    Label {
                                        text: "/home/tankred/FM/FMQml"
                                        color: root.resolvePreviewColor("filePathText", Theme.textSecondary)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCaption
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
