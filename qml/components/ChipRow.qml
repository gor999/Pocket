import QtQuick
import PersonalFinance

// Selectable chips. Scrolls horizontally, or wraps onto several lines when
// `wrap` is true.   model: [{ value, label, icon? }]
Item {
    id: root

    property var model: []
    property var currentValue
    property bool wrap: false
    signal picked(var value)

    implicitHeight: wrap ? flow.implicitHeight : 38
    opacity: enabled ? 1 : 0.45

    Component {
        id: chipComponent

        Rectangle {
            id: chip

            required property var modelData
            readonly property bool selected: modelData.value === root.currentValue

            implicitWidth: row.implicitWidth + 28
            width: implicitWidth
            height: 38
            radius: 19
            color: selected ? Theme.accent : Theme.field
            scale: area.pressed ? 0.96 : 1

            Behavior on color {
                ColorAnimation { duration: 120 }
            }
            Behavior on scale {
                NumberAnimation { duration: 80 }
            }

            Row {
                id: row
                anchors.centerIn: parent
                spacing: 6

                Text {
                    visible: !!chip.modelData.icon
                    text: chip.modelData.icon || ""
                    font.pixelSize: 16
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: chip.modelData.label
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: chip.selected ? Theme.textOnAccent : Theme.text
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: area
                anchors.fill: parent
                onClicked: root.picked(chip.modelData.value)
            }
        }
    }

    ListView {
        anchors.fill: parent
        visible: !root.wrap
        orientation: ListView.Horizontal
        spacing: 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.wrap ? [] : root.model
        delegate: chipComponent
    }

    Flow {
        id: flow
        width: root.width
        visible: root.wrap
        spacing: 8

        Repeater {
            model: root.wrap ? root.model : []
            delegate: chipComponent
        }
    }
}
