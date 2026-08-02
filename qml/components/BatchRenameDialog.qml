import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"
import "common"
import "framework"
import "dialogs"

Dialog {
    id: root

    property var backdropSource: null

    property var controller: null
    property var appRoot: null
    property var sourcePaths: []
    property bool suppressPreviewUpdates: false
    readonly property bool hasConflicts: renameSession.hasConflicts
    readonly property bool isApplied: renameSession.isApplied
    readonly property int successCount: renameSession.successCount
    readonly property int failCount: renameSession.failCount
    readonly property bool isApplying: root.controller ? root.controller.batchRenameInProgress : false
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
        translucent: typeof appSettings !== "undefined" && appSettings
                     ? appSettings.workspaceDialogsTransparency
                     : false
        backdropSource: root.backdropSource
        backdropTransformItem: root
        accentColor: Theme.categoryAction
        shellBorderColor: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.28 : 0.20)
    }

    component ThemedComboBox : FmComboBox {
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeLabel
    }

    component ThemedSpinBox : FmSpinBox {
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeLabel
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/rename.svg"
        iconTint: Theme.categoryAction
        accentColor: Theme.categoryAction
        title: root.isApplied ? "Batch Rename Status" : (root.isApplying ? "Renaming Files" : "Batch Rename")
        subtitle: root.isApplying
                  ? (root.controller.batchRenameCompletedCount + " of " + root.controller.batchRenameTotalCount + " items processed")
                  : root.isApplied
                  ? (root.successCount + " of " + (root.successCount + root.failCount) + " items renamed successfully")
                  : (root.sourcePaths.length + " items selected")
        closeText: "x"
        onCloseRequested: {
            if (!root.isApplying) root.isApplied ? root.accept() : root.reject()
        }
    }

    footer: DialogFooter {
        Item {
            Layout.fillWidth: true
        }

        DialogActionButton {
            text: "Cancel"
            visible: !root.isApplied
            enabled: !root.isApplying
            highlighted: false
            onClicked: root.reject()
        }

        DialogActionButton {
            text: root.isApplied ? "Close" : (root.isApplying ? "Renaming..." : "Apply Changes")
            enabled: root.isApplied || (!root.isApplying && !root.hasConflicts && renameSession.totalCount > 0)
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

    BatchRenameSession {
        id: renameSession
    }

    Connections {
        target: root.controller
        function onBatchRenameFinished(results) {
            renameSession.applyResults(results)
        }
    }

    onOpened: {
        Qt.callLater(() => contentItem.forceActiveFocus())
        filterInput.text = ""
        renameSession.reset(root.sourcePaths)
        loadRuleToEditor(renameSession.selectedRuleIndex)
    }

    onClosed: {
        resetEditorState()
        if (root.appRoot && root.appRoot.finishRenamePreviewSuppression) {
            root.appRoot.finishRenamePreviewSuppression(true)
        }
    }

    function resetEditorState() {
        root.suppressPreviewUpdates = true
        filterInput.text = ""
        renameSession.reset([])
        root.suppressPreviewUpdates = false
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

    function selectRule(index) {
        if (index < 0 || index >= renameSession.ruleModel.count) return
        renameSession.selectedRuleIndex = index
        loadRuleToEditor(index)
    }

    function addRule(type) {
        saveSelectedRule()
        const index = renameSession.addRule(type || ruleTypeFromIndex(ruleTypeCombo.currentIndex))
        loadRuleToEditor(index)
    }

    function removeSelectedRule() {
        if (renameSession.removeSelectedRule()) {
            loadRuleToEditor(renameSession.selectedRuleIndex)
        }
    }

    function loadRuleToEditor(index) {
        if (index < 0 || index >= renameSession.ruleModel.count) return
        root.suppressPreviewUpdates = true
        const rule = renameSession.ruleModel.get(index)
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
        if (root.suppressPreviewUpdates) return
        renameSession.updateSelectedRule({
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
    }

    function applyChanges() {
        if (!controller || root.isApplying) return
        saveSelectedRule()
        renameSession.regeneratePreview()
        controller.startBatchRename(renameSession.sourcePaths, renameSession.engineRules())
    }

    contentItem: RowLayout {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        spacing: 0
        clip: true
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                if (!root.isApplying) root.reject()
                event.accepted = true
            } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                if (root.isApplied) {
                    root.accept()
                } else if (!root.hasConflicts && renameSession.totalCount > 0) {
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
            ScrollBar.vertical: FmScrollBar {
                id: rulesScrollBar
                parent: leftScroll.contentItem
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                policy: ScrollBar.AsNeeded
            }
            
            ColumnLayout {
                width: rulesScrollBar.scrollNeeded
                       ? Math.max(0, rulesScrollBar.x - 6)
                       : leftScroll.availableWidth
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
                            model: renameSession.ruleModel
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                height: 46
                                radius: Theme.radiusSm
                                color: index === renameSession.selectedRuleIndex
                                       ? Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.18 : 0.10)
                                       : Theme.panelSurfaceSoft
                                border.color: index === renameSession.selectedRuleIndex
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
                                            text: (index + 1) + ". " + model.title
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeLabel
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: model.summary
                                            color: Theme.textSecondary
                                            font.pixelSize: Theme.fontSizeCaption
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    FmIconButton {
                                        enabled: renameSession.ruleModel.count > 1
                                        Layout.preferredWidth: 26
                                        Layout.preferredHeight: 26
                                        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/delete.svg"
                                        iconSize: 12
                                        svgRecolorColor: enabled ? Theme.danger : Theme.withAlpha(Theme.textSecondary, 0.42)
                                        onClicked: {
                                            root.selectRule(index)
                                            root.removeSelectedRule()
                                        }
                                    }
                                }
                            }
                        }

                        FmButton {
                            Layout.fillWidth: true
                            implicitHeight: 32
                            text: "+ Add Rule"
                            secondaryTextColor: Theme.categoryAction
                            primaryColor: Theme.categoryAction
                            onClicked: root.addRule()
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
                                FmTextField {
                                    id: findField; placeholderText: "Text to find..."; Layout.fillWidth: true
                                    onTextChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Replace with"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                FmTextField {
                                    id: replaceField; placeholderText: "Replacement..."; Layout.fillWidth: true
                                    onTextChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                }
                            }
                            Flow {
                                Layout.fillWidth: true
                                Layout.preferredHeight: implicitHeight
                                spacing: 12
                                FmCheckBox {
                                    id: caseSensitiveCheck; text: "Case sensitive"; onCheckedChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel
                                }
                                FmCheckBox {
                                    id: regexCheck; text: "Regex"; onCheckedChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel
                                }
                            }
                        }
                        
                        // 1: Format
                        ColumnLayout {
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Prefix"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                FmTextField {
                                    id: prefixField; placeholderText: "Add to start..."; Layout.fillWidth: true
                                    onTextChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: "Suffix"; font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary }
                                FmTextField {
                                    id: suffixField; placeholderText: "Add to end..."; Layout.fillWidth: true
                                    onTextChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
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
                                FmTextField {
                                    id: seqBaseNameField; placeholderText: "e.g. Photo_"; Layout.fillWidth: true
                                    onTextChanged: editorChanged(); font.pixelSize: Theme.fontSizeLabel; leftPadding: 10
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
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
                                    source: root.failCount === 0 ? "../assets/icons-classic/select-all.svg" : "../assets/icons-classic/info.svg"
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
                            source: "../assets/icons-classic/info.svg"
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
                        sourcePath: "../assets/icons-classic/search.svg"
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
                            renameSession.filterText = text
                        }
                    }
                    
                    FmIconButton {
                        visible: filterInput.text.length > 0
                        Layout.preferredWidth: 24; Layout.preferredHeight: 24
                        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/delete.svg"
                        iconSize: 12
                        svgRecolorColor: Theme.textSecondary
                        onClicked: filterInput.text = ""
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
                        text: renameSession.previewModel.count + " files"
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
                model: renameSession.previewModel
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                
                ScrollBar.vertical: FmScrollBar {
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
                                source: model.success ? "../assets/icons-classic/select-all.svg" : "../assets/icons-classic/info.svg"
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
                                source: "../assets/icons-classic/info.svg"
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
