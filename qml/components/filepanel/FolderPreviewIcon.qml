import QtQuick

Item {
    id: root

    required property var panel
    required property string path
    required property string name
    required property string iconName
    property string suffix: ""
    property string mimeType: ""
    property string primaryBadgeKind: ""
    property bool isDirectory: false
    property bool hasThumbnail: false
    property bool active: true
    property bool schedulingPaused: false
    property bool loadingPaused: false
    property int entryIndex: 0
    property int iconSize: 24
    property string surface: "folder-preview"
    readonly property bool traceEnabled: Qt.application.arguments.indexOf("--folder-preview-thumbnail-trace") >= 0
    readonly property string normalizedIconName: iconName.endsWith(".svg")
                                                  ? iconName.slice(0, -4)
                                                  : iconName
    readonly property bool genericSnapshotIcon: normalizedIconName === "folder"
                                                 || normalizedIconName === "document"
                                                 || normalizedIconName === "image"
    readonly property string effectiveIconName: genericSnapshotIcon ? "" : normalizedIconName
    property string thumbnailFailedPath: ""
    property int thumbnailRetryAttempt: 0
    property int thumbnailRetryRevision: 0
    property bool thumbnailLoadEnabled: false

    readonly property bool thumbnailEligible: active
                                                     && panel.effectiveUseNativeIcons
                                                     && panel.effectiveShowThumbnails
                                                     && !isDirectory
                                                     && hasThumbnail
                                                     && thumbnailFailedPath !== path
    readonly property bool canScheduleThumbnail: thumbnailEligible
                                                  && !schedulingPaused
                                                  && !loadingPaused
                                                  && !panel.thumbnailLoadingPaused
                                                  && !panel.thumbnailSchedulingPaused
    readonly property bool thumbnailRequestActive: thumbnailLoadEnabled && thumbnailEligible

    function trace(event, detail) {
        if (!traceEnabled) return
        console.log("[FolderPreviewThumb] surface=" + surface
                    + " entry=" + entryIndex
                    + " event=" + event
                    + " active=" + active
                    + " eligible=" + thumbnailEligible
                    + " request=" + thumbnailRequestActive
                    + " schedulePause=" + schedulingPaused
                    + " loadPause=" + loadingPaused
                    + " panelLoadPause=" + panel.thumbnailLoadingPaused
                    + " panelSchedulePause=" + panel.thumbnailSchedulingPaused
                    + (detail ? " " + detail : ""))
    }

    function queueThumbnailLoad(clearExisting) {
        trace("queue", "clear=" + (clearExisting === true))
        if (clearExisting === true || !thumbnailEligible) thumbnailLoadEnabled = false
        if (canScheduleThumbnail && !thumbnailLoadEnabled) {
            thumbnailDelayTimer.restart()
        } else {
            thumbnailDelayTimer.stop()
        }
    }

    function resetThumbnailLoad() {
        trace("reset", "")
        thumbnailDelayTimer.stop()
        thumbnailRetryTimer.stop()
        thumbnailFailedPath = ""
        thumbnailRetryAttempt = 0
        thumbnailRetryRevision = 0
        queueThumbnailLoad(true)
    }

    function scheduleThumbnailRetry() {
        if (!thumbnailRequestActive || thumbnailRetryAttempt >= 3) {
            trace("retry-skip", "attempt=" + thumbnailRetryAttempt)
            return
        }
        trace("retry-schedule", "attempt=" + thumbnailRetryAttempt)
        thumbnailRetryTimer.interval = 350 * Math.pow(2, thumbnailRetryAttempt)
        thumbnailRetryTimer.restart()
    }

    function resumeAfterScroll() {
        if (!thumbnailEligible || panel.thumbnailLoadingPaused
                || panel.thumbnailSchedulingPaused) {
            trace("resume-skip", "")
            return
        }
        if (!thumbnailLoadEnabled) thumbnailLoadEnabled = true
        trace("resume", "size=" + (iconSize * 2))
        if (typeof thumbnailController !== "undefined" && thumbnailController) {
            thumbnailController.requestThumbnail(path,
                                                 iconSize * 2,
                                                 iconSize * 2,
                                                 100,
                                                 "visible")
        }
    }

    onPathChanged: resetThumbnailLoad()
    onActiveChanged: { trace("active-change", ""); queueThumbnailLoad(!active) }
    onSchedulingPausedChanged: { trace("schedule-pause-change", ""); queueThumbnailLoad() }
    onLoadingPausedChanged: { trace("load-pause-change", ""); queueThumbnailLoad() }
    Component.onCompleted: { trace("created", ""); queueThumbnailLoad(true) }
    Component.onDestruction: trace("destroyed", "")

    Connections {
        target: root.panel
        function onThumbnailLoadingPausedChanged() {
            root.queueThumbnailLoad()
        }
        function onThumbnailSchedulingPausedChanged() { root.queueThumbnailLoad() }
        function onEffectiveUseNativeIconsChanged() { root.queueThumbnailLoad(true) }
        function onEffectiveShowThumbnailsChanged() { root.queueThumbnailLoad(true) }
    }

    Connections {
        target: typeof thumbnailController !== "undefined" ? thumbnailController : null
        function onThumbnailReady(path, identity, width, height, revision) {
            if (String(path) !== root.path) return
            root.thumbnailFailedPath = ""
            root.thumbnailRetryAttempt = 0
            root.thumbnailLoadEnabled = true
            root.thumbnailRetryRevision += 1
            root.trace("controller-ready", "revision=" + revision)
        }
    }

    Timer {
        id: thumbnailDelayTimer
        interval: 100 + (Math.max(0, root.entryIndex) % 16) * 28
        onTriggered: {
            root.thumbnailLoadEnabled = root.canScheduleThumbnail
            root.trace("delay-fired", "enabled=" + root.thumbnailLoadEnabled)
            if (root.thumbnailLoadEnabled
                    && typeof thumbnailController !== "undefined"
                    && thumbnailController) {
                thumbnailController.requestThumbnail(root.path,
                                                     root.iconSize * 2,
                                                     root.iconSize * 2,
                                                     100,
                                                     "visible")
                root.trace("request-sent", "size=" + (root.iconSize * 2))
            }
        }
    }

    Timer {
        id: thumbnailRetryTimer
        onTriggered: {
            if (!root.thumbnailRequestActive) return
            root.thumbnailRetryAttempt += 1
            root.thumbnailRetryRevision += 1
            root.trace("retry-fired", "attempt=" + root.thumbnailRetryAttempt)
        }
    }

    FileIconCell {
        id: iconCell
        anchors.fill: parent
        iconSize: root.iconSize
        path: root.path
        name: root.name
        iconName: root.effectiveIconName
        suffix: root.suffix
        mimeType: root.mimeType
        primaryBadgeKind: root.primaryBadgeKind
        isDirectory: root.isDirectory
        hasThumbnail: root.hasThumbnail
        useNativeIcons: root.panel.effectiveUseNativeIcons
        showThumbnail: root.thumbnailRequestActive
        suppressThumbnailDisplay: root.loadingPaused || root.panel.thumbnailLoadingPaused
        thumbnailSource: root.thumbnailRequestActive
                         ? root.panel.thumbnailSourceFor(root.path,
                                                         root.thumbnailRetryRevision * 1000000)
                         : ""
        onThumbnailError: {
            root.trace("image-error", "")
            root.thumbnailFailedPath = root.path
            root.thumbnailLoadEnabled = false
        }
        onThumbnailSoftMiss: {
            root.trace("image-soft-miss", "")
            root.scheduleThumbnailRetry()
        }
        onThumbnailDisplayedChanged: root.trace("display-change", "displayed=" + thumbnailDisplayed)
    }
}
