import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string path: ""
    property var extraProperties: []

    clip: true

    function getProperty(label) {
        if (!root.extraProperties) return "";
        for (let i = 0; i < root.extraProperties.length; i++) {
            if (root.extraProperties[i].label === label) {
                const value = root.extraProperties[i].value;
                return value === undefined || value === null ? "" : String(value);
            }
        }
        return "";
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 40

        ColumnLayout {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            spacing: 20
            Layout.alignment: Qt.AlignVCenter

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 240
                height: 240
                radius: 16
                color: themeController.isDark ? Theme.withAlpha(Theme.textPrimary, 0.05)
                                              : Theme.withAlpha(Theme.textPrimary, 0.03)
                border.color: Theme.panelBorder
                border.width: 1
                clip: true

                ImagePreview {
                    id: audioCoverArt
                    anchors.fill: parent
                    sourcePath: root.path
                    fillMode: Image.PreserveAspectCrop
                    sourceSizeWidth: 2048
                    sourceSizeHeight: 2048
                }

                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    visible: audioCoverArt.imageStatus !== Image.Ready

                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.accent }
                            GradientStop { position: 1.0; color: Qt.darker(Theme.accent, 1.6) }
                        }
                        opacity: 0.15
                    }

                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/FM/qml/assets/icons/music.svg"
                        sourceSize: Qt.size(64, 64)
                        opacity: 0.8
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: {
                        const title = root.getProperty("Title")
                        return title.length > 0 ? title : root.path.split(/[/\\]/).pop()
                    }
                    font.bold: true
                    font.pixelSize: 18
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                }

                Label {
                    text: {
                        const artist = root.getProperty("Artist")
                        return artist.length > 0 ? artist : "Unknown Artist"
                    }
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: Theme.accent
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                }

                Label {
                    text: {
                        const album = root.getProperty("Album")
                        return album.length > 0 ? album : "Unknown Album"
                    }
                    font.pixelSize: 12
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                    opacity: 0.8
                }
            }

            Item { Layout.fillHeight: true }
        }

        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: Theme.panelBorder
            opacity: 0.2
        }

        PreviewPropertiesList {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "Audio Information"
            properties: {
                const extras = Array.isArray(root.extraProperties) ? root.extraProperties : []
                return extras.filter(p => !["Title", "Artist", "Album"].includes(p.label))
            }
            rowRadius: 10
            rowPadding: 12
            labelPixelSize: 11
            valuePixelSize: 13
            rowSpacing: 10
        }
    }
}
