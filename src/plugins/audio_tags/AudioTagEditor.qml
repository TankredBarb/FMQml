import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FM
import FMAudioTags 1.0

Item {
    id: root

    property var pluginContext: ({})
    readonly property int currentIndex: backend.currentIndex
    readonly property var editorRecord: backend.currentRecord
    readonly property bool busy: backend.busy
    readonly property int dirtyCount: backend.dirtyCount
    readonly property var coverLookupCandidates: backend.coverLookupCandidates
    readonly property string coverLookupStatus: backend.coverLookupStatus
    readonly property bool coverLookupStatusIsError: backend.coverLookupStatusIsError
    readonly property bool coverLookupBusy: backend.coverLookupBusy
    readonly property bool coverDownloadBusy: backend.coverDownloadBusy
    readonly property string downloadingImageUrl: backend.downloadingImageUrl
    readonly property var lyricsCandidates: backend.lyricsCandidates
    readonly property string lyricsLookupStatus: backend.lyricsLookupStatus
    readonly property bool lyricsLookupStatusIsError: backend.lyricsLookupStatusIsError
    readonly property bool lyricsLookupBusy: backend.lyricsLookupBusy
    readonly property string tagLookupStatus: backend.tagLookupStatus
    readonly property bool tagLookupStatusIsError: backend.tagLookupStatusIsError
    readonly property bool tagLookupBusy: backend.tagLookupBusy
    property bool applyDoesNotClose: true
    signal applyFinished(var result)
    readonly property bool coverNetworkBusy: root.coverLookupBusy || root.coverDownloadBusy
    readonly property bool lyricsNetworkBusy: root.lyricsLookupBusy
    readonly property bool tagNetworkBusy: root.tagLookupBusy
    readonly property bool canApplyCurrent: root.editorRecord.dirty === true
        && !root.busy
        && !root.coverNetworkBusy
        && !root.lyricsNetworkBusy
        && !root.tagNetworkBusy
    readonly property bool canApplyAll: root.dirtyCount > 0
        && !root.busy
        && !root.coverNetworkBusy
        && !root.lyricsNetworkBusy
        && !root.tagNetworkBusy
    // Host compat: canApply enables the "Apply" button (current track).
    readonly property bool canApply: root.canApplyCurrent

    // ───── helpers ─────────────────────────────────────────────────────────────

    function selectIndex(index) {
        backend.currentIndex = index
    }

    function updateField(field, value) {
        backend.updateCurrentField(field, value)
    }

    function chooseCover(url) {
        const coverUrl = String(url || "")
        const coverPath = backend.localPathFromUrl(coverUrl)
        if (coverPath.length === 0) {
            return
        }
        backend.setCurrentCover(coverPath, coverUrl, false)
    }

    function clearCover() {
        backend.setCurrentCover("", "", true)
    }

    function clearAllTags() {
        backend.clearCurrentTags()
    }

    function applyCurrentCoverToAll() {
        backend.applyCurrentCoverToAll()
    }

    function fetchCoverCandidates() {
        backend.fetchCoverCandidates()
    }

    function useCoverCandidate(candidate) {
        backend.useCoverCandidate(candidate || ({}))
    }

    function applyCurrent() {
        backend.applyCurrent()
    }

    function applyAll() {
        backend.applyAll()
    }

    // Host compat: apply() routes to applyCurrent().
    function apply() {
        return root.applyCurrent()
    }

    function fetchLyricsCandidates() {
        backend.fetchLyricsCandidates()
    }

    function useLyricCandidate(candidate) {
        backend.useLyricsCandidate(candidate || ({}))
    }

    function fetchTags() {
        backend.fetchTags()
    }

    Component.onCompleted: {
        const paths = root.pluginContext.selectedPaths || []
        backend.load(paths)
    }

    // ───── backend ─────────────────────────────────────────────────────────────

    AudioTagEditorSession {
        id: backend
    }

    Connections {
        target: backend

        function onApplyFinished(result) {
            root.applyFinished(result)
        }

        function onLyricsApplied() {
            Qt.callLater(function() { lyricsTextArea.cursorPosition = 0 })
        }
    }

    FileDialog {
        id: coverDialog
        title: "Choose Cover Art"
        nameFilters: ["Images (*.jpg *.jpeg *.png)"]
        onAccepted: root.chooseCover(selectedFile)
    }

    // ───── layout ──────────────────────────────────────────────────────────────

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 16

        // ── Left panel: file list ─────────────────────────────────────────────
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 280
            radius: Theme.radiusSm
            color: Theme.panelSurfaceSoft
            border.color: Theme.withAlpha(Theme.panelBorder, 0.55)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: backend.editModel.count + " audio file" + (backend.editModel.count === 1 ? "" : "s")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    elide: Text.ElideRight
                }

                ListView {
                    id: fileList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: backend.editModel
                    currentIndex: root.currentIndex

                    delegate: Rectangle {
                        required property int index
                        required property var record
                        readonly property var rowRecord: record

                        width: ListView.view.width
                        height: 54
                        radius: Theme.radiusSm
                        color: index === root.currentIndex
                               ? Theme.withAlpha(Theme.accent, 0.14)
                               : (mouseArea.containsMouse ? Theme.surfaceHover : "transparent")
                        border.color: index === root.currentIndex ? Theme.withAlpha(Theme.accent, 0.45) : "transparent"

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.selectIndex(index)
                        }

                        Column {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            anchors.topMargin: 7
                            spacing: 3

                            Label {
                                width: parent.width
                                text: rowRecord.name || rowRecord.path || ""
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                elide: Text.ElideMiddle
                            }

                            Label {
                                width: parent.width
                                text: rowRecord.error && rowRecord.error.length > 0
                                      ? rowRecord.error
                                      : (rowRecord.dirty === true ? "Modified" : (rowRecord.ok === true ? "Ready" : "Unavailable"))
                                color: rowRecord.error && rowRecord.error.length > 0
                                       ? Theme.danger
                                       : (rowRecord.dirty === true ? Theme.accent : Theme.textSecondary)
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMicro
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        // ── Right panel: editor ───────────────────────────────────────────────
        ScrollView {
            id: editorScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: ScrollBar {
                id: editorVerticalBar
                policy: ScrollBar.AsNeeded
                interactive: true
                width: 8

                background: Item { implicitWidth: 8 }
                contentItem: Rectangle {
                    implicitWidth: 4
                    radius: 2
                    color: Theme.withAlpha(Theme.textSecondary,
                                           editorVerticalBar.pressed ? 0.46
                                                                     : (editorVerticalBar.active ? 0.30 : 0.18))
                }
            }

            // One single scrolling column. Inner sections must NOT use
            // Layout.fillHeight: the outer ScrollView handles overflow.
            ColumnLayout {
                width: Math.max(0, editorScroll.availableWidth
                                   - (editorVerticalBar.visible ? editorVerticalBar.width + 8 : 0))
                spacing: 18
                enabled: root.editorRecord.ok === true

                // ── File header (name + path + pending badge) ──────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: root.editorRecord.name || "Audio file"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.Medium
                        elide: Text.ElideMiddle
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.editorRecord.path || ""
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMicro
                        elide: Text.ElideMiddle
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.dirtyCount > 0
                        text: root.dirtyCount + " file" + (root.dirtyCount === 1 ? "" : "s") + " with pending changes"
                        color: Theme.accent
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMicro
                        elide: Text.ElideRight
                    }
                }

                // ── Section: Cover Art ─────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    // Section header
                    Label {
                        text: "COVER ART"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMicro
                        font.letterSpacing: 1
                        font.weight: Font.Medium
                    }

                    // Cover preview + button grid
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14

                        // Cover thumbnail (compact 128×128)
                        Rectangle {
                            Layout.preferredWidth: 128
                            Layout.preferredHeight: 128
                            radius: Theme.radiusMd
                            color: Theme.panelSurfaceSoft
                            border.color: Theme.withAlpha(Theme.panelBorder, 0.55)
                            clip: true

                            Image {
                                anchors.fill: parent
                                anchors.margins: 1
                                source: root.editorRecord.pendingCoverSource
                                        ? root.editorRecord.pendingCoverSource
                                        : (root.editorRecord.removeCover === true ? "" : (root.editorRecord.coverSource || ""))
                                fillMode: Image.PreserveAspectCrop
                                cache: false
                            }

                            Label {
                                anchors.centerIn: parent
                                width: parent.width - 16
                                visible: root.editorRecord.removeCover === true
                                         || (!root.editorRecord.pendingCoverSource && !root.editorRecord.coverSource)
                                text: root.editorRecord.removeCover === true ? "Will be\nremoved" : "No cover"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMicro
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                            }
                        }

                        // 2×2 button grid for cover actions
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 8
                            columnSpacing: 8

                            // Choose Cover — accent-outlined
                            DialogActionButton {
                                Layout.fillWidth: true
                                text: "Choose Cover"
                                enabled: root.editorRecord.coverWriteSupported === true && !root.coverNetworkBusy
                                onClicked: coverDialog.open()
                            }

                            // Fetch Cover — accent-filled
                            DialogActionButton {
                                Layout.fillWidth: true
                                text: root.coverLookupBusy ? "Searching…" : "Fetch Cover"
                                enabled: root.editorRecord.coverWriteSupported === true
                                         && root.editorRecord.artist
                                         && root.editorRecord.album
                                         && !root.coverNetworkBusy
                                onClicked: root.fetchCoverCandidates()
                                highlighted: true
                            }

                            // Clear Cover — danger-outlined
                            DialogActionButton {
                                Layout.fillWidth: true
                                text: "Clear Cover"
                                enabled: root.editorRecord.coverWriteSupported === true && !root.coverNetworkBusy
                                onClicked: root.clearCover()
                            }

                            // Apply to All
                            DialogActionButton {
                                Layout.fillWidth: true
                                text: "Apply to All"
                                enabled: root.editorRecord.coverWriteSupported === true
                                         && root.editorRecord.coverDirty === true
                                         && root.editorRecord.removeCover !== true
                                         && !root.coverNetworkBusy
                                onClicked: root.applyCurrentCoverToAll()
                            }
                        }
                    }

                    // Caption (outside GridLayout to avoid cell-placement conflicts)
                    Label {
                        Layout.fillWidth: true
                        text: root.editorRecord.coverWriteSupported === true
                              ? (root.editorRecord.coverDirty === true ? "Cover has pending changes" : "Cover art can be edited")
                              : "Cover editing is not supported for this file type"
                        color: root.editorRecord.coverDirty === true ? Theme.accent : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMicro
                        elide: Text.ElideRight
                    }

                    // Cover search status + spinner
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.coverNetworkBusy || root.coverLookupStatus.length > 0

                        BusyIndicator {
                            implicitWidth: 18
                            implicitHeight: 18
                            running: root.coverNetworkBusy
                            visible: root.coverNetworkBusy
                            palette.dark: Theme.accent
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.coverLookupStatus
                            color: root.coverLookupStatusIsError ? Theme.danger
                                 : (root.coverNetworkBusy ? Theme.textSecondary : Theme.accent)
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideRight
                        }
                    }

                    // Big centered spinner shown while cover search is running
                    // but no candidates have been returned yet.
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 96
                        visible: root.coverLookupBusy && root.coverLookupCandidates.length === 0

                        BusyIndicator {
                            anchors.centerIn: parent
                            implicitWidth: 36
                            implicitHeight: 36
                            running: true
                            palette.dark: Theme.accent
                        }

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.bottom
                            anchors.topMargin: 6
                            text: root.coverLookupStatus.length > 0 ? root.coverLookupStatus : "Searching cover art…"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideRight
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // Cover candidates list
                    ListView {
                        id: candidateList
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.coverLookupCandidates.length > 0
                                                ? Math.min(280, root.coverLookupCandidates.length * 78)
                                                : 0
                        clip: true
                        spacing: 6
                        model: root.coverLookupCandidates
                        enabled: !root.coverDownloadBusy
                        opacity: root.coverLookupCandidates.length > 0 ? 1.0 : 0.0
                        Behavior on opacity {
                            NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
                        }
                        Behavior on Layout.preferredHeight {
                            NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
                        }

                        delegate: Rectangle {
                            id: coverDelegate
                            required property var modelData
                            readonly property bool isDownloading: root.coverDownloadBusy
                                                                  && root.downloadingImageUrl === coverDelegate.modelData.imageUrl
                            width: ListView.view.width
                            height: 74
                            radius: Theme.radiusSm
                            color: candidateArea.containsMouse
                                   ? Theme.withAlpha(Theme.accent, 0.08)
                                   : Theme.panelSurfaceSoft
                            border.color: candidateArea.containsMouse
                                          ? Theme.withAlpha(Theme.accent, 0.40)
                                          : Theme.withAlpha(Theme.panelBorder, 0.55)

                            Behavior on color { ColorAnimation { duration: 100 } }
                            Behavior on border.color { ColorAnimation { duration: 100 } }

                            MouseArea {
                                id: candidateArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.useCoverCandidate(coverDelegate.modelData)
                            }

                            RowLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 12

                                // Thumbnail with BusyIndicator while loading
                                Rectangle {
                                    Layout.preferredWidth: 54
                                    Layout.preferredHeight: 54
                                    radius: Theme.radiusSm
                                    color: Theme.surface
                                    border.color: Theme.withAlpha(Theme.panelBorder, 0.45)
                                    clip: true

                                    Image {
                                        id: thumbImage
                                        anchors.fill: parent
                                        source: coverDelegate.modelData.thumbnailSource || ""
                                        fillMode: Image.PreserveAspectCrop
                                        cache: true
                                        asynchronous: true
                                        visible: status === Image.Ready
                                    }

                                    BusyIndicator {
                                        anchors.centerIn: parent
                                        implicitWidth: 22
                                        implicitHeight: 22
                                        running: thumbImage.status === Image.Loading
                                        visible: thumbImage.status === Image.Loading
                                        palette.dark: Theme.accent
                                    }

                                    Label {
                                        anchors.centerIn: parent
                                        visible: thumbImage.status === Image.Null || thumbImage.status === Image.Error
                                        text: "♪"
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                    }
                                }

                                // Text info
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: coverDelegate.modelData.title || "Cover"
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: coverDelegate.modelData.subtitle || "Cover Art Archive"
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: coverDelegate.isDownloading ? "Downloading…" : "Click to use this cover"
                                        color: coverDelegate.isDownloading ? Theme.warning : Theme.accent
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                        font.weight: coverDelegate.isDownloading ? Font.Medium : Font.Normal
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }

                // ── Separator ─────────────────────────────────────────────────
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.withAlpha(Theme.panelBorder, 0.40)
                }

                // ── Section: Tags ──────────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: "TAGS"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                            font.letterSpacing: 1
                            font.weight: Font.Medium
                        }
                        Item { Layout.fillWidth: true }

                        BusyIndicator {
                            implicitWidth: 16
                            implicitHeight: 16
                            running: root.tagLookupBusy
                            visible: root.tagLookupBusy
                            palette.dark: Theme.accent
                        }

                        Label {
                            visible: root.tagLookupStatus.length > 0
                            text: root.tagLookupStatus
                            color: root.tagLookupStatusIsError ? Theme.danger : Theme.accent
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideRight
                            Layout.maximumWidth: 360
                        }

                        DialogActionButton {
                            text: "Clear All Tags"
                            enabled: root.editorRecord.ok === true && !root.tagNetworkBusy
                            onClicked: root.clearAllTags()
                        }

                        // Auto-Fill Tags — accent-filled
                        DialogActionButton {
                            text: root.tagLookupBusy ? "Searching…" : "Auto-Fill Tags"
                            enabled: root.editorRecord.ok === true
                                     && root.editorRecord.artist
                                     && root.editorRecord.title
                                     && !root.tagNetworkBusy
                            onClicked: root.fetchTags()
                            highlighted: true
                        }
                    }

                    // Title + Artist side by side
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                text: "Title"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMicro
                            }
                            TextField {
                                Layout.fillWidth: true
                                text: root.editorRecord.title || ""
                                selectByMouse: true
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                onTextEdited: root.updateField("title", text)
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                text: "Artist"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMicro
                            }
                            TextField {
                                Layout.fillWidth: true
                                text: root.editorRecord.artist || ""
                                selectByMouse: true
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                onTextEdited: root.updateField("artist", text)
                            }
                        }
                    }

                    // Album + Genre side by side
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                text: "Album"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMicro
                            }
                            TextField {
                                Layout.fillWidth: true
                                text: root.editorRecord.album || ""
                                selectByMouse: true
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                onTextEdited: root.updateField("album", text)
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                text: "Genre"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMicro
                            }
                            TextField {
                                Layout.fillWidth: true
                                text: root.editorRecord.genre || ""
                                selectByMouse: true
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                onTextEdited: root.updateField("genre", text)
                            }
                        }
                    }

                    // Year + Track side by side
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                text: "Year"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMicro
                            }
                            TextField {
                                Layout.fillWidth: true
                                text: root.editorRecord.year || ""
                                selectByMouse: true
                                inputMethodHints: Qt.ImhDigitsOnly
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                onTextEdited: root.updateField("year", text)
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                text: "Track"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMicro
                            }
                            TextField {
                                Layout.fillWidth: true
                                text: root.editorRecord.track || ""
                                selectByMouse: true
                                inputMethodHints: Qt.ImhDigitsOnly
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                onTextEdited: root.updateField("track", text)
                            }
                        }
                    }

                    // Comment — single line, compact
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            text: "Comment"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                        }
                        TextField {
                            Layout.fillWidth: true
                            text: root.editorRecord.comment || ""
                            selectByMouse: true
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLabel
                            placeholderText: "Optional"
                            onTextEdited: root.updateField("comment", text)
                        }
                    }
                }

                // ── Separator ─────────────────────────────────────────────────
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.withAlpha(Theme.panelBorder, 0.40)
                }

                // ── Section: Lyrics ────────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: "LYRICS"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                            font.letterSpacing: 1
                            font.weight: Font.Medium
                        }
                        Item { Layout.fillWidth: true }

                        // Fetch Lyrics — accent-filled
                        DialogActionButton {
                            text: root.lyricsLookupBusy ? "Searching…" : "Fetch Lyrics"
                            enabled: root.editorRecord.ok === true
                                     && root.editorRecord.artist
                                     && root.editorRecord.title
                                     && !root.lyricsNetworkBusy
                            onClicked: root.fetchLyricsCandidates()
                            highlighted: true
                        }
                    }

                    // Lyrics search status + spinner
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.lyricsNetworkBusy || root.lyricsLookupStatus.length > 0

                        BusyIndicator {
                            implicitWidth: 18
                            implicitHeight: 18
                            running: root.lyricsNetworkBusy
                            visible: root.lyricsNetworkBusy
                            palette.dark: Theme.accent
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.lyricsLookupStatus
                            color: root.lyricsLookupStatusIsError ? Theme.danger
                                 : (root.lyricsNetworkBusy ? Theme.textSecondary : Theme.accent)
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideRight
                        }
                    }

                    // Lyrics candidates list
                    ListView {
                        id: lyricCandidateList
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.lyricsCandidates.length > 0
                                                ? Math.min(220, root.lyricsCandidates.length * 68)
                                                : 0
                        clip: true
                        spacing: 6
                        model: root.lyricsCandidates
                        enabled: !root.lyricsLookupBusy
                        opacity: root.lyricsCandidates.length > 0 ? 1.0 : 0.0
                        Behavior on opacity {
                            NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
                        }
                        Behavior on Layout.preferredHeight {
                            NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
                        }

                        delegate: Rectangle {
                            id: lyricDelegate
                            required property var modelData
                            width: ListView.view.width
                            height: Math.max(64, lyricRow.implicitHeight + 16)
                            radius: Theme.radiusSm
                            color: lyricArea.containsMouse
                                   ? Theme.withAlpha(Theme.accent, 0.08)
                                   : Theme.panelSurfaceSoft
                            border.color: lyricArea.containsMouse
                                          ? Theme.withAlpha(Theme.accent, 0.40)
                                          : Theme.withAlpha(Theme.panelBorder, 0.55)

                            Behavior on color { ColorAnimation { duration: 100 } }
                            Behavior on border.color { ColorAnimation { duration: 100 } }

                            MouseArea {
                                id: lyricArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.useLyricCandidate(lyricDelegate.modelData)
                            }

                            RowLayout {
                                id: lyricRow
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 10

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: (lyricDelegate.modelData.title || "Lyrics")
                                              + (lyricDelegate.modelData.artist ? " — " + lyricDelegate.modelData.artist : "")
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: (lyricDelegate.modelData.album || "")
                                              + (lyricDelegate.modelData.hasSynced ? "  ·  synced" : "  ·  plain")
                                              + (lyricDelegate.modelData.durationSec > 0
                                                 ? "  ·  %1:%2".arg(Math.floor(lyricDelegate.modelData.durationSec / 60))
                                                       .arg((lyricDelegate.modelData.durationSec % 60).toString().padStart(2, "0"))
                                                 : "")
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: lyricDelegate.modelData.preview || ""
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                        wrapMode: Text.WrapAnywhere
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }

                    // Lyrics text area — fixed height ScrollView so inner scroll works
                    ScrollView {
                        id: lyricsScroll
                        Layout.fillWidth: true
                        Layout.preferredHeight: 240
                        clip: true
                        contentWidth: availableWidth
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical: ScrollBar {
                            id: lyricsVerticalBar
                            policy: ScrollBar.AsNeeded
                            interactive: true
                            width: 8

                            background: Item { implicitWidth: 8 }
                            contentItem: Rectangle {
                                implicitWidth: 4
                                radius: 2
                                color: Theme.withAlpha(Theme.textSecondary,
                                                       lyricsVerticalBar.pressed ? 0.46
                                                                                 : (lyricsVerticalBar.active ? 0.30 : 0.18))
                            }
                        }

                        TextArea {
                            id: lyricsTextArea
                            width: Math.max(0, lyricsScroll.availableWidth
                                               - (lyricsVerticalBar.visible ? lyricsVerticalBar.width + 8 : 0))
                            text: root.editorRecord.lyrics || ""
                            selectByMouse: true
                            wrapMode: TextEdit.Wrap
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLabel
                            placeholderText: "No lyrics — type manually or click Fetch Lyrics"
                            onTextChanged: if (activeFocus) root.updateField("lyrics", text)
                        }
                    }
                }
            }
        }
    }
}
