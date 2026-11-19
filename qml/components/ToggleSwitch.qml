import QtQuick
import QtQuick.Controls.Basic
import PersonalFinance

// iOS-style on/off switch. `toggled(checked)` fires only when the user flips it.
AbstractButton {
    id: root

    signal toggled(bool value)

    implicitWidth: 52
    implicitHeight: 32
    checkable: true
    opacity: enabled ? 1 : 0.45
    onClicked: root.toggled(root.checked)

    background: Rectangle {
        radius: height / 2
        color: root.checked ? Theme.income : Theme.field
        border.width: root.checked ? 0 : 1
        border.color: Theme.border
        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    contentItem: Item {
        Rectangle {
            width: 26
            height: 26
            radius: 13
            y: 3
            x: root.checked ? parent.width - width - 3 : 3
            color: "#FFFFFF"
            border.width: 1
            border.color: "#00000018"
            Behavior on x {
                NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
            }
        }
    }
}
