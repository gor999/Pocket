import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PersonalFinance

// Labelled single-line text field.
ColumnLayout {
    id: root

    property string label: ""
    property alias text: input.text
    property alias placeholderText: input.placeholderText
    property alias inputMethodHints: input.inputMethodHints
    property alias validator: input.validator
    property alias maximumLength: input.maximumLength
    property alias field: input
    property bool invalid: false

    signal accepted()

    spacing: 6

    Text {
        visible: root.label.length > 0
        text: root.label
        font.pixelSize: Theme.fontCaption
        font.weight: Font.Medium
        color: Theme.textSecondary
    }

    TextField {
        id: input
        Layout.fillWidth: true
        implicitHeight: 50
        leftPadding: 14
        rightPadding: 14
        font.pixelSize: Theme.fontBody
        color: Theme.text
        placeholderTextColor: Theme.textTertiary
        selectionColor: Theme.accent
        selectedTextColor: Theme.textOnAccent
        onAccepted: root.accepted()

        background: Rectangle {
            radius: Theme.radiusField
            color: Theme.field
            border.width: 1.5
            border.color: root.invalid ? Theme.expense : input.activeFocus ? Theme.accent : "transparent"
        }
    }
}
