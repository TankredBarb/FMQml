import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../framework"
import "../../style"

Item {
    id: root

    property string path: ""
    property var info: ({})
    property var previewController: null
    property var backdropSource: null
    property bool requested: false
    property bool suppressed: false
    property bool peekEnabled: false
    required property var panel
    property int viewMode: 0
    property rect anchorRect: Qt.rect(0, 0, 1, 1)
    property int boundaryBottomInset: 0
    readonly property var snapshot: previewController ? previewController.snapshot : ({})
    readonly property string state: snapshot && snapshot.state ? String(snapshot.state) : "idle"
    readonly property var entries: snapshot && snapshot.entries ? snapshot.entries : []
    readonly property bool pointerInside: cardHover.hovered
    readonly property int margin: 12
    readonly property int cursorGap: 18
    readonly property real availableWidth: parent ? parent.width : width
    readonly property real availableHeight: parent ? Math.max(0, parent.height - boundaryBottomInset) : height
    readonly property bool placeLeft: anchorRect.x + anchorRect.width + width + cursorGap > availableWidth
    readonly property bool placeBelow: anchorRect.y + height + margin > availableHeight
    readonly property real preferredX: placeLeft ? anchorRect.x - width - cursorGap : anchorRect.x + anchorRect.width + cursorGap
    readonly property real preferredY: placeBelow ? anchorRect.y + anchorRect.height - height : anchorRect.y
    readonly property bool translucentSurface: appSettings ? appSettings.hoverPreviewTransparency : false
    readonly property real transparencyStrength: appSettings ? appSettings.commandPaletteTransparencyStrength / 100.0 : 0.6
    readonly property real surfaceAlpha: themeController.isDark ? 1.0 - transparencyStrength * 0.32 : 1.0 - transparencyStrength * 0.26
    readonly property bool blurSurface: translucentSurface && appSettings && appSettings.surfaceBlur && backdropSource

    signal openRequested(string path)
    signal peekRequested(string path)
    signal viewModeRequested(int mode)

    width: Math.min(Math.max(0, availableWidth - margin * 2), 304)
    height: 316
    x: Math.max(margin, Math.min(preferredX, availableWidth - width - margin))
    y: Math.max(margin, Math.min(preferredY, availableHeight - height - margin))
    visible: opacity > 0
    opacity: delayTimer.ready && requested && !suppressed ? 1 : 0
    enabled: opacity > 0 && !suppressed

    function cancelSnapshot() {
        if (previewController) previewController.cancel()
    }

    onPathChanged: {
        cancelSnapshot()
        delayTimer.restart()
    }
    onRequestedChanged: {
        if (requested) delayTimer.restart()
        else { delayTimer.stop(); cancelSnapshot() }
    }
    onSuppressedChanged: {
        if (suppressed) { delayTimer.stop(); cancelSnapshot() }
        else if (requested) delayTimer.restart()
    }

    Timer {
        id: delayTimer
        property bool ready: false
        interval: 360
        onRunningChanged: if (running) ready = false
        onTriggered: {
            ready = root.requested && !root.suppressed && root.path.length > 0
            if (ready && root.previewController) {
                const showHidden = root.previewController && root.info && root.info.showHidden === true
                const model = root.panel && root.panel.controller ? root.panel.controller.directoryModel : null
                root.previewController.requestWithSort(root.path, showHidden, 9,
                                                       model ? model.sortRole : 0,
                                                       model ? model.sortOrder : Qt.AscendingOrder,
                                                       model ? model.mixFilesAndFolders : false)
            }
        }
    }

    HoverHandler { id: cardHover; enabled: root.enabled }
    Behavior on opacity { NumberAnimation { duration: 120 } }

    TranslucentSurface {
        anchors.fill: parent
        translucent: root.translucentSurface
        active: root.visible
        backgroundBlurEnabled: root.blurSurface
        backdropSource: root.backdropSource
        backdropTransformItem: root
        cornerRadius: 8
        baseColor: root.translucentSurface ? Theme.withAlpha(Theme.panelSurface, root.surfaceAlpha) : Theme.panelSurface
        startColor: root.translucentSurface ? Theme.withAlpha(Theme.chromeGradientStart, root.surfaceAlpha) : Theme.chromeGradientStart
        midColor: root.translucentSurface ? Theme.withAlpha(Theme.chromeGradientMid, root.surfaceAlpha) : Theme.chromeGradientMid
        endColor: root.translucentSurface
                  ? Theme.withAlpha(Theme.panelSurface, root.surfaceAlpha)
                  : Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.88 : 0.82)
        gradientStrength: 0.32
        borderColor: Theme.panelStroke
        borderWidth: 1
        shadowEnabled: false
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            RecolorSvgIcon { Layout.preferredWidth: 20; Layout.preferredHeight: 20; sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder.svg"; recolorColor: Theme.textSecondary }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 0
                Label { Layout.fillWidth: true; text: root.info && root.info.name ? root.info.name : root.path; color: Theme.textPrimary; font.weight: Font.DemiBold; elide: Text.ElideRight }
                Label {
                    Layout.fillWidth: true
                    text: root.state === "ready" ? root.snapshot.displayedCount + (root.snapshot.hasMore ? "+ items" : " items") : root.state
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro; elide: Text.ElideRight
                }
            }
            FmIconButton {
                iconSource: root.viewMode === 0 ? "qrc:/qt/qml/FM/qml/assets/icons-classic/list.svg" : "qrc:/qt/qml/FM/qml/assets/icons-classic/layout-grid.svg"
                Accessible.name: root.viewMode === 0 ? "Use list view" : "Use grid view"
                onClicked: root.viewModeRequested(root.viewMode === 0 ? 1 : 0)
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            GridView {
                id: hoverGridView
                anchors.fill: parent
                visible: root.state === "ready" && root.viewMode === 0
                model: root.entries.slice(0, 6)
                cellWidth: width / 3
                cellHeight: height / 2
                interactive: false
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: hoverGridView.cellWidth
                    height: hoverGridView.cellHeight
                    radius: 6
                    color: "transparent"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 5; spacing: 2
                        FolderPreviewIcon {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 30
                            panel: root.panel
                            surface: "hover-grid"
                            active: root.requested && !root.suppressed && root.state === "ready"
                            entryIndex: index
                            iconSize: 30
                            path: modelData.path
                            name: modelData.name
                            iconName: modelData.iconName
                            suffix: modelData.suffix || ""
                            mimeType: modelData.mimeType || ""
                            isDirectory: modelData.isDirectory
                            hasThumbnail: modelData.hasThumbnail === true
                        }
                        Label { Layout.fillWidth: true; text: modelData.name; color: modelData.isDirectory ? TextColors.folderNameText : TextColors.fileNameText; font.pixelSize: Theme.fontSizeMicro; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                    }
                }
            }
            ListView {
                id: hoverListView
                anchors.fill: parent
                visible: root.state === "ready" && root.viewMode === 1
                model: root.entries.slice(0, 6)
                spacing: 4
                interactive: false
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: hoverListView.width
                    height: Math.max(24, (hoverListView.height - hoverListView.spacing * 5) / 6)
                    radius: 5
                    color: "transparent"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 7; anchors.rightMargin: 7
                        FolderPreviewIcon {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            panel: root.panel
                            surface: "hover-list"
                            active: root.requested && !root.suppressed && root.state === "ready"
                            entryIndex: index
                            iconSize: 24
                            path: modelData.path
                            name: modelData.name
                            iconName: modelData.iconName
                            suffix: modelData.suffix || ""
                            mimeType: modelData.mimeType || ""
                            isDirectory: modelData.isDirectory
                            hasThumbnail: modelData.hasThumbnail === true
                        }
                        Label { Layout.fillWidth: true; text: modelData.name; color: modelData.isDirectory ? TextColors.folderNameText : TextColors.fileNameText; font.pixelSize: Theme.fontSizeCaption; elide: Text.ElideRight }
                    }
                }
            }
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 24, 220)
                spacing: 6
                visible: root.state === "empty"

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 68
                    Layout.preferredHeight: 68
                    Rectangle {
                        anchors.centerIn: parent
                        width: 64; height: 64; radius: 32
                        color: Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.12 : 0.09)
                        border.color: Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.24 : 0.18)
                    }
                    RecolorSvgIcon {
                        anchors.centerIn: parent
                        width: 38; height: 38
                        sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg"
                        recolorColor: Theme.activeAccent
                    }
                    Rectangle {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        width: 24; height: 24; radius: 12
                        color: Theme.panelSurfaceStrong
                        border.color: Theme.panelStroke
                        RecolorSvgIcon {
                            anchors.centerIn: parent
                            width: 14; height: 14
                            sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/file-plus.svg"
                            recolorColor: Theme.activeAccent
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: "Nothing here yet"
                    color: Theme.textPrimary
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    text: "A clean space, ready for your files."
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeMicro
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
            ColumnLayout {
                anchors.centerIn: parent; spacing: 8
                visible: root.state !== "ready" && root.state !== "empty"
                FmProgressRing { Layout.alignment: Qt.AlignHCenter; visible: root.state === "loading"; running: visible }
                Label {
                    visible: root.state !== "loading"; color: root.state === "error" ? Theme.warning : Theme.textSecondary
                    text: root.state === "unavailable" ? "Preview unavailable for this provider" : root.snapshot.errorText || "Waiting…"
                }
            }
        }

        Label { Layout.fillWidth: true; text: root.path; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro; elide: Text.ElideMiddle }
        RowLayout {
            Layout.fillWidth: true
            FmButton { Layout.fillWidth: true; text: "Open"; onClicked: root.openRequested(root.path) }
            FmButton { Layout.fillWidth: true; text: "Peek"; visible: root.peekEnabled; highlighted: true; onClicked: root.peekRequested(root.path) }
        }
    }
}
