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
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.useNativeIcons
                                           : true

    clip: true

    readonly property string typeLabel: mimeName === "drive"
                                       ? extension.toUpperCase()
                                       : directory ? "Folder" : "File"
    readonly property bool showPathTags: path.length > 0 && path !== "devices://"

    function displayPath(path) {
        if (!path || String(path).length === 0) {
            return ""
        }
        const value = String(path)
        if (value.indexOf("archive://") === 0 || value.indexOf("devices://") === 0) {
            return value
        }
        return Qt.platform.os === "windows" ? value.replace(/\//g, "\\") : value
    }

    function supportsNativeIcon(path) {
        const value = String(path || "")
        return !isProviderIconPath(value)
               ? (value.indexOf("://") < 0 || value.indexOf("archive://") === 0)
               : true
    }

    function isProviderIconPath(path) {
        const value = String(path || "")
        const lower = value.toLowerCase()
        return value.indexOf("://") > 0
               && lower.indexOf("archive://") !== 0
               && lower.indexOf("file://") !== 0
               && value !== "devices://" && value !== "favorites://"
               && value !== "gdrive://" && value !== "selection://"
    }

    function nativeIconQuery(path) {
        let query = root.directory
            ? ("directory=true&hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
            : ("hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
        if (isProviderIconPath(path)) {
            query += "&provider=true"
        }
        if (root.extension.length > 0) {
            query += "&suffix=" + encodeURIComponent(root.extension)
        }
        if (root.mimeName.length > 0) {
            query += "&mime=" + encodeURIComponent(root.mimeName)
        }
        return query
    }

    function nativeIconOverrideForPath(path, directory) {
        const value = String(path || "")
        if (value.length === 0 || value === "devices://" || value === "favorites://"
                || value === "gdrive://" || value === "selection://") {
            return ""
        }
        return fileTypeIconResolver.nativeIconOverrideForPathHint(value, directory)
    }

    function nativeIconOverrideForIdentity(path, directory, suffix) {
        const overrideIcon = nativeIconOverrideForPath(path, directory)
        if (overrideIcon.length > 0) {
            return overrideIcon
        }
        const suffixValue = String(suffix || "")
        if (isProviderIconPath(path) && suffixValue.length > 0) {
            return nativeIconOverrideForPath("file." + suffixValue, directory)
        }
        return ""
    }

    function displayIconSource() {
        if (!root.showPathTags) {
            return "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
        }
        const overrideIcon = nativeIconOverrideForIdentity(root.path, root.directory, root.extension)
        if (overrideIcon.length > 0) {
            return overrideIcon
        }
        if (!root.useNativeIcons || !supportsNativeIcon(root.path)) {
            return fileTypeIconResolver.iconForSuffix(root.extension, root.directory)
        }
        return "image://icon/" + encodeURIComponent(root.path + "?" + nativeIconQuery(root.path))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 112
            radius: Theme.panelRadius
            color: themeController.isDark ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                                         : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
            border.color: Theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Image {
                    source: root.displayIconSource()
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
                        font.pixelSize: Theme.fontSizeSubtitle
                        font.bold: true
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                    }

                    Label {
                        text: root.typeLabel
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textSecondary
                    }

                    Label {
                        text: root.sizeText + "  |  " + root.modifiedText
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textSecondary
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: root.statusNote.length > 0
                        text: root.statusNote
                        font.pixelSize: Theme.fontSizeMicro
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
                            radius: Theme.radiusSm
                            color: Theme.surfaceHover
                            border.color: Theme.border
                            border.width: 1
                            implicitHeight: 20
                            implicitWidth: hiddenTag.implicitWidth + 14

                            Label {
                                id: hiddenTag
                                anchors.centerIn: parent
                                text: "Hidden"
                                font.pixelSize: Theme.scaledSize(9)
                                color: Theme.textSecondary
                            }
                        }

                        Rectangle {
                            visible: root.symlink
                            radius: Theme.radiusSm
                            color: Theme.surfaceHover
                            border.color: Theme.border
                            border.width: 1
                            implicitHeight: 20
                            implicitWidth: linkTag.implicitWidth + 14

                            Label {
                                id: linkTag
                                anchors.centerIn: parent
                                text: "Symlink"
                                font.pixelSize: Theme.scaledSize(9)
                                color: Theme.textSecondary
                            }
                        }

                        Rectangle {
                            radius: Theme.radiusSm
                            color: Theme.surfaceHover
                            border.color: Theme.border
                            border.width: 1
                            implicitHeight: 20
                            implicitWidth: accessTag.implicitWidth + 14

                            Label {
                                id: accessTag
                                anchors.centerIn: parent
                                text: root.permissionsText
                                font.pixelSize: Theme.scaledSize(9)
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
                    props.push({ label: "Location", value: root.displayPath(root.absolutePath.length > 0 ? root.absolutePath : root.path) })
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
            rowRadius: Theme.radiusMd
            rowPadding: 12
            labelPixelSize: 11
            valuePixelSize: 13
            rowSpacing: 10
        }
    }
}
