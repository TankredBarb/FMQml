import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string path: ""
    property string sourcePath: ""
    property string name: ""
    property string sizeText: ""
    property string modifiedText: ""
    property string mimeName: ""
    property string extension: ""
    property string audioTitle: ""
    property string audioArtist: ""
    property string audioAlbum: ""
    property string audioYear: ""
    property string audioTrack: ""
    property string audioGenre: ""
    property string audioComment: ""
    property string audioDuration: ""
    property string audioBitrate: ""
    property string audioSampleRate: ""
    property string audioChannels: ""
    property string mediaSourceUrl: ""
    property bool compact: false
    property bool showDetails: false
    property bool multimediaControlsAvailable: false
    property bool playbackControlsActive: true

    readonly property string titleText: audioTitle.length > 0 ? audioTitle : (name.length > 0 ? name : "Audio File")
    readonly property string subtitleText: audioArtist.length > 0 && audioAlbum.length > 0
                                           ? audioArtist + " - " + audioAlbum
                                           : (audioArtist.length > 0
                                              ? audioArtist
                                              : (audioAlbum.length > 0
                                                 ? audioAlbum
                                                 : (mimeName.length > 0 ? mimeName : "Audio")))
    readonly property string formatText: extension.length > 0 ? extension.toUpperCase() : "AUDIO"
    readonly property string coverPath: sourcePath.length > 0 ? sourcePath : path
    readonly property var metaParts: [audioDuration, audioBitrate, audioSampleRate].filter(value => value.length > 0)
    readonly property string metaText: metaParts.length > 0 ? metaParts.join("  |  ") : (sizeText.length > 0 ? sizeText : formatText)
    readonly property var primaryTags: [
        { label: "Title", value: root.audioTitle },
        { label: "Artist", value: root.audioArtist },
        { label: "Album", value: root.audioAlbum },
        { label: "Year", value: root.audioYear },
        { label: "Track", value: root.audioTrack },
        { label: "Genre", value: root.audioGenre }
    ].filter(item => item.value.length > 0)
    readonly property bool hasAnyTags: primaryTags.length > 0 || audioComment.length > 0

    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? 10 : 18
        spacing: root.compact ? 10 : 14

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 150 : 230
            Layout.maximumHeight: parent.height
            radius: Theme.panelRadius
            color: Theme.withAlpha(Theme.secondaryAccent, themeController.isDark ? 0.12 : 0.09)
            border.color: Theme.withAlpha(Theme.secondaryAccent, themeController.isDark ? 0.34 : 0.26)
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: root.compact ? 12 : 16
                    spacing: root.compact ? 10 : 16

                    Rectangle {
                        Layout.preferredWidth: root.compact ? 78 : 132
                        Layout.preferredHeight: width
                        radius: Theme.radiusLg
                        color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.16 : 0.12)
                        border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.42 : 0.30)
                        border.width: 1

                        Image {
                            id: coverArt
                            anchors.fill: parent
                            source: root.coverPath.length > 0 ? "image://thumbnail/" + encodeURIComponent(root.coverPath + "::cover") : ""
                            sourceSize: Qt.size(root.compact ? 256 : 512, root.compact ? 256 : 512)
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                            smooth: true
                            visible: status === Image.Ready
                        }

                        Image {
                            anchors.centerIn: parent
                            visible: coverArt.status !== Image.Ready
                            source: "qrc:/qt/qml/FM/qml/assets/icons/music.svg"
                            sourceSize: Qt.size(root.compact ? 38 : 58, root.compact ? 38 : 58)
                            opacity: 0.88
                            smooth: true
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 8
                            width: formatLabel.implicitWidth + 12
                            height: 20
                            radius: Theme.radiusSm
                            color: Theme.withAlpha(Theme.bg, themeController.isDark ? 0.72 : 0.82)
                            visible: coverArt.status !== Image.Ready

                            Label {
                                id: formatLabel
                                anchors.centerIn: parent
                                text: root.formatText
                                font.pixelSize: 9
                                font.bold: true
                                color: Theme.textSecondary
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: root.compact ? 4 : 6

                        Label {
                            Layout.fillWidth: true
                            text: root.titleText
                            font.pixelSize: root.compact ? 16 : 24
                            font.bold: true
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.subtitleText
                            font.pixelSize: root.compact ? 12 : 15
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.metaText
                            font.pixelSize: root.compact ? 10 : 12
                            color: Theme.textSecondary
                            opacity: 0.84
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.formatText
                            font.pixelSize: 10
                            font.bold: true
                            color: Theme.warmAccent
                            elide: Text.ElideRight
                        }
                    }
                }

                Rectangle {
                    id: playbackControlsSlot
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.compact ? 42 : 46
                    visible: root.showDetails && root.multimediaControlsAvailable
                    color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.70 : 0.54)
                    border.width: 0

                    Loader {
                        id: playbackControlsLoader
                        anchors.fill: parent
                        active: playbackControlsSlot.visible && root.playbackControlsActive
                        source: "AudioPlaybackControls.qml"
                    }
                }

                Binding {
                    target: playbackControlsLoader.item
                    property: "path"
                    value: root.path
                    when: playbackControlsLoader.status === Loader.Ready
                }

                Binding {
                    target: playbackControlsLoader.item
                    property: "sourceUrl"
                    value: root.mediaSourceUrl
                    when: playbackControlsLoader.status === Loader.Ready
                }

                Binding {
                    target: playbackControlsLoader.item
                    property: "compact"
                    value: root.compact
                    when: playbackControlsLoader.status === Loader.Ready
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.showDetails
            radius: Theme.panelRadius
            color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
            border.color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.70 : 0.54)
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.compact ? 10 : 14
                spacing: root.compact ? 8 : 10

                Label {
                    Layout.fillWidth: true
                    text: "Tags"
                    font.pixelSize: root.compact ? 11 : 13
                    font.bold: true
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 2 : 3
                    rowSpacing: root.compact ? 6 : 8
                    columnSpacing: root.compact ? 6 : 8
                    visible: primaryRepeater.count > 0

                    Repeater {
                        id: primaryRepeater
                        model: root.primaryTags

                        TagCell {
                            label: modelData.label
                            value: modelData.value
                            prominent: true
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.compact ? 42 : 50
                    visible: !root.hasAnyTags
                    radius: Theme.radiusSm
                    color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
                    border.color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.58 : 0.44)
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 20
                        text: "No tags found"
                        font.pixelSize: root.compact ? 11 : 12
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: root.audioComment.length > 0
                    radius: Theme.radiusSm
                    color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
                    border.color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.58 : 0.44)
                    border.width: 1
                    implicitHeight: commentColumn.implicitHeight + 16

                    ColumnLayout {
                        id: commentColumn
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 3

                        Label {
                            Layout.fillWidth: true
                            text: "Comment"
                            font.pixelSize: 9
                            font.bold: true
                            color: Theme.textSecondary
                            opacity: 0.82
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.audioComment
                            color: Theme.textPrimary
                            font.pixelSize: 11
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }

    component TagCell: Rectangle {
        property string label: ""
        property string value: ""
        property bool prominent: false

        Layout.fillWidth: true
        Layout.preferredHeight: root.compact ? 42 : 52
        radius: Theme.radiusSm
        color: prominent
               ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.12 : 0.08)
               : (themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025))
        border.color: prominent
                      ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.34 : 0.24)
                      : Theme.withAlpha(Theme.border, themeController.isDark ? 0.58 : 0.44)
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.compact ? 7 : 9
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: parent.parent.label
                font.pixelSize: 9
                font.bold: true
                color: Theme.textSecondary
                opacity: 0.82
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: parent.parent.value
                font.pixelSize: root.compact ? 11 : 12
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }
        }
    }

}
