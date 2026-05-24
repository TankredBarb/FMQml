import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

ScrollView {
    id: root

    property var properties: []
    property string title: ""
    property int rowRadius: 12
    property int rowPadding: 14
    property int labelPixelSize: 10
    property int valuePixelSize: 12
    property int rowSpacing: 8

    background: null
    clip: true
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    function safeText(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: root.rowSpacing

        Label {
            visible: root.title.length > 0
            text: root.title
            font.bold: true
            font.pixelSize: 14
            color: Theme.textPrimary
            Layout.bottomMargin: 4
            Layout.fillWidth: true
        }

        Repeater {
            model: root.properties

            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.preferredWidth: root.availableWidth
                radius: root.rowRadius
                color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
                border.color: Theme.border
                border.width: 1
                implicitHeight: valueColumn.implicitHeight + (root.rowPadding * 2)

                Item {
                    anchors.fill: parent
                    anchors.margins: root.rowPadding

                    ColumnLayout {
                        id: valueColumn
                        width: parent.width
                        spacing: 4

                        Label {
                            text: root.safeText(modelData.label)
                            font.pixelSize: root.labelPixelSize
                            font.bold: true
                            color: Theme.textSecondary
                            opacity: 0.88
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            width: parent.width
                            text: {
                                const value = root.safeText(modelData.value)
                                return value.length > 0 ? value : "-"
                            }
                            color: Theme.textPrimary
                            font.pixelSize: root.valuePixelSize
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            horizontalAlignment: Text.AlignLeft
                        }
                    }
                }
            }
        }
    }
}
