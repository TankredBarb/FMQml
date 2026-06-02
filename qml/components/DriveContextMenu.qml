import QtQuick
import QtQuick.Controls
import "../style"

ThemedContextMenu {
    id: root

    property int driveIndex: -1
    property string drivePath: ""
    property string driveType: ""
    property bool canEject: false
    property bool managedIsoMount: false

    signal openRequested(string path)
    signal analyzeRequested(string path)
    signal ejectRequested(string path, bool managedIsoMount)
    signal propertiesRequested(string path)

    function reset() {
        driveIndex = -1
        drivePath = ""
        driveType = ""
        canEject = false
        managedIsoMount = false
    }

    ThemedMenuItem {
        text: "Open"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons/open.svg"
        iconColor: Theme.actionIconColor("open")
        onTriggered: root.openRequested(root.drivePath)
    }

    ThemedMenuSeparator {}

    ThemedMenuItem {
        text: "Analyze Disk Usage"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons/disk-usage.svg"
        iconColor: Theme.actionIconColor("analyze")
        enabled: typeof diskUsageController !== "undefined"
                 && diskUsageController
                 && diskUsageController.canAnalyzePath(root.drivePath)
        onTriggered: root.analyzeRequested(root.drivePath)
    }

    ThemedMenuItem {
        text: "Eject"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons/eject.svg"
        iconColor: Theme.actionIconColor("eject")
        visible: root.canEject || root.managedIsoMount || root.driveType === "usb" || root.driveType === "optical"
        enabled: visible
        onTriggered: root.ejectRequested(root.drivePath, root.managedIsoMount)
    }

    ThemedMenuSeparator {
        visible: root.canEject || root.managedIsoMount || root.driveType === "usb" || root.driveType === "optical"
    }

    ThemedMenuItem {
        text: "Properties"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons/info.svg"
        iconColor: Theme.actionIconColor("info")
        onTriggered: root.propertiesRequested(root.drivePath)
    }
}
