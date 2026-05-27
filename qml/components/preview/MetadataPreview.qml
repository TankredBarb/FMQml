import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string path: ""
    property string absolutePath: ""
    property string name: ""
    property string mimeName: ""
    property string extension: ""
    property bool directory: false
    property string sizeText: ""
    property string modifiedText: ""
    property bool hidden: false
    property bool symlink: false
    property string permissionsText: ""
    property string attributesText: ""
    property var extraProperties: []
    property string statusNote: ""
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings
                                                      ? appSettings.useHighQualitySystemIcons
                                                      : true

    clip: true

    readonly property string typeLabel: mimeName === "drive"
                                       ? extension.toUpperCase()
                                       : directory ? "Folder" : "File"
    readonly property bool showPathTags: path.length > 0 && path !== "devices://"

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 112
            radius: 14
            color: themeController.isDark ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                                         : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
            border.color: Theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Image {
                    source: root.showPathTags
                            ? "image://icon/" + encodeURIComponent(root.path + "?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
                            : "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
                    sourceSize: Qt.size(40, 40)
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    smooth: true
                    mipmap: false
                    opacity: 0.92
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: root.name.length > 0 ? root.name : "Item"
                        font.pixelSize: 14
                        font.bold: true
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                    }

                    Label {
                        text: root.typeLabel
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }

                    Label {
                        text: root.sizeText + "  |  " + root.modifiedText
                        font.pixelSize: 11
                        color: Theme.textSecondary
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: root.statusNote.length > 0
                        text: root.statusNote
                        font.pixelSize: 10
                        color: Theme.textSecondary
                        opacity: 0.82
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        visible: root.showPathTags
                        Layout.fillWidth: true
                        spacing: 6

                        Rectangle {
                            visible: root.hidden
                            radius: 8
                            color: Theme.surfaceHover
                            border.color: Theme.border
                            border.width: 1
                            implicitHeight: 20
                            implicitWidth: hiddenTag.implicitWidth + 14

                            Label {
                                id: hiddenTag
                                anchors.centerIn: parent
                                text: "Hidden"
                                font.pixelSize: 9
                                color: Theme.textSecondary
                            }
                        }

                        Rectangle {
                            visible: root.symlink
                            radius: 8
                            color: Theme.surfaceHover
                            border.color: Theme.border
                            border.width: 1
                            implicitHeight: 20
                            implicitWidth: linkTag.implicitWidth + 14

                            Label {
                                id: linkTag
                                anchors.centerIn: parent
                                text: "Symlink"
                                font.pixelSize: 9
                                color: Theme.textSecondary
                            }
                        }

                        Rectangle {
                            radius: 8
                            color: Theme.surfaceHover
                            border.color: Theme.border
                            border.width: 1
                            implicitHeight: 20
                            implicitWidth: accessTag.implicitWidth + 14

                            Label {
                                id: accessTag
                                anchors.centerIn: parent
                                text: root.permissionsText
                                font.pixelSize: 9
                                color: Theme.textSecondary
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        PreviewPropertiesList {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: root.directory ? "Folder Information" : "File Information"
            properties: {
                const props = [
                    { label: "Name", value: root.name },
                    { label: "Type", value: root.typeLabel }
                ]

                if (root.showPathTags) {
                    props.push({ label: "Location", value: root.absolutePath.length > 0 ? root.absolutePath : root.path })
                }

                if (root.sizeText.length > 0) {
                    props.push({ label: "Size", value: root.sizeText })
                }

                if (root.modifiedText.length > 0) {
                    props.push({ label: "Modified", value: root.modifiedText })
                }

                if (root.permissionsText.length > 0) {
                    props.push({ label: "Access", value: root.permissionsText })
                }

                if (root.attributesText.length > 0) {
                    props.push({ label: "Attributes", value: root.attributesText })
                }

                const extras = Array.isArray(root.extraProperties) ? root.extraProperties : []
                for (let i = 0; i < extras.length; i++) {
                    props.push(extras[i])
                }

                return props
            }
            rowRadius: 10
            rowPadding: 12
            labelPixelSize: 11
            valuePixelSize: 13
            rowSpacing: 10
        }
    }
}
