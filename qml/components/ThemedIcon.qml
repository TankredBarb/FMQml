import QtQuick

Image {
    id: root
    property color color: "#000000"
    
    sourceSize: Qt.size(16, 16)
    fillMode: Image.PreserveAspectFit
    
    // Временно отключим шейдер, чтобы проверить, отображаются ли иконки вообще.
    // Если они появятся (хоть и не того цвета), значит, проблема была в шейдере.
    layer.enabled: false
}
