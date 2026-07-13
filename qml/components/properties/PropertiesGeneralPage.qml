import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"

ScrollView {
    id: page
    required property int currentIndex
    required property var rows
    required property string sectionTitle
    readonly property real contentImplicitHeight: contentLayout.implicitHeight
    anchors.fill: parent
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    clip: true
    enabled: currentIndex === 0
    opacity: currentIndex === 0 ? 1.0 : 0.0
    z: currentIndex === 0 ? 1 : 0
    transform: Translate {
        x: page.currentIndex === 0 ? 0 : (0 < page.currentIndex ? -400 : 400)
        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    }
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.InOutQuad } }
    ColumnLayout {
        id: contentLayout
        x: 16
        y: contentLayout.implicitHeight >= page.availableHeight ? 4
           : Math.max(4, Math.round((page.availableHeight - contentLayout.implicitHeight) / 2))
        width: page.availableWidth - 32
        spacing: 12
        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
        DialogSection {
            title: page.sectionTitle
            visible: page.rows.length > 0
            Repeater {
                model: page.rows
                DialogListRow {
                    required property var modelData
                    label: modelData && modelData.label ? modelData.label : ""
                    value: modelData && modelData.value ? modelData.value : ""
                    isLink: modelData && (modelData.key === "general.location"
                                                  || modelData.key === "general.fullPath"
                                                  || modelData.key === "selection.location")
                    valueMaximumLineCount: isLink ? 4 : 2
                    showBusy: modelData && modelData.busy ? true : false
                    accentColor: Theme.accent
                    emphasizeValue: modelData && modelData.emphasize ? true : false
                }
            }
        }
        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
    }
}

