import QtQuick
import QtQuick.Controls
import FM
import "../../style"

AbstractButton {
    id: root

    implicitWidth: 245
    implicitHeight: visible ? 26 : 0
    leftPadding: 8
    rightPadding: 8
    topPadding: 0
    bottomPadding: 0
    clip: true

    property bool destructive: false
    property bool active: false
    property bool highlighted: root.activeFocus
    property string shortcut: ""
    property color iconColor: destructive ? Theme.danger : Theme.textSecondary
    property bool recolorEnabled: true
    property url iconFallbackSource: ""
    property bool iconCircular: false
    property bool useHighlightedState: false
    property real paintActivation: root.down ? 1
                                          : (root.hovered || (root.useHighlightedState && root.highlighted) ? 1 : 0)

    readonly property string displayText: root.text === undefined || root.text === null ? "" : String(root.text)
    readonly property string displayShortcut: root.shortcut
    function resolvedSource(value) {
        const source = value ? value.toString() : ""
        const misplacedQrcPrefix = "qrc:/qt/qml/FM/qml/components/assets/"
        if (source.indexOf(misplacedQrcPrefix) === 0) {
            return "qrc:/qt/qml/FM/qml/assets/" + source.slice(misplacedQrcPrefix.length)
        }
        return source.indexOf("../assets/") === 0 ? "../../assets/" + source.slice(10) : source
    }

    readonly property string iconSourceText: root.resolvedSource(root.icon && root.icon.source
                                                                  ? root.icon.source : "")
    readonly property string iconFallbackSourceText: root.resolvedSource(root.iconFallbackSource)
    readonly property bool hasIconSource: iconSourceText.length > 0
    readonly property bool useSvgRecolor: hasIconSource
                                           && iconSourceText.toLowerCase().endsWith(".svg")
                                           && root.recolorEnabled

    signal triggered()

    onPressed: root.triggered()

    Keys.onSpacePressed: event => {
        root.triggered()
        event.accepted = true
    }
    Keys.onReturnPressed: event => {
        root.triggered()
        event.accepted = true
    }
    Keys.onEnterPressed: event => {
        root.triggered()
        event.accepted = true
    }

    Accessible.role: Accessible.MenuItem

    background: FmMenuItemVisual {
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        activation: root.paintActivation
        pressed: root.down
        hoverColor: Theme.menuItemHover
        pressedColor: Theme.menuItemPressed
        accentColor: root.active ? root.iconColor : (root.destructive ? Theme.danger : Theme.accent)
    }

    contentItem: FmMenuItemContent {
        control: root
    }

    Behavior on paintActivation {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }
}
