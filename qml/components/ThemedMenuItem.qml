import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

MenuItem {
    id: root

    implicitWidth: 196
    implicitHeight: 26
    clip: true

    property bool destructive: false
    property color iconColor: destructive ? Theme.danger : Theme.textSecondary

    readonly property string displayText: {
        const t = root.text
        return (t === undefined || t === null) ? "" : String(t)
    }
    readonly property string displayShortcut: {
        const s = root["shortcut"]
        return (s === undefined || s === null) ? "" : String(s)
    }

    readonly property color hoverFill: Theme.menuItemHover
    readonly property color pressedFill: Theme.menuItemPressed

    background: Item {
        anchors.fill: parent
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        anchors.topMargin: 0
        anchors.bottomMargin: 0
        clip: true

        Rectangle {
            anchors.fill: parent
            anchors.margins: 0
            color: !root.enabled
                    ? "transparent"
                    : root.down
                            ? root.pressedFill
                            : root.hovered
                                    ? root.hoverFill
                                    : "transparent"
            radius: 2
        }

        Rectangle {
            width: 3
            height: Math.max(10, Math.round(parent.height * 0.58))
            radius: 2
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            color: root.destructive ? Theme.danger : Theme.accent
            opacity: root.enabled && (root.hovered || root.down) ? 1 : 0
        }
    }

    contentItem: RowLayout {
        spacing: 6
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        Image {
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            source: root.icon ? root.icon.source : ""
            sourceSize: Qt.size(16, 16)
            smooth: false
            mipmap: false
            visible: root.icon && root.icon.source.toString().length > 0
            opacity: root.enabled ? 1.0 : 0.35
        }

        Label {
            text: root.displayText
            color: !root.enabled ? Theme.textSecondary
                   : (root.destructive && root.hovered) ? Theme.danger : Theme.textPrimary
            font.pixelSize: 12
            font.letterSpacing: -0.2
            font.weight: root.highlighted ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Label {
            text: root.displayShortcut
            color: Theme.textSecondary
            font.pixelSize: 10
            font.letterSpacing: -0.1
            font.italic: true
            verticalAlignment: Text.AlignVCenter
            visible: root.displayShortcut.length > 0
            Layout.alignment: Qt.AlignRight
        }
    }
}
