import QtQuick
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

// Bottom tab bar. Respects the home-indicator safe area.
Rectangle {
    id: root

    property int currentIndex: 0
    signal tabSelected(int index)

    readonly property var tabs: [
        { icon: "home", label: qsTr("Home") },
        { icon: "transactions", label: qsTr("Activity") },
        { icon: "accounts", label: qsTr("Accounts") },
        { icon: "budgets", label: qsTr("Budgets") },
        { icon: "analytics", label: qsTr("Analytics") }
    ]

    implicitHeight: Theme.tabBarHeight + App.platform.safeAreaBottom
    color: Theme.surface

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.tabBarHeight
        spacing: 0

        Repeater {
            model: root.tabs

            delegate: Item {
                id: tab

                required property var modelData
                required property int index
                readonly property bool selected: index === root.currentIndex

                Layout.fillWidth: true
                Layout.fillHeight: true

                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    scale: area.pressed ? 0.92 : 1

                    Behavior on scale {
                        NumberAnimation { duration: 90 }
                    }

                    Icon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        name: tab.modelData.icon
                        size: 24
                        strokeWidth: tab.selected ? 2.1 : 1.7
                        color: tab.selected ? Theme.accent : Theme.textTertiary
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: tab.modelData.label
                        font.pixelSize: 11
                        font.weight: tab.selected ? Font.DemiBold : Font.Medium
                        color: tab.selected ? Theme.accent : Theme.textTertiary
                    }
                }

                MouseArea {
                    id: area
                    anchors.fill: parent
                    onClicked: root.tabSelected(tab.index)
                }
            }
        }
    }
}
