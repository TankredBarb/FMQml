import QtQuick
import QtQuick.Controls
import "../style"

ThemedContextMenu {
    id: root

    property int driveIndex: -1
    property string drivePath: ""
    property string driveType: ""
    property bool canEject: false
    property bool canUnmount: false
    property bool canSafelyRemove: false
    property bool canMount: false
    property string mountId: ""
    property bool actionPending: false
    property bool managedIsoMount: false

    signal openRequested(string path)
    signal analyzeRequested(string path)
    signal ejectRequested(string path, bool managedIsoMount)
    signal mountRequested(string mountId)
    signal propertiesRequested(string path)

    function reset() {
        driveIndex = -1
        drivePath = ""
        driveType = ""
        canEject = false
        canUnmount = false
        canSafelyRemove = false
        canMount = false
        mountId = ""
        actionPending = false
        managedIsoMount = false
    }

    ThemedMenuItem {
        text: "Open"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons/open.svg"
        iconColor: Theme.actionIconColor("open")
        onTriggered: root.openRequested(root.drivePath)
        visible: !root.canMount
    }

    ThemedMenuSeparator {}

    ThemedMenuItem {
        text: "Mount"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons/open.svg"
        iconColor: Theme.actionIconColor("open")
        visible: root.canMount
        enabled: !root.actionPending
        onTriggered: root.mountRequested(root.mountId)
    }

    ThemedMenuItem {
        text: "Analyze Disk Usage"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons/disk-usage.svg"
        iconColor: Theme.actionIconColor("analyze")
        visible: !root.canMount
        enabled: typeof diskUsageController !== "undefined"
                 && diskUsageController
                 && diskUsageController.canAnalyzePath(root.drivePath)
        onTriggered: root.analyzeRequested(root.drivePath)
    }

    ThemedMenuItem {
        text: root.managedIsoMount || root.canEject ? "Eject"
            : (root.canSafelyRemove ? "Safely Remove" : "Unmount")
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons/eject.svg"
        iconColor: Theme.actionIconColor("eject")
        visible: !root.canMount && (root.canEject || root.canUnmount || root.canSafelyRemove || root.managedIsoMount)
        enabled: visible && !root.actionPending
        onTriggered: root.ejectRequested(root.drivePath, root.managedIsoMount)
    }

    ThemedMenuSeparator {
        visible: !root.canMount && (root.canEject || root.canUnmount || root.canSafelyRemove || root.managedIsoMount)
    }

    ThemedMenuItem {
        text: "Properties"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons/info.svg"
        iconColor: Theme.actionIconColor("info")
        onTriggered: root.propertiesRequested(root.drivePath)
    }
}
