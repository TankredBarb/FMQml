import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string name: ""
    property string content: ""
    property string sizeText: ""
    property string modifiedText: ""
    property string extension: ""
    property var extraProperties: []
    property string coverSource: ""
    property string iconSource: "qrc:/qt/qml/FM/qml/assets/filetypes-next/document.svg"
    property string bookTitle: ""
    property string bookAuthor: ""
    property int pageIndex: 0
    property int pageCount: 0
    property bool compact: false
    property bool showDetails: false
    property bool loading: false
    property int readerPixelSize: compact ? 12 : 17

    readonly property string metadataTitleText: bookTitle.length > 0 ? bookTitle : extraValue("Title")
    readonly property string authorText: bookAuthor.length > 0 ? bookAuthor : extraValue("Author")
    readonly property string titleText: metadataTitleText.length > 0
                                        ? metadataTitleText
                                        : (authorText.length > 0 ? "" : (name.length > 0 ? name : "Book"))
    readonly property string genreText: extraValue("Genre")
    readonly property string dateText: extraValue("Date")
    readonly property string seriesText: extraValue("Series")
    readonly property string languageText: extraValue("Language")
    readonly property string annotationText: extraValue("Annotation")
    readonly property string formatText: extension.length > 0 ? extension.toUpperCase() : "FB2"
    readonly property color bookAccent: Theme.actionIconColor("text-file")
    readonly property color paperColor: themeController.isDark ? Theme.panelSurfaceStrong : Theme.bg
    readonly property color inkColor: Theme.textPrimary
    readonly property color mutedInkColor: Theme.textSecondary
    readonly property color ruleColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.58 : 0.46)
    readonly property int defaultReaderPixelSize: compact ? 12 : 17

    signal pageRequested(int pageIndex)
    signal readerSizeChanged(int pixelSize)

    clip: true

    function safeText(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function extraValue(label) {
        const extras = Array.isArray(root.extraProperties) ? root.extraProperties : []
        for (let i = 0; i < extras.length; i++) {
            if (safeText(extras[i].label) === label) {
                return safeText(extras[i].value)
            }
        }
        return ""
    }

    function metaItems() {
        const items = []
        if (root.genreText.length > 0) items.push(root.genreText)
        if (root.dateText.length > 0) items.push(root.dateText)
        if (root.seriesText.length > 0) items.push(root.seriesText)
        if (root.languageText.length > 0) items.push(root.languageText.toUpperCase())
        return items
    }

    function metricItems() {
        const items = []
        function addMetric(label, value) {
            const text = safeText(value)
            if (text.length > 0) {
                items.push({ label: label, value: text })
            }
        }

        addMetric("Author", root.authorText)
        addMetric("Title", root.metadataTitleText)
        addMetric("Format", root.formatText)
        if (!root.showDetails) {
            return items
        }
        addMetric("Genre", root.genreText)
        addMetric("Date", root.dateText)
        addMetric("Series", root.seriesText)
        addMetric("Language", root.languageText.toUpperCase())

        return items
    }

    function adjustReaderSize(delta) {
        root.readerPixelSize = Math.max(root.compact ? 10 : 13,
                                        Math.min(root.compact ? 16 : 24, root.readerPixelSize + delta))
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.compact ? 8 : 18
        radius: Theme.panelRadius
        color: root.paperColor
        border.color: Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.34 : 0.24)
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.compact ? 13 : 24
            spacing: root.compact ? 9 : 16

            RowLayout {
                Layout.fillWidth: true
                spacing: root.compact ? 10 : 18

                Rectangle {
                    Layout.preferredWidth: root.compact ? 48 : 84
                    Layout.preferredHeight: root.compact ? 64 : 112
                    radius: Theme.radiusLg
                    color: Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.20 : 0.14)
                    border.color: Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.48 : 0.34)
                    border.width: 1

                    Image {
                        anchors.centerIn: parent
                        width: root.coverSource.length > 0 ? parent.width : parent.width * 0.52
                        height: root.coverSource.length > 0 ? parent.height : width
                        source: root.coverSource.length > 0
                                ? root.coverSource
                                : root.iconSource
                        sourceSize: root.coverSource.length > 0
                                    ? Qt.size(root.compact ? 128 : 320, root.compact ? 192 : 480)
                                    : Qt.size(64, 64)
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        opacity: 0.86
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: root.compact ? 6 : 9
                        text: root.formatText
                        color: Theme.readableOn(parent.color, root.bookAccent)
                        font.pixelSize: root.compact ? 8 : 10
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: root.compact ? 4 : 8

                    Label {
                        Layout.fillWidth: true
                        visible: root.titleText.length > 0
                        text: root.titleText
                        color: root.inkColor
                        font.pixelSize: root.compact ? 15 : 25
                        font.bold: true
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.authorText.length > 0
                        text: root.authorText
                        color: root.mutedInkColor
                        font.pixelSize: root.compact ? 11 : 15
                        elide: Text.ElideRight
                    }

                    PreviewMetaStrip {
                        Layout.fillWidth: true
                        visible: root.metaItems().length > 0
                        compact: root.compact
                        accentColor: root.bookAccent
                        items: root.metaItems()
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

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(annotationLabel.implicitHeight + 22, root.compact ? 82 : 132)
                visible: root.annotationText.length > 0 && root.showDetails
                radius: Theme.radiusSm
                color: Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.10 : 0.07)
                border.color: Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.28 : 0.20)
                border.width: 1
                clip: true

                Text {
                    id: annotationLabel
                    anchors.fill: parent
                    anchors.margins: 11
                    text: root.annotationText
                    color: root.mutedInkColor
                    font.pixelSize: root.compact ? 11 : 13
                    lineHeight: 1.16
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    elide: Text.ElideRight
                }
            }

            Flickable {
                id: bookFlickable
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.showDetails
                clip: true
                contentWidth: width
                contentHeight: bookText.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                Connections {
                    target: root
                    function onPageIndexChanged() {
                        bookFlickable.contentY = 0
                    }
                    function onContentChanged() {
                        bookFlickable.contentY = 0
                    }
                }

                Text {
                    id: bookText
                    width: parent.width
                    text: root.loading ? "Loading book..." : root.content
                    color: root.inkColor
                    font.pixelSize: root.readerPixelSize
                    lineHeight: root.compact ? 1.18 : 1.24
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.showDetails
                spacing: 5

                Rectangle {
                    visible: root.pageCount > 1
                    Layout.fillWidth: true
                    Layout.preferredHeight: 2
                    radius: 1
                    color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.18 : 0.11)
                    clip: true

                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(1, (root.pageIndex + 1) / Math.max(1, root.pageCount)))
                        height: parent.height
                        radius: parent.radius
                        color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.82 : 0.68)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Label {
                        visible: root.showDetails && root.pageCount > 1
                        text: "Page"
                        color: root.mutedInkColor
                        font.pixelSize: Theme.scaledSize(9)
                        font.bold: true
                    }

                    ReaderControlButton {
                        visible: root.showDetails && root.pageCount > 1
                        text: "<"
                        enabled: root.pageIndex > 0
                        onClicked: root.pageRequested(root.pageIndex - 1)
                    }

                    TextField {
                        id: pageInput

                        visible: root.showDetails && root.pageCount > 1
                        text: ""
                        validator: IntValidator {
                            bottom: 1
                            top: Math.max(1, root.pageCount)
                        }
                        selectByMouse: true
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        implicitWidth: 54
                        implicitHeight: 24
                        color: root.inkColor
                        selectionColor: Theme.withAlpha(root.bookAccent, 0.34)
                        selectedTextColor: root.inkColor
                        font.pixelSize: Theme.fontSizeMicro
                        font.bold: true
                        padding: 0

                        function applyPage() {
                            const parsed = parseInt(text, 10)
                            if (isNaN(parsed)) {
                                text = String(root.pageIndex + 1)
                                return
                            }

                            const clamped = Math.max(1, Math.min(root.pageCount, parsed))
                            text = String(clamped)
                            if (clamped !== root.pageIndex + 1) {
                                root.pageRequested(clamped - 1)
                            }
                        }

                        Component.onCompleted: text = String(Math.min(root.pageIndex + 1, Math.max(1, root.pageCount)))
                        onAccepted: applyPage()
                        Keys.onReturnPressed: (event) => {
                            applyPage()
                            event.accepted = true
                        }
                        Keys.onEnterPressed: (event) => {
                            applyPage()
                            event.accepted = true
                        }
                        onActiveFocusChanged: {
                            if (!activeFocus) {
                                applyPage()
                            }
                        }

                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: pageInput.activeFocus
                                   ? Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.16 : 0.10)
                                   : Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.08 : 0.05)
                            border.color: pageInput.activeFocus
                                          ? Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.44 : 0.34)
                                          : Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.22 : 0.16)
                            border.width: 1
                        }

                        Connections {
                            target: root
                            function onPageIndexChanged() {
                                if (!pageInput.activeFocus) {
                                    pageInput.text = String(root.pageIndex + 1)
                                }
                            }
                            function onPageCountChanged() {
                                if (!pageInput.activeFocus) {
                                    pageInput.text = String(Math.min(root.pageIndex + 1, Math.max(1, root.pageCount)))
                                }
                            }
                        }
                    }

                    Label {
                        visible: root.showDetails && root.pageCount > 1
                        text: "/ " + root.pageCount + " pages"
                        color: root.mutedInkColor
                        font.pixelSize: Theme.fontSizeMicro
                        font.bold: true
                    }

                    ReaderControlButton {
                        visible: root.showDetails && root.pageCount > 1
                        text: ">"
                        enabled: root.pageIndex < root.pageCount - 1
                        onClicked: root.pageRequested(root.pageIndex + 1)
                    }

                    Rectangle {
                        visible: root.pageCount > 1
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: 18
                        color: root.ruleColor
                    }

                    ReaderControlButton {
                        visible: root.showDetails
                        text: "A-"
                        enabled: root.readerPixelSize > (root.compact ? 10 : 13)
                        onClicked: {
                            root.adjustReaderSize(-1)
                            root.readerSizeChanged(root.readerPixelSize)
                        }
                    }

                    ReaderControlButton {
                        visible: root.showDetails
                        text: "A+"
                        enabled: root.readerPixelSize < (root.compact ? 16 : 24)
                        onClicked: {
                            root.adjustReaderSize(1)
                            root.readerSizeChanged(root.readerPixelSize)
                        }
                    }

                    ReaderControlButton {
                        visible: root.showDetails
                        text: "Reset"
                        implicitWidth: 44
                        enabled: root.readerPixelSize !== root.defaultReaderPixelSize
                        onClicked: {
                            root.readerPixelSize = root.defaultReaderPixelSize
                            root.readerSizeChanged(root.readerPixelSize)
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }
    }

    component ReaderControlButton: Button {
        id: controlButton

        implicitWidth: 28
        implicitHeight: 24
        padding: 0
        hoverEnabled: true

        contentItem: Label {
            text: controlButton.text
            color: controlButton.enabled ? root.bookAccent : Theme.withAlpha(root.mutedInkColor, 0.48)
            font.pixelSize: Theme.scaledSize(9)
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: controlButton.down
                   ? Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.24 : 0.16)
                   : (controlButton.hovered ? Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.16 : 0.10) : "transparent")
            border.color: controlButton.hovered ? Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.40 : 0.30) : "transparent"
            border.width: controlButton.hovered ? 1 : 0
        }
    }

    component MetricPill: Rectangle {
        property string label: ""
        property string value: ""

        Layout.fillWidth: true
        Layout.preferredHeight: root.compact ? 32 : 42
        radius: Theme.radiusSm
        color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
        border.color: Theme.withAlpha(root.bookAccent, themeController.isDark ? 0.32 : 0.22)
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.compact ? 5 : 8
            spacing: 1

            Label {
                Layout.fillWidth: true
                text: parent.parent.label
                font.pixelSize: root.compact ? 8 : 9
                font.bold: true
                color: root.mutedInkColor
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: parent.parent.value
                font.pixelSize: root.compact ? 10 : 11
                font.bold: true
                color: root.inkColor
                elide: Text.ElideRight
            }
        }
    }
}
