import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"

Rectangle {
        id: capabilityRow

        required property string label
        required property string value
        required property bool allowed
        property string accessState: allowed ? "allowed" : "denied"
        required property string description
        readonly property bool unknown: accessState === "unknown"
        readonly property color stateColor: unknown ? Theme.categoryInfo : (allowed ? Theme.success : Theme.warning)

        Layout.fillWidth: true
        implicitHeight: Math.max(58, capabilityLayout.implicitHeight + 16)
        radius: Theme.radiusSm
        color: capabilityMouse.containsMouse ? Theme.panelSurfaceSoft : Theme.panelSurface
        border.color: Theme.panelBorder
        border.width: 1

        MouseArea {
            id: capabilityMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }

        RowLayout {
            id: capabilityLayout
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 9
                Layout.preferredHeight: 34
                radius: 5
                color: capabilityRow.stateColor
                opacity: capabilityRow.allowed ? 0.82 : (capabilityRow.unknown ? 0.76 : 0.70)
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: capabilityRow.label
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Label {
                    text: capabilityRow.description
                    Layout.fillWidth: true
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                }
            }

            Label {
                text: capabilityRow.value
                color: capabilityRow.stateColor
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignRight
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

