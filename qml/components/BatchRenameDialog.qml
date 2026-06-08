import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"
import "common"
import "dialogs"

Dialog {
    id: root

    property var controller: null
    property var appRoot: null
    property var sourcePaths: []
    property var previewModel: []
    property bool hasConflicts: false
    property bool isApplied: false
    property int successCount: 0
    property int failCount: 0
    property string searchFilter: ""
    property bool suppressPreviewUpdates: false
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.useNativeIcons
                                           : true

    title: "Batch Rename"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: 900
    height: 580
    padding: 0

    background: DialogShell {
        accentColor: Theme.categoryAction
        shellBorderColor: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.28 : 0.20)
    }

    // Custom ComboBox
    component ThemedComboBox : ComboBox {
        id: combo
        
        delegate: ItemDelegate {
            width: combo.width; height: 36
            contentItem: Label {
                text: modelData
                color: highlighted ? Theme.accent : Theme.textPrimary
                font.pixelSize: 12; verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: highlighted ? Theme.itemHoverFill : "transparent"
                radius: Theme.radiusSm
            }
            highlighted: combo.highlightedIndex === index
        }

        indicator: RecolorSvgIcon {
            x: combo.width - width - 10
            y: (combo.height - height) / 2
            width: 10; height: 10; sourcePath: "../assets/icons/arrow-up.svg"
            recolorColor: Theme.textPrimary
            rotation: combo.opened ? 0 : 180; opacity: 0.5
        }

        contentItem: Label {
            leftPadding: 10; text: combo.displayText; font.pixelSize: 12
            color: Theme.textPrimary; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
        }

        background: Rectangle {
            implicitHeight: 36; radius: Theme.radiusSm; color: Theme.panelSurfaceSoft
            border.color: combo.opened ? Theme.accent : Theme.panelBorder
            border.width: combo.opened ? 2 : 1
        }

        popup: Popup {
            y: combo.height + 4; width: combo.width
            implicitHeight: contentItem.implicitHeight + 8; padding: 4
            contentItem: ListView {
                clip: true; implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator { }
            }
            background: Rectangle {
                color: Theme.menuSurface; radius: Theme.radiusSm; border.color: Theme.menuBorder
                layer.enabled: true; layer.effect: MultiEffect { shadowEnabled: true; shadowColor: Theme.glassShadow; shadowBlur: 15 }
            }
        }
    }

    // Custom SpinBox
    component ThemedSpinBox : SpinBox {
        id: sb
        editable: true
        
        leftPadding: 28
        rightPadding: 28
        
        contentItem: TextInput {
            text: sb.textFromValue(sb.value, sb.locale)
            font.pixelSize: 12
            color: Theme.textPrimary
            selectionColor: Theme.accent
            selectedTextColor: "white"
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            readOnly: !sb.editable
            validator: sb.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly
        }
        
        up.indicator: Rectangle {
            id: upIndicator
            x: sb.width - width
            height: sb.height
            width: 28
            radius: Theme.radiusSm
            color: sb.up.pressed ? Theme.surfaceActive : (sb.up.hovered ? Theme.panelSurfaceSoft : "transparent")
            border.color: Theme.panelBorder
            border.width: 1
            
            Label {
                text: "+"
                font.pixelSize: 13
                color: Theme.textPrimary
                anchors.centerIn: parent
            }
        }
        
        down.indicator: Rectangle {
            id: downIndicator
            x: 0
            height: sb.height
            width: 28
            radius: Theme.radiusSm
            color: sb.down.pressed ? Theme.surfaceActive : (sb.down.hovered ? Theme.panelSurfaceSoft : "transparent")
            border.color: Theme.panelBorder
            border.width: 1
            
            Label {
                text: "-"
                font.pixelSize: 13
                color: Theme.textPrimary
                anchors.centerIn: parent
            }
        }
        
        background: Rectangle {
            implicitWidth: 100
            implicitHeight: 36
            radius: Theme.radiusSm
            color: Theme.panelSurfaceSoft
            border.color: sb.activeFocus ? Theme.accent : Theme.panelBorder
            border.width: sb.activeFocus ? 2 : 1
        }
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/rename.svg"
        iconTint: Theme.categoryAction
        accentColor: Theme.categoryAction
        title: root.isApplied ? "Batch Rename Status" : "Batch Rename"
        subtitle: root.isApplied
                  ? (root.successCount + " of " + (root.successCount + root.failCount) + " items renamed successfully")
                  : (root.sourcePaths.length + " items selected")
        closeText: "x"
        onCloseRequested: root.isApplied ? root.accept() : root.reject()
    }

    footer: DialogFooter {
        DialogActionButton {
            text: "Cancel"
            visible: !root.isApplied
            highlighted: false
            onClicked: root.reject()
        }

        DialogActionButton {
            text: root.isApplied ? "Close" : "Apply Changes"
            enabled: root.isApplied || (!root.hasConflicts && root.previewModel.length > 0)
            highlighted: true
            onClicked: {
                if (root.isApplied) {
                    root.accept()
                } else {
                    applyChanges()
                }
            }
        }
    }

    ListModel {
        id: internalPreviewModel
    }

    onOpened: {
        Qt.callLater(() => contentItem.forceActiveFocus())
        root.isApplied = false
        root.successCount = 0
        root.failCount = 0
        root.searchFilter = ""
        filterInput.text = ""
        updatePreview()
    }

    onClosed: {
        resetEditorState()
        if (root.appRoot && root.appRoot.finishRenamePreviewSuppression) {
            root.appRoot.finishRenamePreviewSuppression(true)
        }
    }

    function resetEditorState() {
        root.suppressPreviewUpdates = true

        findField.text = ""
        replaceField.text = ""
        prefixField.text = ""
        suffixField.text = ""
        seqBaseNameField.text = ""
        filterInput.text = ""

        caseSensitiveCheck.checked = false
        ruleTypeCombo.currentIndex = 0
        startValue.value = 0
        paddingValue.value = 2
        numPosCombo.currentIndex = 0
        seqStartValue.value = 1
        seqPaddingValue.value = 2

        root.searchFilter = ""
        root.previewModel = []
        root.hasConflicts = false
        root.isApplied = false
        root.successCount = 0
        root.failCount = 0
        internalPreviewModel.clear()

        root.suppressPreviewUpdates = false
    }

    function getRules() {
        let rules = []
        if (ruleTypeCombo.currentIndex === 0) { // Search & Replace
            rules.push({
                "type": "replace",
                "search": findField.text,
                "replace": replaceField.text,
                "caseSensitive": caseSensitiveCheck.checked
            })
        } else if (ruleTypeCombo.currentIndex === 1) { // Format
            rules.push({
                "type": "format",
                "prefix": prefixField.text,
                "suffix": suffixField.text
            })
        } else if (ruleTypeCombo.currentIndex === 2) { // Numbering
            rules.push({
                "type": "numbering",
                "start": startValue.value,
                "padding": paddingValue.value,
                "position": numPosCombo.currentText.toLowerCase()
            })
        } else if (ruleTypeCombo.currentIndex === 3) { // Sequence
            rules.push({
                "type": "template",
                "text": seqBaseNameField.text,
                "start": seqStartValue.value,
                "padding": seqPaddingValue.value
            })
        }
        return rules
    }

    function updatePreview() {
        if (root.suppressPreviewUpdates) {
            return;
        }
        if (!controller) {
            return;
        }
        if (root.isApplied) {
            return;
        }
        
        let rules = getRules()
        let preview = controller.previewBatchRename(sourcePaths, rules)
        
        root.previewModel = preview
        filterPreviewModel()
    }

    function filterPreviewModel() {
        internalPreviewModel.clear()
        let conflict = false
        let query = root.searchFilter.toLowerCase().trim()
        
        for (let i = 0; i < root.previewModel.length; i++) {
            let item = root.previewModel[i]
            if (item.hasConflict) {
                conflict = true
            }
            
            if (query === "" || 
                item.oldName.toLowerCase().includes(query) || 
                item.newName.toLowerCase().includes(query)) {
                
                internalPreviewModel.append({
                    "oldPath": item.oldPath,
                    "oldName": item.oldName,
                    "newName": item.newName,
                    "newPath": item.newPath,
                    "hasConflict": item.hasConflict || false,
                    "error": item.error || "",
                    "success": (item.success !== undefined && item.success !== null) ? item.success : false,
                    "originalIndex": i
                })
            }
        }
        root.hasConflicts = conflict
    }

    function applyChanges() {
        if (!controller) return;
        
        let rules = getRules()
        let results = controller.applyBatchRename(sourcePaths, rules)
        
        let successCount = 0
        let failCount = 0
        let updatedPreview = []
        
        for (let i = 0; i < results.length; i++) {
            let res = results[i]
            let prevItem = root.previewModel[i]
            let successVal = res.success === true
            if (successVal) {
                successCount++
            } else {
                failCount++
            }
            updatedPreview.push({
                "oldPath": prevItem.oldPath,
                "oldName": prevItem.oldName,
                "newName": prevItem.newName,
                "newPath": prevItem.newPath,
                "hasConflict": prevItem.hasConflict || false,
                "success": successVal,
                "error": res.error || ""
            })
        }
        
        root.successCount = successCount
        root.failCount = failCount
        root.isApplied = true
        root.previewModel = updatedPreview
        filterPreviewModel()
    }

    contentItem: RowLayout {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        spacing: 0
        clip: true
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                root.reject()
                event.accepted = true
            } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                if (root.isApplied) {
                    root.accept()
                } else if (!root.hasConflicts && root.previewModel.length > 0) {
                    applyChanges()
                }
                event.accepted = true
            }
        }
        
        // --- Left Panel: Rules ---
        ScrollView {
            id: leftScroll
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            clip: true
            
            leftPadding: 16
            rightPadding: 16
            topPadding: 16
            bottomPadding: 16
            
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            
            ColumnLayout {
                width: 288
                spacing: 16
                
                // Hide rules once applied
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !root.isApplied
                    spacing: 16
                    
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "RENAME METHOD"; color: Theme.categoryAction; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                        ThemedComboBox {
                            id: ruleTypeCombo
                            Layout.fillWidth: true
                            model: ["Search & Replace", "Format (Prefix/Suffix)", "Append/Prepend Number", "Sequence (Name + Number)"]
                            onCurrentIndexChanged: updatePreview()
                        }
                    }
                    
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.panelBorder; opacity: 0.3 }

                    StackLayout {
                        id: ruleStack
                        Layout.fillWidth: true
                        currentIndex: ruleTypeCombo.currentIndex
                        
                        // 0: Search & Replace
                        ColumnLayout {
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Find"; font.pixelSize: 11; color: Theme.textSecondary }
                                TextField {
                                    id: findField; placeholderText: "Text to find..."; Layout.fillWidth: true
                                    onTextChanged: updatePreview(); font.pixelSize: 12; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { color: Theme.panelSurfaceSoft; radius: Theme.radiusSm; border.color: findField.activeFocus ? Theme.accent : Theme.panelBorder; border.width: findField.activeFocus ? 2 : 1 }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Replace with"; font.pixelSize: 11; color: Theme.textSecondary }
                                TextField {
                                    id: replaceField; placeholderText: "Replacement..."; Layout.fillWidth: true
                                    onTextChanged: updatePreview(); font.pixelSize: 12; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { color: Theme.panelSurfaceSoft; radius: Theme.radiusSm; border.color: replaceField.activeFocus ? Theme.accent : Theme.panelBorder; border.width: replaceField.activeFocus ? 2 : 1 }
                                }
                            }
                            CheckBox {
                                id: caseSensitiveCheck; text: "Case sensitive"; onCheckedChanged: updatePreview(); font.pixelSize: 12
                                indicator: Rectangle {
                                    implicitWidth: 18; implicitHeight: 18; radius: Theme.radiusSm
                                    color: caseSensitiveCheck.checked ? Theme.accent : "transparent"
                                    border.color: caseSensitiveCheck.checked ? Theme.accent : Theme.panelBorder
                                    Image { anchors.centerIn: parent; width: 10; height: 10; source: "../assets/icons/select-all.svg"; visible: caseSensitiveCheck.checked; layer.enabled: true; layer.effect: MultiEffect { colorization: 1.0; colorizationColor: "white" } }
                                }
                                contentItem: Label {
                                    text: caseSensitiveCheck.text
                                    font.pixelSize: 12
                                    color: Theme.textPrimary
                                    leftPadding: 24
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                        
                        // 1: Format
                        ColumnLayout {
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Prefix"; font.pixelSize: 11; color: Theme.textSecondary }
                                TextField {
                                    id: prefixField; placeholderText: "Add to start..."; Layout.fillWidth: true
                                    onTextChanged: updatePreview(); font.pixelSize: 12; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { color: Theme.panelSurfaceSoft; radius: Theme.radiusSm; border.color: prefixField.activeFocus ? Theme.accent : Theme.panelBorder; border.width: prefixField.activeFocus ? 2 : 1 }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Suffix"; font.pixelSize: 11; color: Theme.textSecondary }
                                TextField {
                                    id: suffixField; placeholderText: "Add to end..."; Layout.fillWidth: true
                                    onTextChanged: updatePreview(); font.pixelSize: 12; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { color: Theme.panelSurfaceSoft; radius: Theme.radiusSm; border.color: suffixField.activeFocus ? Theme.accent : Theme.panelBorder; border.width: suffixField.activeFocus ? 2 : 1 }
                                }
                            }
                        }
                        
                        // 2: Numbering
                        ColumnLayout {
                            spacing: 12
                            RowLayout {
                                spacing: 12
                                ColumnLayout {
                                    Label { text: "Start Index"; font.pixelSize: 11; color: Theme.textSecondary }
                                    ThemedSpinBox { id: startValue; from: 0; to: 999999; onValueChanged: updatePreview() }
                                }
                                ColumnLayout {
                                    Label { text: "Digits"; font.pixelSize: 11; color: Theme.textSecondary }
                                    ThemedSpinBox { id: paddingValue; from: 1; to: 10; value: 2; onValueChanged: updatePreview() }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Position"; font.pixelSize: 11; color: Theme.textSecondary }
                                ThemedComboBox { id: numPosCombo; Layout.fillWidth: true; model: ["Suffix", "Prefix"]; onCurrentIndexChanged: updatePreview() }
                            }
                        }

                        // 3: Sequence
                        ColumnLayout {
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Base Name"; font.pixelSize: 11; color: Theme.textSecondary }
                                TextField {
                                    id: seqBaseNameField; placeholderText: "e.g. Photo_"; Layout.fillWidth: true
                                    onTextChanged: updatePreview(); font.pixelSize: 12; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { color: Theme.panelSurfaceSoft; radius: Theme.radiusSm; border.color: seqBaseNameField.activeFocus ? Theme.accent : Theme.panelBorder; border.width: seqBaseNameField.activeFocus ? 2 : 1 }
                                }
                            }
                            RowLayout {
                                spacing: 12
                                ColumnLayout {
                                    Label { text: "Start At"; font.pixelSize: 11; color: Theme.textSecondary }
                                    ThemedSpinBox { id: seqStartValue; from: 0; to: 999999; value: 1; onValueChanged: updatePreview() }
                                }
                                ColumnLayout {
                                    Label { text: "Digits"; font.pixelSize: 11; color: Theme.textSecondary }
                                    ThemedSpinBox { id: seqPaddingValue; from: 1; to: 10; value: 2; onValueChanged: updatePreview() }
                                }
                            }
                        }
                    }
                }
                
                // Show summary once applied
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.isApplied
                    spacing: 16
                    
                    Label { text: "STATUS"; color: Theme.textSecondary; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                    
                    SurfaceCard {
                        Layout.fillWidth: true
                        implicitHeight: 160
                        cornerRadius: Theme.radiusMd
                        surfaceColor: Theme.panelSurfaceSoft
                        strokeColor: root.failCount === 0
                            ? Theme.withAlpha(Theme.success, 0.24)
                            : Theme.withAlpha(Theme.danger, 0.24)
                        
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12
                            
                            Rectangle {
                                width: 44; height: 44; radius: 22
                                color: root.failCount === 0 ? Theme.withAlpha(Theme.success, 0.10) : Theme.withAlpha(Theme.danger, 0.10)
                                border.color: root.failCount === 0 ? Theme.withAlpha(Theme.success, 0.20) : Theme.withAlpha(Theme.danger, 0.20)
                                Layout.alignment: Qt.AlignHCenter
                                
                                Image {
                                    anchors.centerIn: parent
                                    source: root.failCount === 0 ? "../assets/icons/select-all.svg" : "../assets/icons/info.svg"
                                    width: 20; height: 20
                                    layer.enabled: true
                                    layer.effect: MultiEffect { colorization: 1.0; colorizationColor: root.failCount === 0 ? Theme.success : Theme.danger }
                                }
                            }
                            
                            ColumnLayout {
                                spacing: 2
                                Layout.alignment: Qt.AlignHCenter
                                Label {
                                    text: root.failCount === 0 ? "Completed Successfully" : "Completed with Errors"
                                    font.pixelSize: 14; font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.alignment: Qt.AlignHCenter
                                }
                                Label {
                                    text: root.successCount + " of " + (root.successCount + root.failCount) + " files renamed"
                                    font.pixelSize: 12; color: Theme.textSecondary; Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // Conflict warning banner
                SurfaceCard {
                    Layout.fillWidth: true
                    implicitHeight: 60
                    visible: !root.isApplied && root.hasConflicts
                    surfaceColor: Theme.withAlpha(Theme.danger, 0.08)
                    cornerRadius: Theme.radiusMd
                    strokeColor: Theme.withAlpha(Theme.danger, 0.20)
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8
                        Image {
                            source: "../assets/icons/info.svg"
                            Layout.preferredWidth: 16; Layout.preferredHeight: 16
                            layer.enabled: true
                            layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.danger }
                        }
                        Label {
                            text: "Naming conflicts detected. Fix them to apply changes."
                            font.pixelSize: 11; color: Theme.danger; Layout.fillWidth: true; wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        // Vertical divider
        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.28 : 0.18)
            opacity: 0.4
        }

        // --- Right Panel: Files list ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            
            // Search / Filter bar above the preview
            Rectangle {
                Layout.fillWidth: true
                height: 48
                color: Theme.panelSurface
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 8
                    
                    RecolorSvgIcon {
                        sourcePath: "../assets/icons/search.svg"
                        recolorColor: Theme.textSecondary
                        Layout.preferredWidth: 14; Layout.preferredHeight: 14
                    }
                    
                    TextField {
                        id: filterInput
                        placeholderText: "Filter files..."
                        Layout.fillWidth: true
                        font.pixelSize: 12
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textSecondary
                        leftPadding: 4
                        background: Rectangle { color: "transparent" }
                        onTextChanged: {
                            root.searchFilter = text
                            filterPreviewModel()
                        }
                    }
                    
                    Button {
                        flat: true
                        visible: filterInput.text.length > 0
                        Layout.preferredWidth: 24; Layout.preferredHeight: 24
                        onClicked: filterInput.text = ""
                        background: Item {}
                        contentItem: RecolorSvgIcon {
                            sourcePath: "../assets/icons/delete.svg"
                            recolorColor: Theme.textSecondary
                            anchors.centerIn: parent
                            width: 12; height: 12
                        }
                    }
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.panelBorder; opacity: 0.3 }
            }
            
            // Preview list subheader
            Rectangle {
                Layout.fillWidth: true; height: 32; color: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.08 : 0.045)
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
                    Label {
                        text: "FILE PREVIEW"
                        font.bold: true; color: Theme.categoryAction; font.pixelSize: 10; font.letterSpacing: 1
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: internalPreviewModel.count + " files"
                        color: Theme.textSecondary; font.pixelSize: 10; font.bold: true
                    }
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.panelBorder; opacity: 0.3 }
            }
            
            // List of files
            ListView {
                id: previewList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: internalPreviewModel
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                
                ScrollBar.vertical: ScrollBar { 
                    policy: ScrollBar.AsNeeded 
                }
                
                delegate: Rectangle {
                    id: delegateRoot
                    width: previewList.width
                    height: delegateLayout.implicitHeight + 14
                    color: hoverHandler.hovered ? Theme.itemHoverFill : (index % 2 === 1 ? Theme.withAlpha(Theme.textPrimary, 0.01) : "transparent")
                    
                    HoverHandler {
                        id: hoverHandler
                    }
                    
                    RowLayout {
                        id: delegateLayout
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12
                        
                        // Icon
                        Rectangle {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            Layout.alignment: Qt.AlignVCenter
                            radius: Theme.radiusSm
                            color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.20 : 0.05)
                            border.color: Theme.panelBorder
                            border.width: 1
                            
                            Image {
                                anchors.centerIn: parent
                                width: 20; height: 20
                                source: root.useNativeIcons
                                        ? "image://icon/" + encodeURIComponent(model.oldPath)
                                        : fileTypeIconResolver.iconForPath(model.oldPath)
                                smooth: true
                            }
                        }
                        
                        // Text details
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 2
                            
                            RowLayout {
                                spacing: 4
                                Label {
                                    text: "Was:"
                                    font.pixelSize: 11; font.weight: Font.Medium
                                    color: Theme.textSecondary
                                    Layout.preferredWidth: 32
                                }
                                Label {
                                    text: model.oldName
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                }
                            }
                            
                            RowLayout {
                                spacing: 4
                                Label {
                                    text: "New:"
                                    font.pixelSize: 12; font.weight: Font.DemiBold
                                    color: {
                                        if (root.isApplied) {
                                            return model.success ? Theme.success : Theme.danger
                                        }
                                        if (model.hasConflict) return Theme.danger
                                        return model.newName !== model.oldName ? Theme.accent : Theme.textPrimary
                                    }
                                    Layout.preferredWidth: 32
                                }
                                Label {
                                    text: model.newName
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    font.pixelSize: 12; font.weight: Font.DemiBold
                                    color: {
                                        if (root.isApplied) {
                                            return model.success ? Theme.success : Theme.danger
                                        }
                                        if (model.hasConflict) return Theme.danger
                                        return model.newName !== model.oldName ? Theme.accent : Theme.textPrimary
                                    }
                                }
                            }
                            
                            Label {
                                visible: model.hasConflict || (root.isApplied && !model.success)
                                text: model.error
                                font.pixelSize: 10
                                color: Theme.danger
                                font.italic: true
                                Layout.leftMargin: 36
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                            }
                        }
                        
                        // Status indicators
                        Item {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            Layout.alignment: Qt.AlignVCenter
                            
                            // Success/Error checkmark
                            Image {
                                anchors.centerIn: parent
                                width: 14; height: 14
                                visible: root.isApplied
                                source: model.success ? "../assets/icons/select-all.svg" : "../assets/icons/info.svg"
                                layer.enabled: true
                                layer.effect: MultiEffect { colorization: 1.0; colorizationColor: model.success ? Theme.success : Theme.danger }
                            }
                            
                            // Pending change dot
                            Rectangle {
                                anchors.centerIn: parent
                                width: 6; height: 6; radius: 3
                                color: Theme.accent
                                visible: !root.isApplied && !model.hasConflict && (model.newName !== model.oldName)
                            }
                            
                            // Naming Conflict symbol
                            Image {
                                anchors.centerIn: parent
                                width: 14; height: 14
                                visible: !root.isApplied && model.hasConflict
                                source: "../assets/icons/info.svg"
                                layer.enabled: true
                                layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.danger }
                            }
                        }
                    }
                }
            }
        }
    }
}

