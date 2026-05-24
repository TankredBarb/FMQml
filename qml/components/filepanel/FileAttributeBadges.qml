import QtQuick
import QtQuick.Controls
import "../../style"
import "../common"

Item {
    id: root

    property string attributesText: ""
    property bool showPlaceholder: true

    readonly property var badgesModel: {
        let badges = []
        const attrs = root.attributesText || ""
        if (attrs.indexOf('D') >= 0) badges.push({ letter: "D", color: "#3b82f6", tip: "Directory" })
        if (attrs.indexOf('H') >= 0) badges.push({ letter: "H", color: "#64748b", tip: "Hidden" })
        if (attrs.indexOf('R') >= 0) badges.push({ letter: "R", color: "#ef4444", tip: "Read-only" })
        if (attrs.indexOf('L') >= 0) badges.push({ letter: "L", color: "#8b5cf6", tip: "Symlink" })
        if (attrs.indexOf('S') >= 0) badges.push({ letter: "S", color: "#f59e0b", tip: "System" })
        return badges
    }

    Row {
        anchors.centerIn: parent
        spacing: 3
        visible: badgesRepeater.count > 0

        Repeater {
            id: badgesRepeater
            model: root.badgesModel

            InlineBadge {
                width: 16
                badgeHeight: 16
                horizontalPadding: 0
                text: modelData.letter
                textColor: modelData.color
                fontSize: 9
                fontWeight: Font.Bold
                fillColor: Qt.rgba(
                    Qt.color(modelData.color).r,
                    Qt.color(modelData.color).g,
                    Qt.color(modelData.color).b, 0.18)
                strokeColor: Qt.rgba(
                    Qt.color(modelData.color).r,
                    Qt.color(modelData.color).g,
                    Qt.color(modelData.color).b, 0.5)

                ToolTip.visible: attrMa.containsMouse
                ToolTip.text: modelData.tip
                ToolTip.delay: 500

                MouseArea {
                    id: attrMa
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        text: "—"
        color: Theme.textSecondary
        opacity: 0.3
        visible: root.showPlaceholder && badgesRepeater.count === 0
        font.pixelSize: 12
    }
}
