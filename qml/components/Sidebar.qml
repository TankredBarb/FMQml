import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Pane {
    id: root
    
    padding: 0
    background: Rectangle {
        color: Theme.bg
        border.color: Theme.border
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 10
        spacing: 2

        Label {
            text: "Places"
            font.bold: true
            font.pixelSize: 11
            color: Theme.textSecondary
            Layout.leftMargin: 12
            Layout.bottomMargin: 4
        }

        ListView {
            id: placesList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: workspaceController.placesModel
            clip: true
            
            delegate: ItemDelegate {
                id: placeDelegate
                width: placesList.width
                height: 34
                
                contentItem: RowLayout {
                    spacing: 10
                    
                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        source: "image://icon/" + path
                        sourceSize: Qt.size(16, 16)
                        asynchronous: true
                        cache: true
                    }
                    
                    Label {
                        text: name
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }
                }
                
                background: Rectangle {
                    color: placeDelegate.down ? Theme.surfaceActive : (placeDelegate.hovered ? Theme.surfaceHover : "transparent")
                    radius: 4
                    anchors.fill: parent
                    anchors.margins: 2
                }
                
                onClicked: {
                    const active = workspaceController.activePanel === 0 
                                 ? workspaceController.leftPanel 
                                 : workspaceController.rightPanel
                    active.openPath(path)
                }
            }
        }
    }
}
