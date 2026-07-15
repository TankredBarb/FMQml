import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string type: "executable"
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

    readonly property bool shortcut: type === "shortcut"
    readonly property string formatText: shortcut ? "LNK" : (extension.length > 0 ? extension.toUpperCase() : "APP")
    readonly property string productText: firstValue(["Product", "Description"])
    readonly property string companyText: extraValue("Company")
    readonly property string versionText: firstValue(["File Version", "Product Version"])
    readonly property string targetText: extraValue("Target")
    readonly property string workingDirectoryText: extraValue("Working Directory")
    readonly property string argumentsText: extraValue("Arguments")
    readonly property string commentText: firstValue(["Comment", "Description"])
    readonly property string titleText: {
        if (root.productText.length > 0 && !root.shortcut) return root.productText
        if (root.name.length > 0) return root.name
        return root.shortcut ? "Shortcut" : "Application"
    }
    readonly property string subtitleText: {
        if (root.shortcut) {
            return root.targetText.length > 0 ? displayPath(root.targetText) : "Windows shortcut"
        }
        if (root.companyText.length > 0) return root.companyText
        return root.mimeName.length > 0 ? root.mimeName : "Windows executable"
    }
    readonly property string metaText: {
        if (root.versionText.length > 0 && root.sizeText.length > 0) return root.versionText + "  |  " + root.sizeText
        if (root.versionText.length > 0) return root.versionText
        if (root.sizeText.length > 0) return root.sizeText
        return root.formatText
    }
    readonly property string iconSource: {
        if (root.path.length === 0) return ""
        if (root.useNativeIcons) {
            return "image://icon/" + encodeURIComponent(root.path)
        }
        return fileTypeIconResolver.iconForSuffix(root.extension, false)
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

    function firstValue(labels) {
        for (let i = 0; i < labels.length; i++) {
            const value = extraValue(labels[i])
            if (value.length > 0) return value
        }
        return ""
    }

    function displayPath(path) {
        const value = safeText(path)
        if (value.length === 0 || value.indexOf("archive://") === 0 || value.indexOf("devices://") === 0) {
            return value
        }
        return Qt.platform.os === "windows" ? value.replace(/\//g, "\\") : value
    }

    function metricItems() {
        const items = []
        function addMetric(label, value) {
            const text = safeText(value)
            if (text.length > 0) {
                items.push({ label: label, value: text })
            }
        }

        if (root.shortcut) {
            addMetric("Target", root.targetText.length > 0 ? displayPath(root.targetText) : "")
            addMetric("Work Dir", root.workingDirectoryText.length > 0 ? displayPath(root.workingDirectoryText) : "")
            addMetric("Arguments", root.argumentsText)
            addMetric("Comment", root.commentText)
            addMetric("Size", root.sizeText)
            addMetric("Modified", root.modifiedText)
        } else {
            addMetric("Company", root.companyText)
            addMetric("Version", root.versionText)
            addMetric("Product", root.productText)
            addMetric("Original", extraValue("Original Name"))
            addMetric("Size", root.sizeText)
            addMetric("Modified", root.modifiedText)
        }

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

                    Image {
                        anchors.centerIn: parent
                        source: root.iconSource
                        sourceSize: Qt.size(root.compact ? 40 : 80, root.compact ? 40 : 80)
                        smooth: true
                        opacity: 0.94
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
                        elide: Text.ElideMiddle
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
                elide: Text.ElideMiddle
            }
        }
    }
}
