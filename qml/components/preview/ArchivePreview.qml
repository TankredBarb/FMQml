import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string path: ""
    property string name: ""
    property string sizeText: ""
    property string modifiedText: ""
    property string mimeName: ""
    property string extension: ""
    property var extraProperties: []
    property bool compact: false
    property bool showDetails: false
    property bool useNativeIcons: true

    readonly property string formatText: {
        const format = extraValue("Format")
        if (format.length > 0) return format
        if (extension.length > 0) return extension.toUpperCase()
        return "ARCHIVE"
    }
    readonly property string entriesText: extraValue("Entries")
    readonly property string filesText: extraValue("Files")
    readonly property string foldersText: extraValue("Folders")
    readonly property string uncompressedText: extraValue("Uncompressed")
    readonly property string packedText: extraValue("Packed")
    readonly property string compressedText: extraValue("Compressed")
    readonly property string ratioText: extraValue("Archive Ratio")
    readonly property string encryptedText: extraValue("Encrypted")
    readonly property string commentText: extraValue("Comment")
    readonly property string subtitleText: entriesText.length > 0
                                      ? entriesText + " entries"
                                      : (mimeName.length > 0 ? mimeName : "Compressed file")
    readonly property string metaText: compressedText.length > 0
                                   ? compressedText
                                   : (sizeText.length > 0 ? sizeText : formatText)
    readonly property string fallbackIconSource: {
        if (root.path.length > 0) {
            return fileTypeIconResolver.iconForPathHint(root.path, false)
        }
        return fileTypeIconResolver.iconForSuffix(root.extension, false)
    }
    readonly property string iconSource: {
        const overrideIcon = nativeIconOverrideForIdentity(root.path, root.extension)
        if (overrideIcon.length > 0) {
            return overrideIcon
        }
        if (root.useNativeIcons && root.path.length > 0) {
            if (!supportsNativeIcon(root.path)) {
                return root.fallbackIconSource
            }
            return "image://icon/" + encodeURIComponent(root.path + "?" + nativeIconQuery(root.path))
        }
        return root.fallbackIconSource
    }

    clip: true

    function safeText(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function extraValue(label) {
        const extras = root.extraProperties || []
        const count = extras.length !== undefined ? extras.length : (extras.count !== undefined ? extras.count : 0)
        for (let i = 0; i < count; i++) {
            if (safeText(extras[i].label) === label) {
                return safeText(extras[i].value)
            }
        }
        return ""
    }

    function nativeIconOverrideForPath(path) {
        const value = String(path || "")
        if (value.length === 0 || value === "devices://" || value === "favorites://"
                || value === "gdrive://" || value === "selection://") {
            return ""
        }
        return fileTypeIconResolver.nativeIconOverrideForPathHint(value, false)
    }

    function nativeIconOverrideForIdentity(path, suffix) {
        const overrideIcon = nativeIconOverrideForPath(path)
        if (overrideIcon.length > 0) {
            return overrideIcon
        }
        const suffixValue = String(suffix || "")
        if (isProviderIconPath(path) && suffixValue.length > 0) {
            return nativeIconOverrideForPath("file." + suffixValue)
        }
        return ""
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
        let query = ""
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

    function metricItems() {
        const items = []
        function addMetric(label, value) {
            const text = safeText(value)
            if (text.length > 0) {
                items.push({ label: label, value: text })
            }
        }

        addMetric("Entries", root.entriesText)
        addMetric("Files", root.filesText)
        addMetric("Folders", root.foldersText)
        addMetric("Unpacked", root.uncompressedText)
        addMetric("Packed", root.packedText)
        addMetric("Ratio", root.ratioText)
        addMetric("Encrypted", root.encryptedText)
        addMetric("Size", root.sizeText)
        addMetric("Modified", root.modifiedText)

        return root.showDetails ? items : items.slice(0, 6)
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.compact ? 8 : 18
        radius: Theme.panelRadius
        color: Theme.withAlpha(Theme.secondaryAccent, themeController.isDark ? 0.12 : 0.09)
        border.color: Theme.withAlpha(Theme.secondaryAccent, themeController.isDark ? 0.34 : 0.26)
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.compact ? 10 : 22
            spacing: root.compact ? 7 : 16

            RowLayout {
                Layout.fillWidth: true
                spacing: root.compact ? 9 : 18

                Rectangle {
                    Layout.preferredWidth: root.compact ? 56 : 116
                    Layout.preferredHeight: width
                    radius: Theme.radiusLg
                    color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.16 : 0.12)
                    border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.42 : 0.30)
                    border.width: 1

                    Item {
                        anchors.centerIn: parent
                        width: root.compact ? 38 : 76
                        height: width

                        Image {
                            id: primaryIcon
                            anchors.fill: parent
                            source: root.iconSource
                            sourceSize: Qt.size(parent.width, parent.height)
                            smooth: true
                            opacity: 0.92
                            visible: root.iconSource.length > 0 && status !== Image.Error
                        }

                        Image {
                            anchors.fill: parent
                            source: root.fallbackIconSource
                            sourceSize: Qt.size(parent.width, parent.height)
                            smooth: true
                            opacity: 0.92
                            visible: root.fallbackIconSource.length > 0 && (root.iconSource.length === 0 || primaryIcon.status === Image.Error)
                        }
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
                        text: root.name.length > 0 ? root.name : "Archive"
                        font.pixelSize: root.compact ? 14 : 24
                        font.bold: true
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.subtitleText
                        font.pixelSize: root.compact ? 11 : 15
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.metaText
                        font.pixelSize: root.compact ? 10 : 12
                        color: Theme.textSecondary
                        opacity: 0.86
                        elide: Text.ElideRight
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                rowSpacing: root.compact ? 6 : 8
                columnSpacing: root.compact ? 6 : 8
                visible: metricsRepeater.count > 0

                Repeater {
                    id: metricsRepeater
                    model: root.metricItems()

                    MetricPill {
                        label: modelData.label
                        value: modelData.value
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !root.compact && root.commentText.length > 0
                text: root.commentText
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeLabel
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                elide: Text.ElideRight
                maximumLineCount: 3
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }

    component MetricPill: Rectangle {
        property string label: ""
        property string value: ""

        Layout.fillWidth: true
        Layout.preferredHeight: root.compact ? 32 : 42
        radius: Theme.radiusSm
        color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
        border.color: Theme.border
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.compact ? 5 : 8
            spacing: 1

            Label {
                Layout.fillWidth: true
                text: parent.parent.label
                font.pixelSize: root.compact ? 8 : 9
                font.bold: true
                color: Theme.textSecondary
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: parent.parent.value
                font.pixelSize: root.compact ? 10 : 11
                font.bold: true
                color: Theme.textPrimary
                elide: Text.ElideRight
            }
        }
    }
}
