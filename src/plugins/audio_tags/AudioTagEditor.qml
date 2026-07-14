import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FM
import FMAudioTags 1.0

Item {
    id: root

    property var pluginContext: ({})
    property var records: []
    property int currentIndex: 0
    property var editorRecord: ({})
    property bool busy: false
    property int dirtyCount: 0
    property var coverLookupCandidates: []
    property string coverLookupStatus: ""
    property bool coverLookupStatusIsError: false
    property int coverLookupRequestId: 0
    property int coverDownloadRequestId: 0
    property bool coverLookupBusy: false
    property bool coverDownloadBusy: false
    property string downloadingImageUrl: ""
    property var lyricsCandidates: []
    property string lyricsLookupStatus: ""
    property bool lyricsLookupStatusIsError: false
    property int lyricsLookupRequestId: 0
    property bool lyricsLookupBusy: false
    property var tagLookupFields: ({})
    property string tagLookupStatus: ""
    property bool tagLookupStatusIsError: false
    property int tagLookupRequestId: 0
    property bool tagLookupBusy: false
    property bool applyDoesNotClose: true
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

    function rebuildFileModel() {
        fileModel.clear()
        for (let i = 0; i < root.records.length; ++i) {
            fileModel.append(root.records[i])
        }
    }

    function recomputeDirtyCount() {
        let count = 0
        for (let i = 0; i < root.records.length; ++i) {
            if (root.records[i].dirty === true) {
                ++count
            }
        }
        root.dirtyCount = count
    }

    function selectIndex(index) {
        if (index < 0 || index >= root.records.length) {
            root.currentIndex = -1
            root.editorRecord = ({})
            return
        }
        root.currentIndex = index
        root.editorRecord = Object.assign({}, root.records[index])
        root.coverLookupCandidates = []
        root.coverLookupStatus = ""
        root.coverLookupStatusIsError = false
        root.coverLookupBusy = false
        root.coverDownloadBusy = false
        root.downloadingImageUrl = ""
        root.coverLookupRequestId += 1
        root.coverDownloadRequestId += 1
        backend.cancelCoverLookups()
        root.lyricsCandidates = []
        root.lyricsLookupStatus = ""
        root.lyricsLookupStatusIsError = false
        root.lyricsLookupBusy = false
        root.lyricsLookupRequestId += 1
        backend.cancelLyricsLookup()
        root.tagLookupFields = ({})
        root.tagLookupStatus = ""
        root.tagLookupStatusIsError = false
        root.tagLookupBusy = false
        root.tagLookupRequestId += 1
        backend.cancelTagsLookup()
    }

    function updateField(field, value) {
        if (root.records.length === 0 || root.currentIndex < 0 || root.currentIndex >= root.records.length) {
            return
        }
        const next = root.records.slice()
        const item = Object.assign({}, next[root.currentIndex])
        item[field] = value
        item.dirty = true
        item.error = ""
        next[root.currentIndex] = item
        root.records = next
        // Reassign the whole editorRecord so QML bindings like
        // `text: root.editorRecord.title` re-evaluate. Mutating a property
        // of a plain JS object does NOT fire editorRecordChanged.
        root.editorRecord = Object.assign({}, item)
        fileModel.set(root.currentIndex, item)
        root.recomputeDirtyCount()
    }

    function setCoverForIndex(index, coverPath, previewSource, removeCover) {
        if (index < 0 || index >= root.records.length) {
            return
        }
        const next = root.records.slice()
        const item = Object.assign({}, next[index])
        item.coverDirty = item.coverWriteSupported === true
        item.dirty = true
        item.removeCover = removeCover === true
        item.coverImagePath = removeCover === true ? "" : coverPath
        item.pendingCoverSource = removeCover === true ? "" : previewSource
        item.error = ""
        next[index] = item
        root.records = next
        fileModel.set(index, item)
        if (index === root.currentIndex) {
            root.editorRecord = Object.assign({}, item)
        }
        root.recomputeDirtyCount()
    }

    function chooseCover(url) {
        const coverUrl = String(url || "")
        const coverPath = backend.localPathFromUrl(coverUrl)
        if (coverPath.length === 0) {
            return
        }
        root.setCoverForIndex(root.currentIndex, coverPath, coverUrl, false)
    }

    function clearCover() {
        root.setCoverForIndex(root.currentIndex, "", "", true)
    }

    function clearAllTags() {
        if (root.records.length === 0 || root.currentIndex < 0 || root.currentIndex >= root.records.length) {
            return
        }
        const next = root.records.slice()
        const item = Object.assign({}, next[root.currentIndex])
        const fields = ["title", "artist", "album", "year", "track", "genre", "comment", "lyrics"]
        for (let i = 0; i < fields.length; ++i) {
            item[fields[i]] = ""
        }
        item.clearAllTags = true
        item.coverDirty = item.coverWriteSupported === true
        item.dirty = true
        item.removeCover = true
        item.coverImagePath = ""
        item.pendingCoverSource = ""
        item.error = ""
        next[root.currentIndex] = item
        root.records = next
        root.editorRecord = Object.assign({}, item)
        fileModel.set(root.currentIndex, item)
        root.recomputeDirtyCount()
    }

    function applyCurrentCoverToAll() {
        if (!root.editorRecord.coverDirty || root.editorRecord.removeCover === true) {
            return
        }
        const coverPath = root.editorRecord.coverImagePath || ""
        const previewSource = root.editorRecord.pendingCoverSource || ""
        if (coverPath.length === 0 || previewSource.length === 0) {
            return
        }
        for (let i = 0; i < root.records.length; ++i) {
            if (root.records[i].coverWriteSupported === true) {
                root.setCoverForIndex(i, coverPath, previewSource, false)
            }
        }
    }

    function fetchCoverCandidates() {
        root.coverLookupCandidates = []
        root.coverLookupStatus = "Searching cover art…"
        root.coverLookupStatusIsError = false
        root.coverLookupBusy = true
        root.coverLookupRequestId += 1
        backend.lookupCoverArtAsync(root.editorRecord, root.coverLookupRequestId)
    }

    function useCoverCandidate(candidate) {
        if (!candidate || !candidate.imageUrl) {
            return
        }
        root.downloadingImageUrl = candidate.imageUrl
        root.coverLookupStatus = "Downloading cover art…"
        root.coverLookupStatusIsError = false
        root.coverDownloadBusy = true
        root.coverDownloadRequestId += 1
        backend.downloadCoverArtAsync(candidate.imageUrl, root.coverDownloadRequestId)
    }

    function applyCurrent() {
        if (root.currentIndex < 0 || root.currentIndex >= root.records.length) {
            return null
        }
        const item = root.records[root.currentIndex]
        if (!item || item.dirty !== true) {
            return null
        }
        root.busy = true
        const result = backend.applyChanges([item])
        const fileResults = result.results || []
        if (fileResults.length > 0) {
            const status = fileResults[0]
            const next = root.records.slice()
            const updated = Object.assign({}, next[root.currentIndex])
            if (status.ok === true) {
                updated.dirty = false
                updated.coverDirty = false
                updated.clearAllTags = false
                updated.removeCover = false
                updated.coverImagePath = ""
                updated.error = ""
            } else {
                updated.error = String(status.message || "Save failed.")
            }
            next[root.currentIndex] = updated
            root.records = next
            root.editorRecord = Object.assign({}, updated)
            fileModel.set(root.currentIndex, updated)
        }
        root.recomputeDirtyCount()
        root.releaseCoverStagingIfUnused()
        root.busy = false
        return result
    }

    function applyAll() {
        root.busy = true
        const result = backend.applyChanges(root.records)
        const statuses = {}
        const fileResults = result.results || []
        for (let i = 0; i < fileResults.length; ++i) {
            statuses[fileResults[i].path] = fileResults[i]
        }

        const next = root.records.slice()
        for (let j = 0; j < next.length; ++j) {
            const item = Object.assign({}, next[j])
            const status = statuses[item.path]
            if (status) {
                if (status.ok === true) {
                    item.dirty = false
                    item.coverDirty = false
                    item.clearAllTags = false
                    item.removeCover = false
                    item.coverImagePath = ""
                    item.error = ""
                } else {
                    item.error = String(status.message || "Save failed.")
                }
            }
            next[j] = item
        }
        root.records = next
        root.rebuildFileModel()
        root.selectIndex(Math.min(root.currentIndex, root.records.length - 1))
        root.recomputeDirtyCount()
        root.releaseCoverStagingIfUnused()
        root.busy = false
        return result
    }

    // Host compat: apply() routes to applyCurrent().
    function apply() {
        return root.applyCurrent()
    }

    function releaseCoverStagingIfUnused() {
        for (let i = 0; i < root.records.length; ++i) {
            const item = root.records[i]
            if (item && item.coverDirty === true && String(item.coverImagePath || "").length > 0) {
                return
            }
        }
        backend.releaseCoverStaging()
    }

    function fetchLyricsCandidates() {
        root.lyricsCandidates = []
        root.lyricsLookupStatus = "Searching lyrics…"
        root.lyricsLookupStatusIsError = false
        root.lyricsLookupBusy = true
        root.lyricsLookupRequestId += 1
        backend.lookupLyricsAsync(root.editorRecord, root.lyricsLookupRequestId)
    }

    function useLyricCandidate(candidate) {
        if (!candidate) {
            return
        }
        let text = ""
        if (candidate.syncedLyrics && candidate.syncedLyrics.length > 0) {
            text = candidate.syncedLyrics
        } else if (candidate.plainLyrics && candidate.plainLyrics.length > 0) {
            text = candidate.plainLyrics
        }
        if (text.length === 0) {
            return
        }
        root.lyricsCandidates = []
        root.lyricsLookupStatus = ""
        root.lyricsLookupStatusIsError = false
        root.updateField("lyrics", text)
        Qt.callLater(function() {
            lyricsTextArea.cursorPosition = 0
        })
    }

    function fetchTags() {
        root.tagLookupStatus = "Looking up tags on MusicBrainz…"
        root.tagLookupStatusIsError = false
        root.tagLookupBusy = true
        root.tagLookupFields = ({})
        root.tagLookupRequestId += 1
        backend.lookupTagsAsync(root.editorRecord, root.tagLookupRequestId)
    }

    function applyTagFields(fields) {
        if (!fields) {
            return 0
        }
        if (root.records.length === 0 || root.currentIndex < 0 || root.currentIndex >= root.records.length) {
            return 0
        }
        const keys = ["title", "artist", "album", "year", "track", "genre"]
        const next = root.records.slice()
        const item = Object.assign({}, next[root.currentIndex])
        let count = 0
        for (let i = 0; i < keys.length; ++i) {
            const k = keys[i]
            const v = fields[k]
            if (v !== undefined && v !== null && String(v).length > 0) {
                item[k] = String(v)
                ++count
            }
        }
        if (count === 0) {
            return 0
        }
        item.dirty = true
        item.error = ""
        next[root.currentIndex] = item
        root.records = next
        root.editorRecord = Object.assign({}, item)
        fileModel.set(root.currentIndex, item)
        root.recomputeDirtyCount()
        return count
    }

    Component.onCompleted: {
        const paths = root.pluginContext.selectedPaths || []
        root.records = backend.loadTags(paths)
        root.rebuildFileModel()
        root.selectIndex(root.records.length > 0 ? 0 : -1)
        root.recomputeDirtyCount()
    }

    // ───── backend ─────────────────────────────────────────────────────────────

    AudioTagEditorBackend {
        id: backend
    }

    Connections {
        target: backend

        function onCoverLookupFinished(requestId, result) {
            if (requestId !== root.coverLookupRequestId) {
                return
            }
            root.coverLookupBusy = result.finished === false
            root.coverLookupStatus = String(result.message || "")
            root.coverLookupStatusIsError = result.ok === false
            root.coverLookupCandidates = result.candidates || []
        }

        function onCoverDownloadFinished(requestId, result) {
            if (requestId !== root.coverDownloadRequestId) {
                return
            }
            root.coverDownloadBusy = false
            root.downloadingImageUrl = ""
            root.coverLookupStatusIsError = result.ok !== true
            root.coverLookupStatus = String(result.message || (result.ok === true ? "Cover art selected." : "Cover art download failed."))
            if (result.ok === true) {
                root.setCoverForIndex(root.currentIndex, String(result.path || ""), String(result.source || ""), false)
            }
        }

        function onLyricsLookupFinished(requestId, result) {
            if (requestId !== root.lyricsLookupRequestId) {
                return
            }
            root.lyricsLookupBusy = false
            root.lyricsLookupStatusIsError = result.ok === false
            root.lyricsLookupStatus = String(result.message || "")
            root.lyricsCandidates = result.candidates || []
        }

        function onTagsLookupFinished(requestId, result) {
            if (requestId !== root.tagLookupRequestId) {
                return
            }
            root.tagLookupBusy = false
            root.tagLookupFields = result.fields || ({})
            root.tagLookupStatusIsError = result.ok === false
            const count = result.ok === true ? root.applyTagFields(result.fields || ({})) : 0
            root.tagLookupStatus = result.ok === true
                ? (String(result.message || "Tags filled.") + (count > 0 ? "" : " (no fields to fill)"))
                : String(result.message || "Tag lookup failed.")
        }
    }

    Component.onDestruction: {
        backend.cancelCoverLookups()
        backend.releaseCoverStaging()
        backend.cancelLyricsLookup()
        backend.cancelTagsLookup()
    }

    FileDialog {
        id: coverDialog
        title: "Choose Cover Art"
        nameFilters: ["Images (*.jpg *.jpeg *.png)"]
        onAccepted: root.chooseCover(selectedFile)
    }

    ListModel {
        id: fileModel
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
                    text: fileModel.count + " audio file" + (fileModel.count === 1 ? "" : "s")
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
                    model: fileModel
                    currentIndex: root.currentIndex

                    delegate: Rectangle {
                        required property int index
                        readonly property var rowRecord: index >= 0 && index < root.records.length
                                                         ? root.records[index]
                                                         : ({})

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
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            // One single scrolling column. Inner sections must NOT use
            // Layout.fillHeight: the outer ScrollView handles overflow.
            ColumnLayout {
                width: editorScroll.availableWidth
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
                            Button {
                                Layout.fillWidth: true
                                text: "Choose Cover"
                                enabled: root.editorRecord.coverWriteSupported === true && !root.coverNetworkBusy
                                onClicked: coverDialog.open()

                                contentItem: Label {
                                    text: parent.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLabel
                                    color: parent.enabled ? Theme.accent : Theme.textSecondary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    implicitHeight: Theme.controlHeight
                                    radius: Theme.radiusSm
                                    color: parent.pressed ? Theme.withAlpha(Theme.accent, 0.18)
                                         : (parent.hovered ? Theme.withAlpha(Theme.accent, 0.10) : "transparent")
                                    border.color: Theme.withAlpha(Theme.accent, parent.enabled ? 0.55 : 0.20)
                                    border.width: 1
                                }
                            }

                            // Fetch Cover — accent-filled
                            Button {
                                Layout.fillWidth: true
                                text: root.coverLookupBusy ? "Searching…" : "Fetch Cover"
                                enabled: root.editorRecord.coverWriteSupported === true
                                         && root.editorRecord.artist
                                         && root.editorRecord.album
                                         && !root.coverNetworkBusy
                                onClicked: root.fetchCoverCandidates()

                                contentItem: Label {
                                    text: parent.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLabel
                                    color: parent.enabled ? Theme.accentText : Theme.textSecondary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    implicitHeight: Theme.controlHeight
                                    radius: Theme.radiusSm
                                    color: !parent.enabled ? Theme.withAlpha(Theme.panelBorder, 0.6)
                                         : parent.pressed ? Qt.darker(Theme.accent, 1.15)
                                         : (parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent)
                                }
                            }

                            // Clear Cover — danger-outlined
                            Button {
                                Layout.fillWidth: true
                                text: "Clear Cover"
                                enabled: root.editorRecord.coverWriteSupported === true && !root.coverNetworkBusy
                                onClicked: root.clearCover()

                                contentItem: Label {
                                    text: parent.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLabel
                                    color: parent.enabled ? Theme.danger : Theme.textSecondary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    implicitHeight: Theme.controlHeight
                                    radius: Theme.radiusSm
                                    color: parent.pressed ? Theme.withAlpha(Theme.danger, 0.18)
                                         : (parent.hovered ? Theme.withAlpha(Theme.danger, 0.10) : "transparent")
                                    border.color: Theme.withAlpha(Theme.danger, parent.enabled ? 0.45 : 0.18)
                                    border.width: 1
                                }
                            }

                            // Apply to All
                            Button {
                                Layout.fillWidth: true
                                text: "Apply to All"
                                enabled: root.editorRecord.coverWriteSupported === true
                                         && root.editorRecord.coverDirty === true
                                         && root.editorRecord.removeCover !== true
                                         && !root.coverNetworkBusy
                                onClicked: root.applyCurrentCoverToAll()

                                contentItem: Label {
                                    text: parent.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLabel
                                    color: parent.enabled ? Theme.textPrimary : Theme.textSecondary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    implicitHeight: Theme.controlHeight
                                    radius: Theme.radiusSm
                                    color: parent.pressed ? Theme.surfaceActive
                                         : (parent.hovered ? Theme.surfaceHover : Theme.panelSurfaceSoft)
                                    border.color: Theme.withAlpha(Theme.panelBorder, 0.45)
                                    border.width: 1
                                }
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

                        Button {
                            text: "Clear All Tags"
                            enabled: root.editorRecord.ok === true && !root.tagNetworkBusy
                            onClicked: root.clearAllTags()

                            contentItem: Label {
                                text: parent.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                color: parent.enabled ? Theme.danger : Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitHeight: Theme.controlHeight
                                radius: Theme.radiusSm
                                color: parent.pressed ? Theme.withAlpha(Theme.danger, 0.18)
                                     : (parent.hovered ? Theme.withAlpha(Theme.danger, 0.10) : "transparent")
                                border.color: Theme.withAlpha(Theme.danger, parent.enabled ? 0.45 : 0.18)
                                border.width: 1
                            }
                        }

                        // Auto-Fill Tags — accent-filled
                        Button {
                            text: root.tagLookupBusy ? "Searching…" : "Auto-Fill Tags"
                            enabled: root.editorRecord.ok === true
                                     && root.editorRecord.artist
                                     && root.editorRecord.title
                                     && !root.tagNetworkBusy
                            onClicked: root.fetchTags()

                            contentItem: Label {
                                text: parent.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                color: parent.enabled ? Theme.accentText : Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitHeight: Theme.controlHeight
                                radius: Theme.radiusSm
                                color: !parent.enabled ? Theme.withAlpha(Theme.panelBorder, 0.6)
                                     : parent.pressed ? Qt.darker(Theme.accent, 1.15)
                                     : (parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent)
                            }
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
                        Button {
                            text: root.lyricsLookupBusy ? "Searching…" : "Fetch Lyrics"
                            enabled: root.editorRecord.ok === true
                                     && root.editorRecord.artist
                                     && root.editorRecord.title
                                     && !root.lyricsNetworkBusy
                            onClicked: root.fetchLyricsCandidates()

                            contentItem: Label {
                                text: parent.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                color: parent.enabled ? Theme.accentText : Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitHeight: Theme.controlHeight
                                radius: Theme.radiusSm
                                color: !parent.enabled ? Theme.withAlpha(Theme.panelBorder, 0.6)
                                     : parent.pressed ? Qt.darker(Theme.accent, 1.15)
                                     : (parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent)
                            }
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
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        TextArea {
                            id: lyricsTextArea
                            width: lyricsScroll.availableWidth
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
