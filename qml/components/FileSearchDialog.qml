import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Dialogs
import "../style"
import "common"
import "framework"
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
    property var backdropSource: null
    property real dragOriginX: 0
    property real dragOriginY: 0
    property string searchRootPath: ""
    property bool includeHidden: false
    property int searchTarget: 0
    property bool caseSensitive: false
    property bool includeFolders: true
    property int matchMode: 0
    property int kindFilter: 0
    property string extensionFilter: ""
    property int modifiedPreset: 0
    property string minimumSizeMiB: ""
    property string maximumSizeMiB: ""
    property int scopeMode: 0
    property string initialScopePath: ""
    property string manualScopePath: ""
    property string selectedResultPath: ""
    property string selectedResultKind: ""
    property int selectedResultLine: 0
    property real selectedResultContentY: 0
    property bool cancelOnClose: true
    property bool returnedFromPanel: false
    readonly property bool searching: fileSearchController && fileSearchController.busy
    readonly property bool hasQuery: searchField.text.trim().length > 0
    readonly property bool hasResults: fileSearchController && fileSearchController.resultsModel.count > 0
    readonly property bool canSearchRoot: fileSearchController && fileSearchController.canSearchPath(root.searchRootPath)
    readonly property bool filtersActive: root.kindFilter !== 0 || root.extensionFilter.trim().length > 0
                                          || root.modifiedPreset !== 0 || root.minimumSizeMiB.trim().length > 0
                                          || root.maximumSizeMiB.trim().length > 0
    readonly property int skippedDetailCount: fileSearchController
                                             ? fileSearchController.skippedDetailEntries.length
                                             : 0
    readonly property color dialogAccent: Theme.accent
    readonly property bool workspaceDialogsTransparency: typeof appSettings !== "undefined" && appSettings
                                                         ? appSettings.workspaceDialogsTransparency
                                                         : false

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
        root.initialScopePath = root.searchRootPath
        root.manualScopePath = ""
        root.scopeMode = 0
        root.includeHidden = includeHiddenFiles === true
        root.searchTarget = 0
        root.caseSensitive = false
        root.includeFolders = true
        root.matchMode = 0
        root.kindFilter = 0
        root.extensionFilter = ""
        root.modifiedPreset = 0
        root.minimumSizeMiB = ""
        root.maximumSizeMiB = ""
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
        const minimumSize = root.minimumSizeMiB.trim().length > 0 ? Number(root.minimumSizeMiB) : -1
        const maximumSize = root.maximumSizeMiB.trim().length > 0 ? Number(root.maximumSizeMiB) : -1
        fileSearchController.search(root.searchRootPath, query, root.includeHidden, root.searchTarget,
                                    root.caseSensitive, root.matchMode, root.includeFolders,
                                    root.kindFilter, root.extensionFilter, root.modifiedPreset,
                                    isNaN(minimumSize) ? -1 : minimumSize,
                                    isNaN(maximumSize) ? -1 : maximumSize)
    }

    function panelPath(side) {
        if (typeof workspaceController === "undefined" || !workspaceController) return ""
        const panel = side === 1 ? workspaceController.leftPanel : workspaceController.rightPanel
        return panel && panel.currentPath ? String(panel.currentPath) : ""
    }

    function localPathFromUrl(url) {
        let value = url ? url.toString() : ""
        if (value.startsWith("file:///")) {
            value = decodeURIComponent(value.substring(8))
            if (Qt.platform.os === "windows" && value.length >= 3 && value[1] === ":")
                return value
            return "/" + value
        }
        if (value.startsWith("file://"))
            return decodeURIComponent(value.substring(7))
        return decodeURIComponent(value)
    }

    function applyScope(mode, path) {
        const nextPath = path || ""
        root.scopeMode = mode
        root.searchRootPath = nextPath
        root.returnedFromPanel = false
        root.searchContextReset()
        if (root.hasQuery) searchDebounceTimer.restart()
    }

    function captureSelectedResult() {
        const item = resultsList.currentItem
        root.selectedResultPath = item ? item.path : ""
        root.selectedResultKind = item ? item.matchKind : ""
        root.selectedResultLine = item ? item.lineNumber : 0
        root.selectedResultContentY = resultsList.contentY
    }

    function restoreSelectedResult() {
        if (!fileSearchController || root.selectedResultPath.length === 0) return
        const index = fileSearchController.resultsModel.indexOfResult(root.selectedResultPath,
                                                                      root.selectedResultKind,
                                                                      root.selectedResultLine)
        if (index >= 0) resultsList.currentIndex = index
        Qt.callLater(() => {
            const minimumY = resultsList.originY
            const maximumY = Math.max(minimumY,
                                      resultsList.originY + resultsList.contentHeight - resultsList.height)
            resultsList.contentY = Math.max(minimumY, Math.min(maximumY, root.selectedResultContentY))
        })
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

    function openContainingFolder(path) {
        const panel = activePanelController()
        if (!panel || !path || path.length === 0) {
            return
        }
        if (panel.openSearchResult(path, false)) {
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

    function escapedStyledText(value) {
        return String(value).replace(/&/g, "&amp;")
                            .replace(/</g, "&lt;")
                            .replace(/>/g, "&gt;")
                            .replace(/\"/g, "&quot;")
    }

    function highlightedName(before, match, after) {
        if (!match || match.length === 0) return root.escapedStyledText(before + after)
        return root.escapedStyledText(before)
                + "<font color=\"" + root.dialogAccent + "\"><b>"
                + root.escapedStyledText(match) + "</b></font>"
                + root.escapedStyledText(after)
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
        if (root.searchTarget !== 0) {
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

    component SearchModeComboBox : FmComboBox {
        id: combo
        implicitHeight: 28
        accentColor: root.dialogAccent
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeCaption

    }

    component SearchToggle : FmCheckBox {
        id: toggle

        property string toolTipText: ""

        implicitHeight: 28
        accentColor: root.dialogAccent
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeCaption
        font.weight: Font.DemiBold

        ToolTip.visible: hovered && toolTipText.length > 0
        ToolTip.delay: 350
        ToolTip.text: toolTipText
    }

    component StopSearchButton : FmButton {
        id: stopButton

        hoverEnabled: true
        implicitWidth: 96
        implicitHeight: 34
        highlighted: true
        primaryColor: Theme.warning
    }

    background: DialogShell {
        translucent: root.workspaceDialogsTransparency
        backdropSource: root.backdropSource
        backdropTransformItem: root
        accentColor: root.dialogAccent
        shellBorderColor: Theme.panelBorder
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/search.svg"
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
            text: root.hasQuery ? root.resultCountText() + "  ·  " + root.progressText() : root.progressText()
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            elide: Text.ElideRight
        }

        FmButton {
            id: skippedButton

            visible: root.skippedDetailCount > 0
            text: root.skippedDetailCount + " skipped"
            flat: true
            primaryColor: Theme.warning
            Layout.preferredHeight: 28
            onClicked: skippedPopup.open()

            contentItem: Label {
                text: parent.text
                color: Theme.warning
                font.pixelSize: Theme.fontSizeCaption
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        StopSearchButton {
            visible: root.searching
            text: "Cancel"
            onClicked: fileSearchController.cancel()
        }

        FmButton {
            visible: root.returnedFromPanel
            text: "Clear Results"
            onClicked: root.clearSearchResults()
            ToolTip.visible: hovered
            ToolTip.delay: 350
            ToolTip.text: "Clears these results and hides the toolbar Search Results button."
        }

        FmButton {
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

    FmMenu {
        id: resultContextMenu

        property string targetPath: ""
        property bool targetIsDirectory: false

        FmMenuItem {
            text: "Open"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg"
            iconColor: Theme.actionIconColor("open")
            onClicked: root.openResult(resultContextMenu.targetPath,
                                       resultContextMenu.targetIsDirectory)
        }

        FmMenuItem {
            text: "Open containing folder"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder.svg"
            iconColor: Theme.actionIconColor("navigation")
            onClicked: root.openContainingFolder(resultContextMenu.targetPath)
        }

        FmMenuSeparator {}

        FmMenuItem {
            text: "Copy path"
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/copy.svg"
            iconColor: Theme.actionIconColor("copy")
            onClicked: root.copyPath(resultContextMenu.targetPath)
        }
    }

    Popup {
        id: filtersPopup

        width: 420
        x: Math.max(12, root.width - width - 20)
        y: 118
        padding: 16
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.panelSurface
            border.color: Theme.panelBorder
            border.width: 1
        }

        ColumnLayout {
            width: parent.width
            spacing: 14

            Label {
                text: "Filters"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.panelBorder
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 16
                rowSpacing: 12

                Label {
                    text: "Kind"
                    color: Theme.textSecondary
                    Layout.preferredWidth: 88
                }
                SearchModeComboBox {
                    Layout.fillWidth: true
                    model: ["All", "Folders", "Files", "Images", "Video", "Audio", "Documents", "Archives"]
                    currentIndex: root.kindFilter
                    onActivated: (index) => {
                        root.kindFilter = index
                        if (root.hasQuery) searchDebounceTimer.restart()
                    }
                }

                Label {
                    text: "Size (MiB)"
                    color: Theme.textSecondary
                    Layout.preferredWidth: 88
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    FmTextField {
                        Layout.fillWidth: true
                        placeholderText: "Minimum"
                        text: root.minimumSizeMiB
                        validator: DoubleValidator { bottom: 0 }
                        onTextChanged: {
                            root.minimumSizeMiB = text
                            if (root.hasQuery) searchDebounceTimer.restart()
                        }
                    }

                    Label {
                        text: "to"
                        color: Theme.textSecondary
                        opacity: 0.72
                    }

                    FmTextField {
                        Layout.fillWidth: true
                        placeholderText: "Maximum"
                        text: root.maximumSizeMiB
                        validator: DoubleValidator { bottom: 0 }
                        onTextChanged: {
                            root.maximumSizeMiB = text
                            if (root.hasQuery) searchDebounceTimer.restart()
                        }
                    }
                }

                Label {
                    text: "Extension"
                    color: Theme.textSecondary
                    Layout.preferredWidth: 88
                }
                FmTextField {
                    Layout.fillWidth: true
                    placeholderText: "e.g. txt or .png"
                    text: root.extensionFilter
                    onTextChanged: {
                        root.extensionFilter = text
                        if (root.hasQuery) searchDebounceTimer.restart()
                    }
                }

                Label {
                    text: "Modified"
                    color: Theme.textSecondary
                    Layout.preferredWidth: 88
                }
                SearchModeComboBox {
                    Layout.fillWidth: true
                    model: ["Any time", "Today", "Last week", "Last month"]
                    currentIndex: root.modifiedPreset
                    onActivated: (index) => {
                        root.modifiedPreset = index
                        if (root.hasQuery) searchDebounceTimer.restart()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.panelBorder
            }

            FmButton {
                Layout.alignment: Qt.AlignRight
                text: "Clear filters"
                flat: true
                enabled: root.kindFilter !== 0 || root.extensionFilter.trim().length > 0 || root.modifiedPreset !== 0
                         || root.minimumSizeMiB.trim().length > 0 || root.maximumSizeMiB.trim().length > 0
                onClicked: {
                    root.kindFilter = 0
                    root.extensionFilter = ""
                    root.modifiedPreset = 0
                    root.minimumSizeMiB = ""
                    root.maximumSizeMiB = ""
                    if (root.hasQuery) searchDebounceTimer.restart()
                }
            }
        }
    }

    Connections {
        target: fileSearchController
        function onResultsAboutToBeSorted() { root.captureSelectedResult() }
        function onResultsSorted() { Qt.callLater(root.restoreSelectedResult) }
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
                sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/search.svg"
                sourceSize: Qt.size(16, 16)
                recolorEnabled: true
                recolorColor: root.dialogAccent
                opacity: 0.86
            }

            FmTextField {
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
                text: "Search in"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
            }
            SearchModeComboBox {
                id: scopeModeCombo
                Layout.preferredWidth: 150
                model: ["Current folder", "Left panel", "Right panel", "Choose folder..."]
                currentIndex: root.scopeMode
                onActivated: (index) => {
                    if (index === 0) root.applyScope(0, root.initialScopePath)
                    else if (index === 1) root.applyScope(1, root.panelPath(1))
                    else if (index === 2) root.applyScope(2, root.panelPath(2))
                    else scopeFolderDialog.open()
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                radius: Theme.controlRadius
                color: Theme.panelSurfaceSoft
                border.width: 1
                border.color: root.canSearchRoot
                              ? Theme.withAlpha(Theme.panelBorder, 0.72)
                              : Theme.withAlpha(Theme.warning, 0.72)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 9
                    anchors.rightMargin: 9
                    spacing: 7

                    RecolorSvgIcon {
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder.svg"
                        sourceSize: Qt.size(14, 14)
                        recolorEnabled: true
                        recolorColor: root.canSearchRoot ? root.dialogAccent : Theme.warning
                        opacity: 0.86
                    }

                    Label {
                        Layout.fillWidth: true
                        text: workspaceController
                              ? workspaceController.displayPath(root.searchRootPath)
                              : root.searchRootPath
                        color: root.canSearchRoot ? Theme.textSecondary : Theme.warning
                        font.pixelSize: Theme.fontSizeCaption
                        elide: Text.ElideMiddle
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                MouseArea {
                    id: scopePathMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }

                ToolTip.visible: scopePathMouse.containsMouse
                ToolTip.delay: 450
                ToolTip.text: root.searchRootPath
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            SearchModeComboBox {
                id: searchTargetCombo

                Layout.preferredWidth: 150
                Layout.preferredHeight: 28
                model: ["Name", "Contents", "Name + contents"]
                currentIndex: root.searchTarget
                enabled: root.canSearchRoot
                onActivated: (index) => {
                    root.searchTarget = index
                    if (root.hasQuery) {
                        root.returnedFromPanel = false
                        root.searchContextReset()
                        searchDebounceTimer.restart()
                    }
                }
            }

            SearchModeComboBox {
                id: entryTypeCombo

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
                model: ["Contains", "Exact", "Wildcard"]
                currentIndex: root.matchMode
                enabled: root.canSearchRoot && root.searchTarget !== 1
                onActivated: (index) => {
                    root.matchMode = index
                    if (root.hasQuery) {
                        root.returnedFromPanel = false
                        root.searchContextReset()
                        searchDebounceTimer.restart()
                    }
                }
            }

            SearchModeComboBox {
                Layout.preferredWidth: 118
                Layout.preferredHeight: 28
                model: ["Relevance", "Name", "Path", "Size", "Modified"]
                currentIndex: fileSearchController ? fileSearchController.sortMode : 0
                enabled: root.hasResults
                onActivated: (index) => {
                    if (fileSearchController) fileSearchController.sortMode = index
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

            SearchToggle {
                id: hiddenToggle

                checked: root.includeHidden
                text: "Hidden files"
                enabled: root.canSearchRoot
                toolTipText: checked ? "Hidden files and folders are included."
                                     : "Hidden files and folders are excluded."
                onToggled: {
                    root.includeHidden = checked
                    if (root.hasQuery) {
                        root.returnedFromPanel = false
                        root.searchContextReset()
                        searchDebounceTimer.restart()
                    }
                }
            }

            FmButton {
                Layout.preferredWidth: 30
                Layout.minimumWidth: 30
                Layout.preferredHeight: 28
                leftPadding: 0
                rightPadding: 0
                flat: true
                highlighted: root.filtersActive
                primaryColor: root.dialogAccent
                onClicked: filtersPopup.open()

                contentItem: Item {
                    RecolorSvgIcon {
                        anchors.centerIn: parent
                        width: 15
                        height: 15
                        sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/funnel.svg"
                        sourceSize: Qt.size(15, 15)
                        recolorEnabled: true
                        recolorColor: root.filtersActive ? Theme.accentText : Theme.textSecondary
                        opacity: parent.parent.enabled ? 1 : 0.5
                    }
                }

                ToolTip.visible: hovered
                ToolTip.delay: 350
                ToolTip.text: root.filtersActive ? "Filters active" : "Filters"
            }

            Item { Layout.fillWidth: true }
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

                ScrollBar.vertical: FmScrollBar {
                    id: resultsScrollBar

                    policy: ScrollBar.AsNeeded
                    active: hovered || resultsList.moving || resultsList.flicking
                    onPressedChanged: resultsList.updateResultHold()
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
                    required property int nameMatchStart
                    required property int nameMatchLength
                    readonly property int boundedNameMatchStart: Math.max(0, Math.min(nameMatchStart, name.length))
                    readonly property int boundedNameMatchLength: Math.max(0, Math.min(nameMatchLength,
                                                                                       name.length - boundedNameMatchStart))
                    readonly property string nameBefore: name.slice(0, boundedNameMatchStart)
                    readonly property string nameMatch: name.slice(boundedNameMatchStart,
                                                                    boundedNameMatchStart + boundedNameMatchLength)
                    readonly property string nameAfter: name.slice(boundedNameMatchStart + boundedNameMatchLength)
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

                    width: resultsScrollBar.scrollNeeded
                           ? Math.max(0, resultsScrollBar.x - 6)
                           : resultsList.width
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

                            Text {
                                Layout.fillWidth: true
                                text: root.highlightedName(row.nameBefore, row.nameMatch, row.nameAfter)
                                textFormat: Text.StyledText
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBody
                                font.weight: Font.DemiBold
                                elide: Text.ElideMiddle
                                maximumLineCount: 1
                                verticalAlignment: Text.AlignVCenter
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

                                ToolTip.visible: mouse.containsMouse
                                ToolTip.delay: 500
                                ToolTip.text: row.displayPath
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: row.matchKind === "content"
                                text: row.displayPath
                                color: Theme.withAlpha(Theme.textSecondary, 0.76)
                                font.pixelSize: Theme.fontSizeMicro
                                elide: Text.ElideMiddle

                                ToolTip.visible: mouse.containsMouse
                                ToolTip.delay: 500
                                ToolTip.text: row.displayPath
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
                                resultContextMenu.targetPath = row.path
                                resultContextMenu.targetIsDirectory = row.isDirectory
                                resultContextMenu.popup()
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
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/search.svg"
                colorizeIcon: true
                iconColor: root.dialogAccent
                title: !root.canSearchRoot
                       ? "Search unavailable"
                       : (fileSearchController && fileSearchController.error.length > 0
                          ? "Search failed"
                       : (!root.hasQuery ? "Start typing" : (root.searching ? "Searching" : "No matches"))
                         )
                subtitle: !root.canSearchRoot
                          ? "Choose a regular local folder to search."
                          : (fileSearchController && fileSearchController.error.length > 0
                             ? fileSearchController.error
                             : (!root.hasQuery ? "Results will update as you type." : ""))
                maxTextWidth: 280
            }
        }
    }

    FolderDialog {
        id: scopeFolderDialog
        title: "Choose Search Folder"
        onAccepted: {
            root.manualScopePath = root.localPathFromUrl(selectedFolder)
            root.applyScope(3, root.manualScopePath)
        }
        onRejected: scopeModeCombo.currentIndex = root.scopeMode
    }
}
