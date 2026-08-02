import QtQuick
import QtQuick.Controls
import "../../style"
import "../framework"

FmButton {
    id: root

    destructive: root.primaryColor === Theme.danger
}
