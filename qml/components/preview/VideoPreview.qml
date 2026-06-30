import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../../style"

Item {
    id: root

    property string sourcePath: ""
    property string name: ""
    property string sizeText: ""
    property string modifiedText: ""
    property string mimeName: ""
    property string extension: ""
    property int sourceSizeWidth: 2048
    property int sourceSizeHeight: 2048
    property string loadingText: "Loading video preview..."
    property bool showBusyIndicator: true
    property bool compact: false
    property var extraProperties: []
    property bool metadataHidden: false

    readonly property int imageStatus: previewImage.imageStatus
    readonly property bool thumbnailReady: previewImage.imageStatus === Image.Ready
    readonly property bool thumbnailLoading: previewImage.imageStatus === Image.Loading
    readonly property int extraPropertyCount: extraList().length
    readonly property string videoDurationText: extraValue("Duration", root.extraPropertyCount)
    readonly property string videoDimensionsText: extraValue("Dimensions", root.extraPropertyCount)
    readonly property string videoCodecText: extraValue("Video Codec", root.extraPropertyCount)
    readonly property string videoFrameRateText: extraValue("Frame Rate", root.extraPropertyCount)
    readonly property string videoBitrateText: extraValue("Bitrate", root.extraPropertyCount)
    readonly property string videoPixelFormatText: extraValue("Pixel Format", root.extraPropertyCount)
    readonly property string audioCodecText: extraValue("Audio Codec", root.extraPropertyCount)
    readonly property string audioSampleRateText: extraValue("Sample Rate", root.extraPropertyCount)
    readonly property string audioChannelsText: extraValue("Channels", root.extraPropertyCount)
    readonly property string formatText: extension.length > 0 ? extension.toUpperCase() : "VIDEO"
    readonly property var metadataStripItems: root.compact
                                             ? [root.formatText, root.videoDurationText, root.videoDimensionsText]
                                             : [
                                                   root.formatText,
                                                   root.videoDurationText,
                                                   root.videoDimensionsText,
                                                   root.videoCodecText,
                                                   root.videoFrameRateText,
                                                   root.videoBitrateText,
                                                   root.videoPixelFormatText,
                                                   root.audioCodecText,
                                                   root.audioSampleRateText,
                                                   root.audioChannelsText
                                               ]
    readonly property string titleText: name.length > 0 ? name : "Video File"
    readonly property string subtitleText: mimeName.length > 0 ? mimeName : "Video preview"
    readonly property bool metadataBarReserved: !root.metadataHidden && root.metadataStripItems.length > 0
    readonly property bool metadataBarVisible: root.metadataBarReserved && root.thumbnailReady
    readonly property string metaText: {
        if (sizeText.length > 0 && modifiedText.length > 0) return sizeText + "  |  " + modifiedText
        if (sizeText.length > 0) return sizeText
        if (modifiedText.length > 0) return modifiedText
        return formatText
    }

    signal hideMetadataRequested()
    signal showMetadataRequested()

    clip: true

    function safeText(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function extraList() {
        return root.extraProperties && root.extraProperties.length !== undefined ? root.extraProperties : []
    }

    function extraValue(label, revision) {
        revision
        const extras = root.extraList()
        for (let i = 0; i < extras.length; i++) {
            if (root.safeText(extras[i].label) === label) {
                return root.safeText(extras[i].value)
            }
        }
        return ""
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        visible: !root.thumbnailReady

        Rectangle {
            anchors.fill: parent
            anchors.margins: 0
            radius: Theme.panelRadius
            color: Theme.withAlpha(Theme.secondaryAccent, themeController.isDark ? 0.12 : 0.09)
            border.color: Theme.withAlpha(Theme.secondaryAccent, themeController.isDark ? 0.34 : 0.26)
            border.width: 1
            clip: true

            RowLayout {
                anchors.fill: parent
                anchors.margins: root.compact ? 10 : 18
                spacing: root.compact ? 10 : 18

                Rectangle {
                    Layout.preferredWidth: root.compact ? 62 : 116
                    Layout.preferredHeight: width
                    Layout.maximumWidth: Math.min(parent.width * 0.34, root.compact ? 62 : 116)
                    Layout.maximumHeight: Layout.maximumWidth
                    radius: Theme.radiusLg
                    color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.16 : 0.12)
                    border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.42 : 0.30)
                    border.width: 1

                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/FM/qml/assets/icons/video.svg"
                        sourceSize: Qt.size(root.compact ? 34 : 58, root.compact ? 34 : 58)
                        opacity: 0.92
                        smooth: true
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: root.compact ? 5 : 7
                        width: formatLabel.implicitWidth + 12
                        height: root.compact ? 18 : 20
                        radius: Theme.radiusSm
                        color: Theme.withAlpha(Theme.bg, themeController.isDark ? 0.72 : 0.82)

                        Label {
                            id: formatLabel
                            anchors.centerIn: parent
                            text: root.formatText
                            font.pixelSize: root.compact ? 8 : 9
                            font.bold: true
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: root.compact ? 3 : 7

                    Label {
                        Layout.fillWidth: true
                        text: root.titleText
                        font.pixelSize: root.compact ? 14 : 22
                        font.bold: true
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.subtitleText
                        font.pixelSize: root.compact ? 11 : 13
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.metaText
                        font.pixelSize: root.compact ? 10 : 11
                        color: Theme.textSecondary
                        opacity: 0.84
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        Layout.preferredWidth: statusText.implicitWidth + 18
                        Layout.preferredHeight: root.compact ? 22 : 24
                        radius: Theme.radiusSm
                        color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.13 : 0.10)
                        border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.32 : 0.24)
                        border.width: 1

                        Label {
                            id: statusText
                            anchors.centerIn: parent
                            text: root.thumbnailLoading ? "Loading thumbnail" : "Preview unavailable"
                            font.pixelSize: root.compact ? 9 : 10
                            font.bold: true
                            color: Theme.accent
                        }
                    }
                }
            }
        }
    }

    Item {
        id: viewport
        anchors.fill: parent
        anchors.topMargin: root.metadataBarReserved ? videoMetaStrip.height : 0
        clip: true

        ImagePreview {
            id: previewImage
            anchors.fill: parent
            sourcePath: root.sourcePath
            fillMode: Image.PreserveAspectFit
            verticalAlignment: Image.AlignTop
            sourceSizeWidth: root.sourceSizeWidth
            sourceSizeHeight: root.sourceSizeHeight
            showOverlayIcon: false
            overlayIconSource: "qrc:/qt/qml/FM/qml/assets/icons/video.svg"
            overlayIconSize: 64
            showBusyIndicator: false
            opacity: root.thumbnailReady ? 1 : 0
        }
    }

    PreviewMetaStrip {
        id: videoMetaStrip
        z: 2
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.right: parent.right
        compact: root.compact
        backgroundOpacity: 0
        borderOpacity: 0
        cornerRadius: 0
        labelWeight: Font.DemiBold
        showHideButton: true
        wrapItems: !root.compact
        items: root.metadataStripItems
        visible: root.metadataBarVisible
        onHideRequested: root.hideMetadataRequested()
    }

    ToolButton {
        id: showMetadataButton
        z: 3
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: root.compact ? 8 : 14
        width: root.compact ? 24 : 28
        height: width
        visible: root.metadataHidden && root.thumbnailReady
        hoverEnabled: true
        padding: 5
        opacity: hovered ? 1.0 : 0.82
        display: AbstractButton.IconOnly
        ToolTip.visible: hovered
        ToolTip.text: "Show metadata"
        onClicked: root.showMetadataRequested()

        contentItem: Item {
            implicitWidth: root.compact ? 13 : 15
            implicitHeight: implicitWidth

            RecolorSvgIcon {
                anchors.centerIn: parent
                width: parent.implicitWidth
                height: parent.implicitHeight
                sourcePath: "qrc:/qt/qml/FM/qml/assets/toolbar-next/eye.svg"
                recolorColor: showMetadataButton.hovered ? Theme.actionIconColor("hidden") : Theme.actionIconColor("muted")
                sourceSize: Qt.size(32, 32)
                opacity: showMetadataButton.enabled ? 1.0 : 0.42
            }
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.withAlpha(themeController.isDark ? Theme.surface : Theme.bg,
                                   showMetadataButton.hovered ? 0.72 : 0.54)
            border.color: Theme.withAlpha(Theme.border, showMetadataButton.hovered ? 0.58 : 0.42)
            border.width: 1
        }
    }
}
