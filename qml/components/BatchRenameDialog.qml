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
    property int selectedRuleIndex: -1
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
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLabel; verticalAlignment: Text.AlignVCenter
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
            leftPadding: 10; text: combo.displayText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLabel
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
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
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
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
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
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
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
        Item {
            Layout.fillWidth: true
        }

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

    ListModel {
        id: ruleModel
    }

    onOpened: {
        Qt.callLater(() => contentItem.forceActiveFocus())
        root.isApplied = false
        root.successCount = 0
        root.failCount = 0
        root.searchFilter = ""
        filterInput.text = ""
        ensureDefaultRule()
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

        ruleModel.clear()
        root.selectedRuleIndex = -1
        filterInput.text = ""

        root.searchFilter = ""
        root.previewModel = []
        root.hasConflicts = false
        root.isApplied = false
        root.successCount = 0
        root.failCount = 0
        internalPreviewModel.clear()

        root.suppressPreviewUpdates = false
    }

    function defaultRule(type) {
        const ruleType = type || "replace"
        return {
            "type": ruleType,
            "search": "",
            "replace": "",
            "caseSensitive": false,
            "regex": false,
            "prefix": "",
            "suffixText": "",
            "start": 0,
            "padding": 2,
            "position": "suffix",
            "text": "",
            "seqStart": 1,
            "seqPadding": 2,
            "mode": "lowercase"
        }
    }

    function ruleTypeIndex(type) {
        if (type === "format") return 1
        if (type === "numbering") return 2
        if (type === "template") return 3
        if (type === "transform") return 4
        return 0
    }

    function ruleTypeFromIndex(index) {
        if (index === 1) return "format"
        if (index === 2) return "numbering"
        if (index === 3) return "template"
        if (index === 4) return "transform"
        return "replace"
    }

    function transformModeIndex(mode) {
        const modes = ["lowercase", "uppercase", "titlecase", "trim", "collapse-spaces", "spaces-underscore", "spaces-dash", "remove-special"]
        const index = modes.indexOf(mode)
        return index >= 0 ? index : 0
    }

    function transformModeFromIndex(index) {
        const modes = ["lowercase", "uppercase", "titlecase", "trim", "collapse-spaces", "spaces-underscore", "spaces-dash", "remove-special"]
        return modes[Math.max(0, Math.min(index, modes.length - 1))]
    }

    function ruleTitle(rule) {
        if (!rule) return "Rule"
        if (rule.type === "format") return "Format"
        if (rule.type === "numbering") return "Numbering"
        if (rule.type === "template") return "Sequence"
        if (rule.type === "transform") return "Transform"
        return rule.regex ? "Regex Replace" : "Search & Replace"
    }

    function ruleSummary(rule) {
        if (!rule) return ""
        if (rule.type === "replace") {
            const search = rule.search || "text"
            return search + " -> " + (rule.replace || "")
        }
        if (rule.type === "format") return (rule.prefix || "") + "name" + (rule.suffixText || "")
        if (rule.type === "numbering") return rule.position + " from " + rule.start
        if (rule.type === "template") return (rule.text || "Name") + " + number"
        if (rule.type === "transform") {
            const labels = ["lowercase", "UPPERCASE", "Title Case", "Trim whitespace", "Collapse spaces", "Spaces to underscores", "Spaces to dashes", "Remove special chars"]
            return labels[transformModeIndex(rule.mode)]
        }
        return ""
    }

    function ensureDefaultRule() {
        if (ruleModel.count === 0) {
            ruleModel.append(defaultRule("replace"))
        }
        selectRule(Math.max(0, Math.min(root.selectedRuleIndex, ruleModel.count - 1)))
    }

    function selectRule(index) {
        if (index < 0 || index >= ruleModel.count) return
        root.selectedRuleIndex = index
        loadRuleToEditor(index)
    }

    function addRule(type) {
        saveSelectedRule()
        ruleModel.append(defaultRule(type || ruleTypeFromIndex(ruleTypeCombo.currentIndex)))
        selectRule(ruleModel.count - 1)
        updatePreview()
    }

    function removeSelectedRule() {
        if (ruleModel.count <= 1 || root.selectedRuleIndex < 0) return
        const nextIndex = Math.min(root.selectedRuleIndex, ruleModel.count - 2)
        ruleModel.remove(root.selectedRuleIndex)
        selectRule(nextIndex)
        updatePreview()
    }

    function loadRuleToEditor(index) {
        if (index < 0 || index >= ruleModel.count) return
        root.suppressPreviewUpdates = true
        const rule = ruleModel.get(index)
        ruleTypeCombo.currentIndex = ruleTypeIndex(rule.type)
        findField.text = rule.search || ""
        replaceField.text = rule.replace || ""
        caseSensitiveCheck.checked = rule.caseSensitive || false
        regexCheck.checked = rule.regex || false
        prefixField.text = rule.prefix || ""
        suffixField.text = rule.suffixText || ""
        startValue.value = rule.start === undefined ? 0 : rule.start
        paddingValue.value = rule.padding === undefined ? 2 : rule.padding
        numPosCombo.currentIndex = rule.position === "prefix" ? 1 : 0
        seqBaseNameField.text = rule.text || ""
        seqStartValue.value = rule.seqStart === undefined ? 1 : rule.seqStart
        seqPaddingValue.value = rule.seqPadding === undefined ? 2 : rule.seqPadding
        transformModeCombo.currentIndex = transformModeIndex(rule.mode)
        root.suppressPreviewUpdates = false
    }

    function saveSelectedRule() {
        if (root.suppressPreviewUpdates || root.selectedRuleIndex < 0 || root.selectedRuleIndex >= ruleModel.count) return
        ruleModel.set(root.selectedRuleIndex, {
            "type": ruleTypeFromIndex(ruleTypeCombo.currentIndex),
            "search": findField.text,
            "replace": replaceField.text,
            "caseSensitive": caseSensitiveCheck.checked,
            "regex": regexCheck.checked,
            "prefix": prefixField.text,
            "suffixText": suffixField.text,
            "start": startValue.value,
            "padding": paddingValue.value,
            "position": numPosCombo.currentText.toLowerCase(),
            "text": seqBaseNameField.text,
            "seqStart": seqStartValue.value,
            "seqPadding": seqPaddingValue.value,
            "mode": transformModeFromIndex(transformModeCombo.currentIndex)
        })
    }

    function editorChanged() {
        if (root.suppressPreviewUpdates) return
        saveSelectedRule()
        updatePreview()
    }

    function getRules() {
        saveSelectedRule()
        let rules = []
        for (let i = 0; i < ruleModel.count; ++i) {
            const rule = ruleModel.get(i)
            if (rule.type === "replace") {
                rules.push({
                "type": "replace",
                    "search": rule.search || "",
                    "replace": rule.replace || "",
                    "caseSensitive": rule.caseSensitive || false,
                    "regex": rule.regex || false
                })
            } else if (rule.type === "format") {
                rules.push({
                "type": "format",
                    "prefix": rule.prefix || "",
                    "suffix": rule.suffixText || ""
                })
            } else if (rule.type === "numbering") {
                rules.push({
                "type": "numbering",
                    "start": rule.start || 0,
                    "padding": rule.padding || 2,
                    "position": rule.position || "suffix"
                })
            } else if (rule.type === "template") {
                rules.push({
                "type": "template",
                    "text": rule.text || "",
                    "start": rule.seqStart || 1,
                    "padding": rule.seqPadding || 2
                })
            } else if (rule.type === "transform") {
                rules.push({
                    "type": "transform",
                    "mode": rule.mode || "lowercase"
                })
            }
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
                        Layout.fillWidth: true
                        spacing: 8
                        Label { text: "RULES"; color: Theme.categoryAction; font.pixelSize: Theme.fontSizeMicro; font.bold: true; font.letterSpacing: 1 }

                        Repeater {
                            model: ruleModel
                            delegate: Rectangle {
                                width: 288
                                height: 46
                                radius: Theme.radiusSm
                                color: index === root.selectedRuleIndex
                                       ? Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.18 : 0.10)
                                       : Theme.panelSurfaceSoft
                                border.color: index === root.selectedRuleIndex
                                              ? Theme.withAlpha(Theme.categoryAction, 0.48)
                                              : Theme.panelBorder
                                border.width: 1

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton
                                    onClicked: root.selectRule(index)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Label {
                                            text: (index + 1) + ". " + root.ruleTitle(ruleModel.get(index))
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeLabel
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: root.ruleSummary(ruleModel.get(index))
                                            color: Theme.textSecondary
                                            font.pixelSize: Theme.fontSizeCaption
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    Button {
                                        flat: true
                                        enabled: ruleModel.count > 1
                                        Layout.preferredWidth: 26
                                        Layout.preferredHeight: 26
                                        onClicked: {
                                            root.selectRule(index)
                                            root.removeSelectedRule()
                                        }
                                        background: Rectangle {
                                            radius: Theme.radiusSm
                                            color: parent.hovered ? Theme.withAlpha(Theme.danger, 0.12) : "transparent"
                                        }
                                        contentItem: RecolorSvgIcon {
                                            sourcePath: "../assets/icons/delete.svg"
                                            recolorColor: parent.enabled ? Theme.danger : Theme.withAlpha(Theme.textSecondary, 0.42)
                                            anchors.centerIn: parent
                                            width: 12
                                            height: 12
                                        }
                                    }
                                }
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            implicitHeight: 32
                            text: "+ Add Rule"
                            onClicked: root.addRule()
                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: parent.hovered ? Theme.itemHoverFill : Theme.panelSurfaceSoft
                                border.color: Theme.panelBorder
                                border.width: 1
                            }
                            contentItem: Label {
                                text: parent.text
                                color: Theme.categoryAction
                                font.pixelSize: Theme.fontSizeLabel
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                    
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "SELECTED RULE"; color: Theme.categoryAction; font.pixelSize: Theme.fontSizeMicro; font.bold: true; font.letterSpacing: 1 }
                        ThemedComboBox {
                            id: ruleTypeCombo
                            Layout.fillWidth: true
                            model: ["Search & Replace", "Format (Prefix/Suffix)", "Append/Prepend Number", "Sequence (Name + Number)", "Transform"]
                            onCurrentIndexChanged: editorChanged()
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
                                Label { text: "Find"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                TextField {
                                    id: findField; placeholderText: "Text to find..."; Layout.fillWidth: true
                                    onTextChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { color: Theme.panelSurfaceSoft; radius: Theme.radiusSm; border.color: findField.activeFocus ? Theme.accent : Theme.panelBorder; border.width: findField.activeFocus ? 2 : 1 }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Replace with"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                TextField {
                                    id: replaceField; placeholderText: "Replacement..."; Layout.fillWidth: true
                                    onTextChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { color: Theme.panelSurfaceSoft; radius: Theme.radiusSm; border.color: replaceField.activeFocus ? Theme.accent : Theme.panelBorder; border.width: replaceField.activeFocus ? 2 : 1 }
                                }
                            }
                            RowLayout {
                                spacing: 12
                                CheckBox {
                                    id: caseSensitiveCheck; text: "Case sensitive"; onCheckedChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel
                                    indicator: Rectangle {
                                        implicitWidth: 18; implicitHeight: 18; radius: Theme.radiusSm
                                        color: caseSensitiveCheck.checked ? Theme.accent : "transparent"
                                        border.color: caseSensitiveCheck.checked ? Theme.accent : Theme.panelBorder
                                        Image { anchors.centerIn: parent; width: 10; height: 10; source: "../assets/icons/select-all.svg"; visible: caseSensitiveCheck.checked; layer.enabled: true; layer.effect: MultiEffect { colorization: 1.0; colorizationColor: "white" } }
                                    }
                                    contentItem: Label {
                                        text: caseSensitiveCheck.text
                                        font.pixelSize: Theme.fontSizeLabel
                                        color: Theme.textPrimary
                                        leftPadding: 24
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                                CheckBox {
                                    id: regexCheck; text: "Regex"; onCheckedChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel
                                    indicator: Rectangle {
                                        implicitWidth: 18; implicitHeight: 18; radius: Theme.radiusSm
                                        color: regexCheck.checked ? Theme.accent : "transparent"
                                        border.color: regexCheck.checked ? Theme.accent : Theme.panelBorder
                                        Image { anchors.centerIn: parent; width: 10; height: 10; source: "../assets/icons/select-all.svg"; visible: regexCheck.checked; layer.enabled: true; layer.effect: MultiEffect { colorization: 1.0; colorizationColor: "white" } }
                                    }
                                    contentItem: Label {
                                        text: regexCheck.text
                                        font.pixelSize: Theme.fontSizeLabel
                                        color: Theme.textPrimary
                                        leftPadding: 24
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                        
                        // 1: Format
                        ColumnLayout {
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Prefix"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                TextField {
                                    id: prefixField; placeholderText: "Add to start..."; Layout.fillWidth: true
                                    onTextChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { color: Theme.panelSurfaceSoft; radius: Theme.radiusSm; border.color: prefixField.activeFocus ? Theme.accent : Theme.panelBorder; border.width: prefixField.activeFocus ? 2 : 1 }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Suffix"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                TextField {
                                    id: suffixField; placeholderText: "Add to end..."; Layout.fillWidth: true
                                    onTextChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel; leftPadding: 10
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
                                    Label { text: "Start Index"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                    ThemedSpinBox { id: startValue; from: 0; to: 999999; onValueChanged: editorChanged() }
                                }
                                ColumnLayout {
                                    Label { text: "Digits"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                    ThemedSpinBox { id: paddingValue; from: 1; to: 10; value: 2; onValueChanged: editorChanged() }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Position"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                ThemedComboBox { id: numPosCombo; Layout.fillWidth: true; model: ["Suffix", "Prefix"]; onCurrentIndexChanged: editorChanged() }
                            }
                        }

                        // 3: Sequence
                        ColumnLayout {
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Base Name"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                TextField {
                                    id: seqBaseNameField; placeholderText: "e.g. Photo_"; Layout.fillWidth: true
                                    onTextChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { color: Theme.panelSurfaceSoft; radius: Theme.radiusSm; border.color: seqBaseNameField.activeFocus ? Theme.accent : Theme.panelBorder; border.width: seqBaseNameField.activeFocus ? 2 : 1 }
                                }
                            }
                            RowLayout {
                                spacing: 12
                                ColumnLayout {
                                    Label { text: "Start At"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                    ThemedSpinBox { id: seqStartValue; from: 0; to: 999999; value: 1; onValueChanged: editorChanged() }
                                }
                                ColumnLayout {
                                    Label { text: "Digits"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                    ThemedSpinBox { id: seqPaddingValue; from: 1; to: 10; value: 2; onValueChanged: editorChanged() }
                                }
                            }
                        }

                        // 4: Transform
                        ColumnLayout {
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Transform"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                ThemedComboBox {
                                    id: transformModeCombo
                                    Layout.fillWidth: true
                                    model: ["lowercase", "UPPERCASE", "Title Case", "Trim whitespace", "Collapse spaces", "Spaces to underscores", "Spaces to dashes", "Remove special chars"]
                                    onCurrentIndexChanged: editorChanged()
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
                    
                    Label { text: "STATUS"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro; font.bold: true; font.letterSpacing: 1 }
                    
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
                                    font.pixelSize: Theme.fontSizeSubtitle; font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.alignment: Qt.AlignHCenter
                                }
                                Label {
                                    text: root.successCount + " of " + (root.successCount + root.failCount) + " files renamed"
                                    font.pixelSize: Theme.fontSizeLabel; color: Theme.textSecondary; Layout.alignment: Qt.AlignHCenter
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
                            font.pixelSize: Theme.fontSizeCaption; color: Theme.danger; Layout.fillWidth: true; wrapMode: Text.WordWrap
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
                        font.pixelSize: Theme.fontSizeLabel
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
                        font.bold: true; color: Theme.categoryAction; font.pixelSize: Theme.fontSizeMicro; font.letterSpacing: 1
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: internalPreviewModel.count + " files"
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro; font.bold: true
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
                                spacing: 8
                                Label {
                                    text: "Was:"
                                    font.pixelSize: Theme.fontSizeCaption; font.weight: Font.Medium
                                    color: Theme.textSecondary
                                    Layout.preferredWidth: Math.max(40, implicitWidth)
                                }
                                Label {
                                    text: model.oldName
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textSecondary
                                }
                            }
                            
                            RowLayout {
                                spacing: 8
                                Label {
                                    text: "New:"
                                    font.pixelSize: Theme.fontSizeLabel; font.weight: Font.DemiBold
                                    color: {
                                        if (root.isApplied) {
                                            return model.success ? Theme.success : Theme.danger
                                        }
                                        if (model.hasConflict) return Theme.danger
                                        return model.newName !== model.oldName ? Theme.accent : Theme.textPrimary
                                    }
                                    Layout.preferredWidth: Math.max(40, implicitWidth)
                                }
                                Label {
                                    text: model.newName
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    font.pixelSize: Theme.fontSizeLabel; font.weight: Font.DemiBold
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
                                font.pixelSize: Theme.fontSizeMicro
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
