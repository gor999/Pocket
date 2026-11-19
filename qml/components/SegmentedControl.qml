import QtQuick
import PersonalFinance

// iOS-style segmented control.  model: [{ value, label }]
Item {
    id: root

    property var model: []
    property var currentValue
    signal picked(var value)

    readonly property int currentIndex: {
        for (let i = 0; i < model.length; ++i) {
            if (model[i].value === currentValue)
                return i
        }
        return -1
    }

    implicitHeight: 40

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: Theme.field
    }

    Rectangle {
        visible: root.currentIndex >= 0
        width: (root.width - 4) / Math.max(1, root.model.length)
        height: root.height - 4
        y: 2
        x: 2 + Math.max(0, root.currentIndex) * width
        radius: 10
        color: Theme.segmentThumb
        border.width: Theme.dark ? 0 : 1
        border.color: Theme.border

        Behavior on x {
            NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
        }
    }

    Row {
        anchors.fill: parent
        anchors.margins: 2

        Repeater {
            model: root.model

            delegate: Item {
                required property var modelData
                required property int index

                width: (root.width - 4) / Math.max(1, root.model.length)
                height: root.height - 4

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 8
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: modelData.label
                    font.pixelSize: 14
                    font.weight: index === root.currentIndex ? Font.DemiBold : Font.Medium
                    color: index === root.currentIndex ? Theme.text : Theme.textSecondary
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.picked(modelData.value)
                }
            }
        }
    }
}
