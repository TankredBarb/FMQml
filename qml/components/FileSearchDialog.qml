import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"
import "dialogs"
import "filepanel"

Dialog {
    id: root

    title: "File Search"
    modal: true
    focus: true
    width: Math.min(parent ? parent.width - 48 : 820, 820)
    height: Math.min(parent ? parent.height - 48 : 600, 600)
    padding: 0

    property var appRoot: null
    property real dragOriginX: 0
    property real dragOriginY: 0
    property string searchRootPath: ""
    property bool includeHidden: false
    property bool searchContents: false
    property bool caseSensitive: false
    property bool includeFolders: true
    property int matchMode: 0
    property bool cancelOnClose: true
    property bool returnedFromPanel: false
    readonly property bool searching: fileSearchController && fileSearchController.busy
    readonly property bool hasQuery: searchField.text.trim().length > 0
    readonly property bool hasResults: fileSearchController && fileSearchController.resultsModel.count > 0
    readonly property bool canSearchRoot: fileSearchController && fileSearchController.canSearchPath(root.searchRootPath)
    readonly property int skippedDetailCount: fileSearchController
                                             ? fileSearchController.skippedDetailEntries.length
                                             : 0
    readonly property color dialogAccent: Theme.accent

    signal resultOpened()
    signal searchContextReset()

    onOpened: {
        root.centerInParent()
        Qt.callLater(() => searchField.forceActiveFocus())
    }
    onWidthChanged: root.clampDialogPosition()
    onHeightChanged: root.clampDialogPosition()
    onClosed: {
        searchDebounceTimer.stop()
        if (fileSearchController) {
            fileSearchController.holdResultUpdates = false
        }
        if (root.cancelOnClose && fileSearchController) {
            fileSearchController.cancel()
        }
        root.cancelOnClose = true
    }
    onSearchingChanged: {
        if (fileSearchController) {
            fileSearchController.holdResultUpdates = root.searching
                    && (resultsList.moving || resultsList.flicking || resultsScrollBar.pressed)
        }
    }

    function openFor(path, includeHiddenFiles) {
        root.returnedFromPanel = false
        root.searchContextReset()
        root.searchRootPath = path || ""
        root.includeHidden = includeHiddenFiles === true
        root.searchContents = false
        root.caseSensitive = false
        root.includeFolders = true
        root.matchMode = 0
        root.cancelOnClose = true
        searchField.text = ""
        if (fileSearchController) {
            fileSearchController.clear()
        }
        root.open()
    }

    function centerInParent() {
        if (!parent) {
            return
        }
        root.setDialogPosition((parent.width - root.width) / 2,
                               (parent.height - root.height) / 2)
    }

    function clampDialogPosition() {
        if (!root.opened || !parent) {
            return
        }
        root.setDialogPosition(root.x, root.y)
    }

    function setDialogPosition(nextX, nextY) {
        if (!parent) {
            root.x = nextX
            root.y = nextY
            return
        }
        const margin = 8
        const maxX = Math.max(margin, parent.width - root.width - margin)
        const maxY = Math.max(margin, parent.height - root.height - margin)
        root.x = Math.max(margin, Math.min(maxX, nextX))
        root.y = Math.max(margin, Math.min(maxY, nextY))
    }

    function reopenResults() {
        root.returnedFromPanel = true
        root.cancelOnClose = true
        root.open()
        Qt.callLater(() => resultsList.forceActiveFocus())
    }

    function clearSearchResults() {
        searchDebounceTimer.stop()
        root.returnedFromPanel = false
        root.searchContextReset()
        searchField.text = ""
        if (fileSearchController) {
            fileSearchController.cancel()
            fileSearchController.clear()
        }
        searchField.forceActiveFocus()
    }

    function activePanelController() {
        return root.appRoot && root.appRoot.activePanelController
            ? root.appRoot.activePanelController()
            : null
    }

    function runSearchNow() {
        searchDebounceTimer.stop()
        if (!fileSearchController) {
            return
        }
        const query = searchField.text.trim()
        if (query.length === 0) {
            root.returnedFromPanel = false
            root.searchContextReset()
            fileSearchController.cancel()
            fileSearchController.clear()
            return
        }
        root.returnedFromPanel = false
        root.searchContextReset()
        fileSearchController.search(root.searchRootPath, query, root.includeHidden, root.searchContents, root.caseSensitive, root.matchMode, root.includeFolders)
    }

    function openResult(path, isDirectory) {
        const panel = activePanelController()
        if (!panel || !path || path.length === 0) {
            return
        }
        if (panel.openSearchResult(path, isDirectory)) {
            root.resultOpened()
            root.accept()
            Qt.callLater(() => {
                if (workspaceController) {
                    workspaceController.focusActivePanel()
                }
            })
        }
    }

    function copyPath(path) {
        if (!workspaceController || !path || path.length === 0) {
            return
        }
        workspaceController.copyTextToClipboard(workspaceController.displayPath(path))
        if (root.appRoot && root.appRoot.showTransientInfo) {
            root.appRoot.showTransientInfo("Path copied to clipboard")
        }
    }

    function resultCountText() {
        if (!fileSearchController || !root.hasQuery) {
            return "Enter a file or folder name"
        }
        const count = fileSearchController.resultsModel.count
        if (count === 0) {
            return root.searching ? "Searching" : "No matches"
        }
        return count + (count === 1 ? " match" : " matches")
    }

    function progressText() {
        if (!fileSearchController) {
            return ""
        }
        const scanned = fileSearchController.scannedFiles + fileSearchController.scannedFolders
        if (scanned <= 0) {
            return fileSearchController.coverageStatusText
        }
        let text = fileSearchController.scannedFiles + " files, "
                 + fileSearchController.scannedFolders + " folders scanned"
        if (root.searchContents) {
            text += " - contents: " + fileSearchController.contentFilesScanned + " text files checked"
            if (fileSearchController.contentFilesSkipped > 0) {
                text += ", " + fileSearchController.contentFilesSkipped + " skipped"
            }
        }
        return text + " - " + fileSearchController.coverageStatusText
    }

    Timer {
        id: searchDebounceTimer
        interval: 280
        repeat: false
        onTriggered: root.runSearchNow()
    }

    component SearchModeComboBox : ComboBox {
        id: combo

        delegate: ItemDelegate {
            width: combo.width
            height: 30
            highlighted: combo.highlightedIndex === index

            contentItem: Label {
                text: modelData
                color: highlighted ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                font.weight: highlighted ? Font.DemiBold : Font.Normal
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                leftPadding: 8
            }

            background: Rectangle {
                radius: 2
                color: !enabled ? "transparent"
                       : down ? Theme.menuItemPressed
                       : highlighted || hovered ? Theme.menuItemHover
                       : "transparent"
            }
        }

        indicator: RecolorSvgIcon {
            x: combo.width - width - 9
            y: Math.round((combo.height - height) / 2)
            width: 10
            height: 10
            sourcePath: "../assets/icons/arrow-up.svg"
            sourceSize: Qt.size(16, 16)
            recolorEnabled: true
            recolorColor: combo.enabled ? Theme.textSecondary : Theme.withAlpha(Theme.textSecondary, 0.45)
            rotation: combo.opened ? 0 : 180
            opacity: combo.enabled ? 0.72 : 0.42
        }

        contentItem: Label {
            leftPadding: 9
            rightPadding: 24
            text: combo.displayText
            color: combo.enabled ? Theme.textSecondary : Theme.withAlpha(Theme.textSecondary, 0.45)
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            implicitHeight: 28
            radius: Theme.radiusSm
            color: combo.opened ? Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.13 : 0.08)
                   : combo.hovered ? Theme.menuItemHover
                   : "transparent"
            border.color: combo.opened ? Theme.withAlpha(root.dialogAccent, 0.46) : Theme.withAlpha(Theme.menuBorder, 0.85)
            border.width: 1
        }

        popup: Popup {
            y: combo.height + 4
            width: combo.width
            padding: 3
            implicitHeight: contentItem.implicitHeight + 6
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                spacing: 0
                ScrollIndicator.vertical: ScrollIndicator {}
            }

            background: Rectangle {
                color: Theme.menuSurface
                radius: Theme.radiusSm
                border.color: Theme.menuBorder
                border.width: 1

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Theme.glassShadow
                    shadowBlur: 0.28
                    shadowOpacity: themeController.isDark ? 0.38 : 0.16
                    shadowVerticalOffset: 6
                }
            }
        }
    }

    component SearchToggle : Button {
        id: toggle

        property string toolTipText: ""

        checkable: true
        hoverEnabled: true
        leftPadding: 8
        rightPadding: 9
        topPadding: 0
        bottomPadding: 0
        implicitHeight: 28
        implicitWidth: toggleContent.implicitWidth + leftPadding + rightPadding

        ToolTip.visible: hovered && toolTipText.length > 0
        ToolTip.delay: 350
        ToolTip.text: toolTipText

        background: Rectangle {
            radius: Theme.radiusSm
            color: toggle.checked
                   ? Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.18 : 0.10)
                   : toggle.hovered ? Theme.menuItemHover
                   : "transparent"
            border.color: toggle.checked
                          ? Theme.withAlpha(root.dialogAccent, 0.46)
                          : Theme.withAlpha(Theme.menuBorder, 0.85)
            border.width: 1
        }

        contentItem: RowLayout {
            id: toggleContent

            spacing: 6

            Rectangle {
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
                radius: 4
                color: toggle.checked
                       ? Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.28 : 0.18)
                       : Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.62 : 0.78)
                border.color: toggle.checked
                              ? root.dialogAccent
                              : Theme.withAlpha(Theme.textSecondary, 0.45)
                border.width: 1

                Rectangle {
                    anchors.centerIn: parent
                    width: 6
                    height: 6
                    radius: 2
                    visible: toggle.checked
                    color: root.dialogAccent
                }
            }

            Label {
                text: toggle.text
                color: toggle.enabled ? Theme.textSecondary : Theme.withAlpha(Theme.textSecondary, 0.45)
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                font.weight: Font.DemiBold
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    component StopSearchButton : Button {
        id: stopButton

        hoverEnabled: true
        implicitWidth: 96
        implicitHeight: 34

        contentItem: Label {
            text: stopButton.text
            color: stopButton.enabled ? Theme.warning : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: stopButton.pressed
                   ? Theme.withAlpha(Theme.warning, themeController.isDark ? 0.24 : 0.14)
                   : stopButton.hovered
                     ? Theme.withAlpha(Theme.warning, themeController.isDark ? 0.16 : 0.09)
                     : Theme.withAlpha(Theme.warning, themeController.isDark ? 0.09 : 0.045)
            border.color: Theme.withAlpha(Theme.warning, stopButton.hovered || stopButton.pressed ? 0.72 : 0.46)
            border.width: 1
        }
    }

    background: DialogShell {
        accentColor: root.dialogAccent
        shellBorderColor: Theme.panelBorder
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/search.svg"
        iconTint: root.dialogAccent
        accentColor: root.dialogAccent
        title: root.title
        subtitle: fileSearchController && fileSearchController.displayRootPath.length > 0
                  ? fileSearchController.displayRootPath
                  : (workspaceController ? workspaceController.displayPath(root.searchRootPath) : root.searchRootPath)
        closeText: "x"
        onCloseRequested: root.accept()

        DragHandler {
            target: null
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (active) {
                    root.dragOriginX = root.x
                    root.dragOriginY = root.y
                }
            }
            onTranslationChanged: {
                if (active) {
                    root.setDialogPosition(root.dragOriginX + translation.x,
                                           root.dragOriginY + translation.y)
                }
            }
        }
    }

    footer: DialogFooter {
        Label {
            Layout.fillWidth: true
            text: root.progressText()
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            elide: Text.ElideRight
        }

        StopSearchButton {
            visible: root.searching
            text: "Cancel"
            onClicked: fileSearchController.cancel()
        }

        DialogActionButton {
            visible: root.returnedFromPanel
            text: "Clear Results"
            onClicked: root.clearSearchResults()
            ToolTip.visible: hovered
            ToolTip.delay: 350
            ToolTip.text: "Clears these results and hides the toolbar Search Results button."
        }

        DialogActionButton {
            text: "Close"
            highlighted: true
            primaryColor: root.dialogAccent
            onClicked: root.accept()
        }
    }

    Popup {
        id: skippedPopup

        width: Math.min(root.width - 44, 620)
        height: Math.min(root.height - 140, 300)
        x: Math.round((root.width - width) / 2)
        y: Math.round(root.height - height - 70)
        modal: false
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.panelSurface
            border.color: Theme.panelBorder
            border.width: 1

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 0.35
                shadowOpacity: themeController.isDark ? 0.42 : 0.18
                shadowVerticalOffset: 8
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: "Skipped paths"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: fileSearchController ? fileSearchController.skippedDetailEntries : []
                spacing: 4

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 32
                    radius: Theme.radiusSm
                    color: "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Label {
                            text: modelData.label
                            color: modelData.kind === "link" ? Theme.categoryInfo : Theme.warning
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                            font.weight: Font.DemiBold
                            Layout.preferredWidth: 80
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: modelData.path
                            color: Theme.textSecondary
                            font.family: "Consolas"
                            font.pixelSize: Theme.fontSizeCaption
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            radius: Theme.controlRadius
            color: Theme.panelSurfaceSoft
            border.width: 1
            border.color: searchField.activeFocus ? Theme.withAlpha(root.dialogAccent, 0.60) : Theme.withAlpha(Theme.border, 0.5)

            RecolorSvgIcon {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 15
                height: 15
                sourcePath: "qrc:/qt/qml/FM/qml/assets/toolbar-next/search.svg"
                sourceSize: Qt.size(16, 16)
                recolorEnabled: true
                recolorColor: root.dialogAccent
                opacity: 0.86
            }

            PremiumTextField {
                id: searchField
                anchors.fill: parent
                anchors.leftMargin: 36
                anchors.rightMargin: 12
                placeholderText: root.canSearchRoot ? "Search files and folders..." : "This location cannot be searched"
                enabled: root.canSearchRoot
                background: null
                onTextChanged: {
                    if (root.canSearchRoot) {
                        root.returnedFromPanel = false
                        root.searchContextReset()
                        searchDebounceTimer.restart()
                    }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Escape) {
                        root.reject()
                        event.accepted = true
                    } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                               && resultsList.currentIndex >= 0
                               && fileSearchController
                               && fileSearchController.resultsModel.count > 0) {
                        root.openResult(fileSearchController.resultsModel.pathAt(resultsList.currentIndex),
                                        fileSearchController.resultsModel.isDirectoryAt(resultsList.currentIndex))
                        event.accepted = true
                    } else if (event.key === Qt.Key_Down && fileSearchController && fileSearchController.resultsModel.count > 0) {
                        resultsList.forceActiveFocus()
                        resultsList.currentIndex = Math.max(0, resultsList.currentIndex)
                        event.accepted = true
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: root.resultCountText()
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            BusyIndicator {
                running: root.searching
                visible: root.searching
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
            }

            SearchModeComboBox {
                id: targetModeCombo

                Layout.preferredWidth: 132
                Layout.preferredHeight: 28
                model: ["Files + folders", "Files only"]
                currentIndex: root.includeFolders ? 0 : 1
                enabled: root.canSearchRoot
                onActivated: (index) => {
                    root.includeFolders = index === 0
                    if (root.hasQuery) {
                        root.returnedFromPanel = false
                        root.searchContextReset()
                        searchDebounceTimer.restart()
                    }
                }
            }

            SearchModeComboBox {
                id: matchModeCombo

                Layout.preferredWidth: 104
                Layout.preferredHeight: 28
                model: ["Contains", "Exact"]
                currentIndex: root.matchMode
                enabled: root.canSearchRoot
                onActivated: (index) => {
                    root.matchMode = index
                    if (root.hasQuery) {
                        root.returnedFromPanel = false
                        root.searchContextReset()
                        searchDebounceTimer.restart()
                    }
                }
            }

            SearchToggle {
                id: contentsToggle

                checked: root.searchContents
                text: "Contents"
                enabled: root.canSearchRoot
                toolTipText: "Search inside readable text files."
                onToggled: {
                    root.searchContents = checked
                    if (root.hasQuery) {
                        root.returnedFromPanel = false
                        root.searchContextReset()
                        searchDebounceTimer.restart()
                    }
                }
            }

            SearchToggle {
                id: caseToggle

                checked: root.caseSensitive
                text: "Match case"
                enabled: root.canSearchRoot
                toolTipText: checked ? "Search considers letter case." : "Search ignores letter case."
                onToggled: {
                    root.caseSensitive = checked
                    if (root.hasQuery) {
                        root.returnedFromPanel = false
                        root.searchContextReset()
                        searchDebounceTimer.restart()
                    }
                }
            }

            Button {
                id: skippedButton

                visible: root.skippedDetailCount > 0
                text: root.skippedDetailCount + " skipped"
                flat: true
                onClicked: skippedPopup.open()

                contentItem: Label {
                    text: parent.text
                    color: Theme.warning
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: skippedButton.hovered ? Theme.withAlpha(Theme.warning, themeController.isDark ? 0.14 : 0.08) : "transparent"
                    border.color: Theme.withAlpha(Theme.warning, 0.32)
                    border.width: 1
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: resultsList

                anchors.fill: parent
                clip: true
                spacing: 4
                model: fileSearchController ? fileSearchController.resultsModel : null
                cacheBuffer: Math.max(0, height * 2)
                reuseItems: true
                onMovingChanged: updateResultHold()
                onFlickingChanged: updateResultHold()

                function updateResultHold() {
                    if (fileSearchController) {
                        fileSearchController.holdResultUpdates = root.searching
                                && (moving || flicking || resultsScrollBar.pressed)
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    id: resultsScrollBar

                    policy: ScrollBar.AsNeeded
                    width: 10
                    active: hovered || resultsList.moving || resultsList.flicking
                    onPressedChanged: resultsList.updateResultHold()

                    background: Rectangle {
                        color: "transparent"
                    }

                    contentItem: Rectangle {
                        implicitWidth: 6
                        radius: 3
                        color: resultsScrollBar.pressed || resultsScrollBar.hovered
                               ? Theme.withAlpha(Theme.textSecondary, themeController.isDark ? 0.58 : 0.42)
                               : Theme.withAlpha(Theme.textSecondary, themeController.isDark ? 0.34 : 0.24)
                    }
                }
                onCountChanged: {
                    if (count <= 0) {
                        currentIndex = -1
                    } else if (currentIndex < 0 || currentIndex >= count) {
                        currentIndex = 0
                    }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Escape) {
                        root.reject()
                        event.accepted = true
                    } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                               && currentIndex >= 0
                               && currentItem) {
                        currentItem.openCurrent()
                        event.accepted = true
                    }
                }

                delegate: Rectangle {
                    id: row

                    required property int index
                    required property string path
                    required property string name
                    required property string displayPath
                    required property string displayParentPath
                    required property string sizeText
                    required property string modifiedText
                    required property bool isDirectory
                    required property string matchKind
                    required property int lineNumber
                    required property string lineText
                    required property int lineMatchStart
                    required property int lineMatchLength
                    readonly property int boundedMatchStart: Math.max(0, Math.min(lineMatchStart, lineText.length))
                    readonly property int boundedMatchLength: Math.max(0, Math.min(lineMatchLength,
                                                                                   lineText.length - boundedMatchStart))
                    readonly property string rawContentBefore: lineText.slice(0, boundedMatchStart)
                    readonly property string rawContentAfter: lineText.slice(boundedMatchStart + boundedMatchLength)
                    readonly property string contentBefore: rawContentBefore.trim()
                    readonly property string contentMatch: lineText.slice(boundedMatchStart,
                                                                          boundedMatchStart + boundedMatchLength)
                    readonly property string contentAfter: rawContentAfter.trim()
                    readonly property bool spaceBeforeMatch: /\s$/.test(rawContentBefore)
                    readonly property bool spaceAfterMatch: /^\s/.test(rawContentAfter)

                    width: ListView.view.width
                    height: row.matchKind === "content" ? 70 : 52
                    radius: Theme.radiusSm
                    color: ListView.isCurrentItem
                           ? Theme.itemCurrentFill
                           : (mouse.containsMouse ? Theme.itemHoverFill : "transparent")
                    border.color: ListView.isCurrentItem
                                  ? Theme.itemCurrentBorder
                                  : "transparent"
                    border.width: 1

                    function openCurrent() {
                        root.openResult(row.path, row.isDirectory)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        Item {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32

                            FileIconCell {
                                anchors.centerIn: parent
                                width: 24
                                height: 24
                                iconSize: 24
                                path: row.path
                                isDirectory: row.isDirectory
                                useNativeIcons: typeof appSettings !== "undefined" && appSettings
                                                ? appSettings.useNativeIcons
                                                : true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: row.name
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeBody
                                font.weight: Font.DemiBold
                                elide: Text.ElideMiddle
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                visible: row.matchKind === "content"
                                spacing: 0

                                Label {
                                    text: "Line " + row.lineNumber + ": "
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeCaption
                                }

                                Label {
                                    Layout.maximumWidth: 180
                                    text: row.contentBefore
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeCaption
                                    elide: Text.ElideLeft
                                    maximumLineCount: 1
                                }

                                Item {
                                    visible: row.spaceBeforeMatch
                                    implicitWidth: 4
                                    implicitHeight: 1
                                }

                                Label {
                                    Layout.maximumWidth: 240
                                    text: row.contentMatch
                                    color: root.dialogAccent
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.bold: true
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }

                                Item {
                                    visible: row.spaceAfterMatch
                                    implicitWidth: 4
                                    implicitHeight: 1
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    text: row.contentAfter
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeCaption
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: row.matchKind !== "content"
                                text: row.displayPath
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeCaption
                                elide: Text.ElideMiddle
                                maximumLineCount: 1
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: row.matchKind === "content"
                                text: row.displayPath
                                color: Theme.withAlpha(Theme.textSecondary, 0.76)
                                font.pixelSize: Theme.fontSizeMicro
                                elide: Text.ElideMiddle
                            }
                        }

                        ColumnLayout {
                            Layout.preferredWidth: 118
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: row.sizeText
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeCaption
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: row.modifiedText
                                color: Theme.withAlpha(Theme.textSecondary, 0.78)
                                font.pixelSize: Theme.fontSizeMicro
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                        }
                    }

                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (mouseEvent) => {
                            resultsList.currentIndex = row.index
                            resultsList.forceActiveFocus()
                            if (mouseEvent.button === Qt.RightButton) {
                                root.copyPath(row.path)
                            }
                        }
                        onDoubleClicked: (mouseEvent) => {
                            resultsList.currentIndex = row.index
                            resultsList.forceActiveFocus()
                            if (mouseEvent.button === Qt.LeftButton) {
                                row.openCurrent()
                            }
                        }
                    }
                }
            }

            EmptyState {
                anchors.centerIn: parent
                visible: !root.hasResults
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/search.svg"
                colorizeIcon: true
                iconColor: root.dialogAccent
                title: !root.canSearchRoot
                       ? "Search unavailable"
                       : (!root.hasQuery ? "Start typing" : (root.searching ? "Searching" : "No matches"))
                subtitle: !root.canSearchRoot
                          ? "Open a regular folder to search its files."
                          : (!root.hasQuery ? "Results will update as you type." : "")
                maxTextWidth: 280
            }
        }
    }
}
