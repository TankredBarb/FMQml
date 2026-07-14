import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "dialogs"

Dialog {
    id: root
    modal: true
    focus: true
    width: Math.min(parent ? parent.width - 32 : 1144, 1144)
    height: Math.min(parent ? parent.height - 36 : 680, 680)
    padding: 0

    property string leftPath: ""
    property string rightPath: ""
    property var appRoot: null
    property bool recursive: false
    property bool includeHidden: false
    property bool compareContents: false
    property bool showEqual: true
    property int planMode: 3
    property bool restoringPreferences: false
    readonly property bool canCompare: folderCompareController && folderCompareController.canCompare(leftPath, rightPath)
    readonly property color dialogAccent: Theme.categoryAction
    readonly property color leftTone: Theme.categoryNavigation
    readonly property color rightTone: Theme.categoryUtility
    readonly property real dateColumnWidth: 146
    readonly property real sizeColumnWidth: 62
    readonly property real rowActionsWidth: 53

    function preferenceBool(value, fallbackValue) {
        if (value === undefined || value === null) return fallbackValue
        if (typeof value === "boolean") return value
        if (typeof value === "number") return value !== 0
        const normalized = String(value).toLowerCase()
        return normalized === "true" || normalized === "1"
    }

    function openFor(left, right) {
        leftPath = left || ""
        rightPath = right || ""
        const preferences = appSettings ? appSettings.folderComparePreferences() : ({})
        restoringPreferences = true
        recursive = preferenceBool(preferences["recursive"], false)
        includeHidden = preferenceBool(preferences["includeHidden"], false)
        compareContents = preferenceBool(preferences["strictContent"], false)
        showEqual = preferenceBool(preferences["showEqual"], true)
        planMode = Math.max(1, Math.min(5, Number(preferences["planMode"]) || 3))
        if (folderCompareController) {
            folderCompareController.clear()
            folderCompareController.setShowEqual(showEqual)
            folderCompareController.resultsModel.setFilterMode(Number(preferences["filterMode"]) || 0)
            folderCompareController.resultsModel.setSortMode(Number(preferences["sortMode"]) || 0)
        }
        restoringPreferences = false
        open()
    }

    function savePreferences() {
        if (restoringPreferences || !appSettings || !folderCompareController) return
        appSettings.saveFolderComparePreferences({
            recursive: recursive,
            includeHidden: includeHidden,
            strictContent: compareContents,
            showEqual: showEqual,
            filterMode: folderCompareController.resultsModel.filterMode,
            sortMode: folderCompareController.resultsModel.sortMode,
            planMode: planMode
        })
    }

    onClosed: savePreferences()

    function runCompare() {
        if (canCompare) folderCompareController.compare(leftPath, rightPath, recursive, includeHidden, compareContents)
    }

    function stateColor(state) {
        if (state === 0 || state === 1) return Theme.success
        if (state >= 8 && state <= 12) return Theme.warning
        return Theme.categoryAction
    }

    function planColor(action) {
        if (action === 1) return root.leftTone
        if (action === 2) return root.rightTone
        if (action === 3) return Theme.warning
        return Theme.textSecondary
    }

    function formatPlanBytes(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
    }

    function openResult(path, isDirectory, panel) {
        if (!workspaceController || !path) return
        const controller = panel === "left" ? workspaceController.leftPanel : workspaceController.rightPanel
        if (controller && controller.openSearchResult && controller.openSearchResult(path, isDirectory)) close()
    }

    function previewResult(path) {
        if (path && appRoot && appRoot.openTransientQuickLookPath) appRoot.openTransientQuickLookPath(path)
    }

    onOpened: {
        if (parent) {
            x = Math.round((parent.width - width) / 2)
            y = Math.round((parent.height - height) / 2)
        }
    }

    background: DialogShell { accentColor: root.dialogAccent; shellBorderColor: Theme.panelBorder }
    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/columns-2.svg"
        iconTint: root.dialogAccent
        accentColor: root.dialogAccent
        title: "Folder comparison"
        subtitle: "Compare, review the plan, then synchronize"
        onCloseRequested: root.close()
    }

    contentItem: Item {
        implicitWidth: 930
        implicitHeight: 470

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                PathHeader { Layout.fillWidth: true; sideName: "LEFT"; path: root.leftPath; tone: root.leftTone }
                Label { Layout.preferredWidth: 150; text: "COMPARISON"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
                PathHeader { Layout.fillWidth: true; sideName: "RIGHT"; path: root.rightPath; tone: root.rightTone }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                CompareCheckBox { text: "Recursive"; checked: root.recursive; onToggled: { root.recursive = checked; root.savePreferences() } }
                CompareCheckBox { text: "Include hidden"; checked: root.includeHidden; onToggled: { root.includeHidden = checked; root.savePreferences() } }
                CompareCheckBox { text: "Strict content"; checked: root.compareContents; onToggled: { root.compareContents = checked; root.savePreferences() } }
                CompareCheckBox { text: "Show equal"; checked: root.showEqual; onToggled: { root.showEqual = checked; if (folderCompareController) folderCompareController.setShowEqual(checked); root.savePreferences() } }
                Item { Layout.fillWidth: true }
                RowLayout {
                    Layout.preferredWidth: 112
                    spacing: 6
                    Item {
                        id: compareSpinner
                        readonly property bool running: Boolean(folderCompareController && folderCompareController.busy)
                        visible: running
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: width / 2
                            color: "transparent"
                            border.width: 2
                            border.color: Theme.withAlpha(root.dialogAccent, 0.28)
                        }
                        Rectangle {
                            width: 5
                            height: 5
                            radius: 2.5
                            color: root.dialogAccent
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                        }
                        RotationAnimator on rotation {
                            from: 0
                            to: 360
                            duration: 760
                            loops: Animation.Infinite
                            running: compareSpinner.running
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: folderCompareController && folderCompareController.busy
                              ? "Comparing…"
                              : (folderCompareController ? folderCompareController.resultsModel.count + " rows" : "")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeCaption
                        elide: Text.ElideRight
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                ViewCombo {
                    model: ["All states", "Equal", "One-sided", "Different", "Conflicts", "One-sided + different"]
                    currentIndex: folderCompareController ? folderCompareController.resultsModel.filterMode : 0
                    onActivated: index => { folderCompareController.resultsModel.setFilterMode(index); root.savePreferences() }
                }
                ViewCombo {
                    model: ["Sort: name", "Sort: status", "Sort: left date", "Sort: right date"]
                    currentIndex: folderCompareController ? folderCompareController.resultsModel.sortMode : 0
                    onActivated: index => { folderCompareController.resultsModel.setSortMode(index); root.savePreferences() }
                }
                Item { Layout.fillWidth: true }
                PlanCombo {
                    currentIndex: root.planMode - 1
                    enabled: !(folderCompareController && folderCompareController.executing)
                    onActivated: index => {
                        root.planMode = index + 1
                        root.savePreferences()
                        if (folderCompareController && folderCompareController.planReady)
                            folderCompareController.buildPlan(root.planMode)
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.panelBorder }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                SideHeader { Layout.fillWidth: true }
                ListHeader { Layout.preferredWidth: 150; text: "STATUS"; horizontalAlignment: Text.AlignHCenter }
                SideHeader { Layout.fillWidth: true; mirrored: true }
            }

            ListView {
                id: resultsList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 1
                model: folderCompareController ? folderCompareController.resultsModel : null
                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    required property string relativePath
                    required property int state
                    required property string stateText
                    required property string leftPath
                    required property string rightPath
                    required property string leftSize
                    required property string rightSize
                    required property string leftModified
                    required property string rightModified
                    required property bool leftDirectory
                    required property bool rightDirectory
                    required property int plannedAction
                    required property string plannedActionText
                    required property string executionError
                    required property int index
                    width: resultsList.width
                    height: 38

                    RowLayout {
                        anchors.fill: parent
                        spacing: 8
                        SideCell {
                            Layout.fillWidth: true
                            exists: leftPath.length > 0
                            name: relativePath
                            sizeText: leftSize
                            modifiedText: leftModified
                            sideTone: root.leftTone
                            openText: "Reveal left"
                            onOpenRequested: root.openResult(leftPath, leftDirectory, "left")
                            onPreviewRequested: root.previewResult(leftPath)
                        }
                        Rectangle {
                            Layout.preferredWidth: 150
                            Layout.fillHeight: true
                            radius: Theme.radiusSm
                            color: Theme.withAlpha(root.stateColor(state), themeController.isDark ? 0.14 : 0.09)
                            RowLayout { anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 5
                                Rectangle { Layout.preferredWidth: 4; Layout.preferredHeight: 15; radius: 2; color: root.stateColor(state) }
                                ColumnLayout { Layout.fillWidth: true; spacing: 0
                                    Label { Layout.fillWidth: true; text: stateText; color: root.stateColor(state); font.pixelSize: Theme.fontSizeCaption; font.weight: Font.DemiBold; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter }
                                    Label { Layout.fillWidth: true; visible: plannedActionText.length > 0; text: plannedActionText; color: root.planColor(plannedAction); font.pixelSize: Theme.fontSizeMicro; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter }
                                    Label { Layout.fillWidth: true; visible: executionError.length > 0; text: executionError; color: Theme.warning; font.pixelSize: Theme.fontSizeMicro; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter }
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: folderCompareController && folderCompareController.planReady
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: folderCompareController.resultsModel.cyclePlannedAction(index)
                            }
                        }
                        SideCell {
                            Layout.fillWidth: true
                            exists: rightPath.length > 0
                            name: relativePath
                            sizeText: rightSize
                            modifiedText: rightModified
                            sideTone: root.rightTone
                            openText: "Reveal right"
                            mirrored: true
                            onOpenRequested: root.openResult(rightPath, rightDirectory, "right")
                            onPreviewRequested: root.previewResult(rightPath)
                        }
                    }
                }

                Label { anchors.centerIn: parent; visible: resultsList.count === 0 && !(folderCompareController && folderCompareController.busy); text: "Choose two local folders, then press Compare"; color: Theme.textSecondary }
            }

            Label { visible: folderCompareController && folderCompareController.error.length > 0; Layout.fillWidth: true; text: folderCompareController ? folderCompareController.error : ""; color: Theme.warning; wrapMode: Text.Wrap }
            Label {
                visible: folderCompareController && folderCompareController.executionSummary.length > 0
                Layout.fillWidth: true
                text: folderCompareController ? folderCompareController.executionSummary : ""
                color: folderCompareController && folderCompareController.executionSucceeded ? Theme.success : Theme.warning
                font.pixelSize: Theme.fontSizeCaption
                wrapMode: Text.Wrap
            }
        }
    }

    footer: DialogFooter {
        Label {
            Layout.fillWidth: true
            text: !folderCompareController ? "" : folderCompareController.planReady
                  ? folderCompareController.resultsModel.plannedCount + " planned actions  •  " + root.formatPlanBytes(folderCompareController.resultsModel.plannedBytes) + "  •  " + folderCompareController.resultsModel.unresolvedCount + " unresolved  •  " + folderCompareController.resultsModel.changedAfterCompareCount + " changed"
                  : folderCompareController.resultsModel.equalCount + " equal  •  " + folderCompareController.resultsModel.oneSidedCount + " one-sided  •  " + folderCompareController.resultsModel.differentCount + " different"
            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption; elide: Text.ElideRight
        }
        DialogActionButton {
            visible: folderCompareController && (folderCompareController.busy || folderCompareController.executing)
            text: "Cancel"
            onClicked: folderCompareController.executing
                       ? folderCompareController.cancelExecution()
                       : folderCompareController.cancel()
        }
        DialogActionButton { visible: folderCompareController && folderCompareController.planReady; text: "Revalidate"; onClicked: folderCompareController.revalidatePlan() }
        DialogActionButton { visible: folderCompareController && folderCompareController.planReady; text: "Clear plan"; onClicked: folderCompareController.clearPlan() }
        DialogActionButton { text: "Close"; onClicked: root.close() }
        DialogActionButton { visible: folderCompareController && folderCompareController.resultsModel.count > 0 && !folderCompareController.planReady; text: "Build plan"; onClicked: folderCompareController.buildPlan(root.planMode) }
        DialogActionButton {
            visible: folderCompareController && folderCompareController.planReady
            text: folderCompareController && folderCompareController.executing ? "Synchronizing…" : "Synchronize"
            highlighted: true
            primaryColor: Theme.success
            enabled: folderCompareController && !folderCompareController.executing
                     && folderCompareController.resultsModel.plannedCount > 0
                     && folderCompareController.resultsModel.unresolvedCount === 0
                     && folderCompareController.resultsModel.changedAfterCompareCount === 0
                     && workspaceController && !workspaceController.operationQueue.busy
            onClicked: folderCompareController.executePlan()
            ToolTip.visible: hovered && !enabled
            ToolTip.delay: 350
            ToolTip.text: folderCompareController && folderCompareController.resultsModel.unresolvedCount > 0
                          ? "Click each unresolved item to skip it"
                          : "Revalidate the plan before synchronizing"
        }
        DialogActionButton { text: "Compare"; highlighted: !(folderCompareController && folderCompareController.planReady); primaryColor: root.dialogAccent; enabled: root.canCompare && !(folderCompareController && (folderCompareController.busy || folderCompareController.executing)); onClicked: root.runCompare() }
    }

    component PathHeader: Rectangle {
        property string sideName: ""
        property string path: ""
        property color tone: Theme.categoryAction
        Layout.preferredHeight: 40
        radius: Theme.radiusSm
        color: Theme.withAlpha(tone, themeController.isDark ? 0.12 : 0.07)
        border.color: Theme.withAlpha(tone, themeController.isDark ? 0.42 : 0.30)
        border.width: 1
        ColumnLayout { anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; spacing: 0
            Label { text: parent.parent.sideName; color: parent.parent.tone; font.pixelSize: Theme.fontSizeCaption; font.weight: Font.DemiBold }
            Label { Layout.fillWidth: true; text: workspaceController ? workspaceController.displayPath(parent.parent.path) : parent.parent.path; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeCaption; elide: Text.ElideMiddle }
        }
    }
    component ListHeader: Label { color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption; font.weight: Font.DemiBold; elide: Text.ElideRight }
    component SideHeader: Item {
        property bool mirrored: false
        Layout.preferredHeight: 20
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 7
            anchors.rightMargin: 7
            spacing: 5
            ListHeader { Layout.fillWidth: true; visible: !parent.parent.mirrored; text: "NAME" }
            ListHeader { Layout.preferredWidth: root.dateColumnWidth; visible: !parent.parent.mirrored; text: "MODIFIED"; horizontalAlignment: Text.AlignRight }
            ListHeader { Layout.preferredWidth: root.sizeColumnWidth; text: "SIZE"; horizontalAlignment: parent.parent.mirrored ? Text.AlignLeft : Text.AlignRight }
            Item { Layout.preferredWidth: root.rowActionsWidth }
            ListHeader { Layout.preferredWidth: root.dateColumnWidth; visible: parent.parent.mirrored; text: "MODIFIED"; horizontalAlignment: Text.AlignLeft }
            ListHeader { Layout.fillWidth: true; visible: parent.parent.mirrored; text: "NAME"; horizontalAlignment: Text.AlignRight }
        }
    }
    component CompareCheckBox: CheckBox {
        id: checkControl
        spacing: 7
        indicator: Rectangle {
            implicitWidth: 18
            implicitHeight: 18
            x: checkControl.leftPadding
            y: Math.round((checkControl.height - height) / 2)
            radius: Theme.radiusSm
            color: checkControl.checked ? root.dialogAccent : "transparent"
            border.color: checkControl.checked ? root.dialogAccent : Theme.panelBorder
            border.width: 1
            Image {
                anchors.centerIn: parent
                width: 10
                height: 10
                source: "../assets/icons/select-all.svg"
                visible: checkControl.checked
                layer.enabled: true
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: Theme.readableOn(root.dialogAccent, Theme.textPrimary)
                }
            }
        }
        contentItem: Label {
            text: checkControl.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            color: checkControl.enabled ? Theme.textPrimary : Theme.textSecondary
            leftPadding: checkControl.indicator.width + checkControl.spacing
            verticalAlignment: Text.AlignVCenter
        }
    }
    component PlanCombo: ComboBox {
        id: planCombo
        model: ["Update left from right", "Update right from left", "Two-way newest",
                "Copy missing to left", "Copy missing to right"]
        Layout.preferredWidth: 180
        Layout.preferredHeight: 30
        leftPadding: 9
        rightPadding: 24
        contentItem: Label { text: planCombo.displayText; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeCaption; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        background: Rectangle { radius: Theme.radiusSm; color: planCombo.hovered ? Theme.surfaceActive : Theme.panelSurfaceSoft; border.color: planCombo.activeFocus ? root.dialogAccent : Theme.panelBorder; border.width: 1 }
        indicator: Label { x: planCombo.width - width - 8; anchors.verticalCenter: parent.verticalCenter; text: "▾"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
        delegate: ItemDelegate {
            width: planCombo.popup.availableWidth
            text: modelData
            highlighted: planCombo.highlightedIndex === index
            contentItem: Label { text: parent.text; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeCaption; leftPadding: 7; rightPadding: 7; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            background: Rectangle { radius: Theme.radiusSm; color: parent.highlighted ? Theme.surfaceActive : "transparent" }
        }
        popup: Popup {
            y: planCombo.height + 4
            width: planCombo.width
            padding: 5
            contentItem: ListView { clip: true; implicitHeight: contentHeight; model: planCombo.popup.visible ? planCombo.delegateModel : null; currentIndex: planCombo.highlightedIndex }
            background: Rectangle { radius: Theme.radiusMd; color: Theme.panelSurfaceStrong; border.color: Theme.panelBorder; border.width: 1 }
        }
    }
    component ViewCombo: ComboBox {
        id: viewCombo
        Layout.preferredWidth: 116
        Layout.preferredHeight: 28
        leftPadding: 8
        rightPadding: 20
        contentItem: Label { text: viewCombo.displayText; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeCaption; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        background: Rectangle { radius: Theme.radiusSm; color: viewCombo.hovered ? Theme.surfaceActive : Theme.panelSurfaceSoft; border.color: viewCombo.activeFocus ? root.dialogAccent : Theme.panelBorder; border.width: 1 }
        indicator: Label { x: viewCombo.width - width - 7; anchors.verticalCenter: parent.verticalCenter; text: "▾"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
        delegate: ItemDelegate {
            width: viewCombo.popup.availableWidth
            text: modelData
            highlighted: viewCombo.highlightedIndex === index
            contentItem: Label { text: parent.text; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeCaption; leftPadding: 7; rightPadding: 7; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            background: Rectangle { radius: Theme.radiusSm; color: parent.highlighted ? Theme.surfaceActive : "transparent" }
        }
        popup: Popup {
            y: viewCombo.height + 4
            width: Math.max(viewCombo.width, 140)
            padding: 5
            contentItem: ListView { clip: true; implicitHeight: contentHeight; model: viewCombo.popup.visible ? viewCombo.delegateModel : null; currentIndex: viewCombo.highlightedIndex }
            background: Rectangle { radius: Theme.radiusMd; color: Theme.panelSurfaceStrong; border.color: Theme.panelBorder; border.width: 1 }
        }
    }
    component SideCell: Rectangle {
        property bool exists: false
        property bool mirrored: false
        property string name: ""
        property string sizeText: ""
        property string modifiedText: ""
        property string openText: "Open"
        property color sideTone: Theme.categoryAction
        signal openRequested()
        signal previewRequested()
        Layout.fillHeight: true
        radius: Theme.radiusSm
        color: exists
               ? Theme.withAlpha(sideTone, sideHover.hovered
                                 ? (themeController.isDark ? 0.18 : 0.11)
                                 : (themeController.isDark ? 0.07 : 0.035))
               : Theme.withAlpha(sideTone, themeController.isDark ? 0.09 : 0.05)
        border.color: Theme.withAlpha(sideTone, sideHover.hovered ? 0.34 : 0.12)
        border.width: 1
        HoverHandler { id: sideHover }
        RowLayout { anchors.fill: parent; anchors.leftMargin: 7; anchors.rightMargin: 7; spacing: 5
            Label { Layout.fillWidth: true; visible: !mirrored; text: exists ? name : "—"; color: exists ? Theme.textPrimary : Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall; elide: Text.ElideMiddle }
            Label { Layout.preferredWidth: root.dateColumnWidth; visible: !mirrored; text: exists ? modifiedText : ""; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMini; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
            Label { Layout.preferredWidth: root.sizeColumnWidth; text: exists ? sizeText : "Missing"; color: exists ? Theme.textSecondary : sideTone; font.pixelSize: Theme.fontSizeMini; font.weight: exists ? Font.Normal : Font.DemiBold; horizontalAlignment: mirrored ? Text.AlignLeft : Text.AlignRight; elide: Text.ElideRight }
            Button { visible: exists; text: "◉"; flat: true; Layout.preferredWidth: 24; Layout.preferredHeight: 26; onClicked: previewRequested(); ToolTip.visible: hovered; ToolTip.delay: 350; ToolTip.text: "Preview"
                contentItem: Label { text: parent.text; color: parent.hovered ? Theme.textPrimary : Theme.textSecondary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: Theme.radiusSm; color: parent.hovered ? Theme.surfaceActive : "transparent" } }
            Button { visible: exists; text: "↗"; flat: true; Layout.preferredWidth: 24; Layout.preferredHeight: 26; onClicked: openRequested(); ToolTip.visible: hovered; ToolTip.delay: 350; ToolTip.text: openText
                contentItem: Label { text: parent.text; color: parent.hovered ? Theme.textPrimary : Theme.textSecondary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: Theme.radiusSm; color: parent.hovered ? Theme.surfaceActive : "transparent" } }
            Label { Layout.preferredWidth: root.dateColumnWidth; visible: mirrored; text: exists ? modifiedText : ""; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMini; horizontalAlignment: Text.AlignLeft; elide: Text.ElideRight }
            Label { Layout.fillWidth: true; visible: mirrored; text: exists ? name : "—"; color: exists ? Theme.textPrimary : Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall; elide: Text.ElideMiddle; horizontalAlignment: Text.AlignRight }
        }
    }
}
