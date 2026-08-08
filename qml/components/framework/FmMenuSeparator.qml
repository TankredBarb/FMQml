import QtQuick
import QtQuick.Controls
import FM
import "../../style"

MenuSeparator {
    id: root

    implicitHeight: visible ? 10 : 0
    padding: 0

    contentItem: FmMenuSeparatorVisual {
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        lineColor: Theme.withAlpha(Theme.menuSeparator, themeController.isDark ? 0.66 : 0.52)
        highlightColor: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.055 : 0.04)
    }
}
