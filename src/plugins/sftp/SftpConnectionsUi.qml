import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM

Item {
    id: root
    property var pluginContext: ({})
    readonly property bool busy: card.busy
    readonly property string busyText: card.status
    readonly property real preferredContentHeight: card.implicitHeight + 32
    signal applyFinished(var result)
    function onHostClosed() { card.clearSensitiveInput() }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 16
        contentWidth: availableWidth
        SftpSettingsCard {
            id: card
            width: parent.width
            initialPath: String(root.pluginContext.currentPath || "")
            onStatusReported: (message, error) => root.applyFinished({ok: false, message: message, error: error})
            onNavigationRequested: (path, message, saved) => root.applyFinished({
                ok: saved, openPath: path, message: message
            })
        }
    }
}
