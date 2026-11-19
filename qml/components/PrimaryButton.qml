import QtQuick
import QtQuick.Controls.Basic
import PersonalFinance

AbstractButton {
    id: root

    property bool destructive: false
    property bool secondary: false

    implicitHeight: 54
    implicitWidth: Math.max(120, label.implicitWidth + 40)
    scale: down ? 0.97 : 1
    opacity: enabled ? 1 : 0.45

    Behavior on scale {
        NumberAnimation { duration: 90; easing.type: Easing.OutQuad }
    }

    background: Rectangle {
        radius: 16
        color: root.destructive ? Theme.alpha(Theme.expense, 0.14)
             : root.secondary ? Theme.field
             : Theme.accent
    }

    contentItem: Text {
        id: label
        text: root.text
        font.pixelSize: 17
        font.weight: Font.DemiBold
        color: root.destructive ? Theme.expense
             : root.secondary ? Theme.text
             : Theme.textOnAccent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
