import QtQuick
import QtQuick.Layouts
import PersonalFinance

// One transaction row. Used by ListView (Activity) and Repeater (Home).
// The required properties are filled from TransactionListModel roles.
Item {
    id: root

    required property int index
    required property string title
    required property string subtitle
    required property string categoryIcon
    required property string categoryColor
    required property string signedAmountText
    required property string timeText
    required property bool isIncome

    property bool showSeparator: true

    signal clicked()

    implicitHeight: 70
    width: parent ? parent.width : 360

    Rectangle {
        anchors.fill: parent
        color: area.pressed ? Theme.pressed : "transparent"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pagePadding
        anchors.rightMargin: Theme.pagePadding
        spacing: 14

        Rectangle {
            Layout.preferredWidth: 46
            Layout.preferredHeight: 46
            radius: 23
            color: Theme.alpha(root.categoryColor, Theme.dark ? 0.22 : 0.15)

            Text {
                anchors.centerIn: parent
                text: root.categoryIcon
                font.pixelSize: 22
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
                color: Theme.text
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: root.subtitle
                font.pixelSize: Theme.fontCaption
                color: Theme.textSecondary
                elide: Text.ElideRight
            }
        }

        ColumnLayout {
            spacing: 2

            Text {
                Layout.alignment: Qt.AlignRight
                text: root.signedAmountText
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
                color: root.isIncome ? Theme.income : Theme.text
            }
            Text {
                Layout.alignment: Qt.AlignRight
                text: root.timeText
                font.pixelSize: 12
                color: Theme.textTertiary
            }
        }
    }

    Rectangle {
        visible: root.showSeparator
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.pagePadding + 60
        height: 1
        color: Theme.border
    }

    MouseArea {
        id: area
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
