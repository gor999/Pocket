import QtQuick
import QtQuick.Layouts
import PersonalFinance

ColumnLayout {
    id: root

    property string icon: ""
    property string title: ""
    property string subtitle: ""
    property string actionText: ""
    signal actionTriggered()

    spacing: 10

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 72
        Layout.preferredHeight: 72
        radius: 36
        color: Theme.accentSoft

        Icon {
            anchors.centerIn: parent
            name: root.icon
            size: 32
            color: Theme.accent
        }
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 6
        text: root.title
        font.pixelSize: 20
        font.weight: Font.DemiBold
        color: Theme.text
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        Layout.leftMargin: 32
        Layout.rightMargin: 32
        visible: text.length > 0
        text: root.subtitle
        font.pixelSize: 15
        color: Theme.textSecondary
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }

    PrimaryButton {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 10
        visible: root.actionText.length > 0
        text: root.actionText
        onClicked: root.actionTriggered()
    }
}
