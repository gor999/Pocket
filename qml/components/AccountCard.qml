import QtQuick
import QtQuick.Layouts
import PersonalFinance

// One account in the Accounts list. Required properties come from AccountListModel.
Card {
    id: root

    required property int index
    required property string bankName
    required property string name
    required property string currency
    required property string cardNetworkLabel
    required property string lastFour
    required property string balanceText
    required property bool isNegative
    required property int transactionCount

    signal clicked()

    // Stable accent color per bank, derived from its name.
    readonly property color tint: {
        let h = 0
        for (let i = 0; i < bankName.length; ++i)
            h = (h * 31 + bankName.charCodeAt(i)) % 360
        return Qt.hsla(h / 360, 0.55, Theme.dark ? 0.62 : 0.5, 1)
    }

    implicitHeight: 92
    scale: area.pressed ? 0.985 : 1

    Behavior on scale {
        NumberAnimation { duration: 90 }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        Rectangle {
            Layout.preferredWidth: 50
            Layout.preferredHeight: 50
            radius: 16
            color: Theme.alpha(root.tint, 0.18)

            Text {
                anchors.centerIn: parent
                text: root.bankName.length > 0 ? root.bankName.charAt(0).toUpperCase() : "?"
                font.pixelSize: 22
                font.weight: Font.Bold
                color: root.tint
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Text {
                Layout.fillWidth: true
                text: root.name
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
                color: Theme.text
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: {
                    const parts = [root.bankName]
                    if (root.cardNetworkLabel.length > 0)
                        parts.push(root.cardNetworkLabel)
                    if (root.lastFour.length > 0)
                        parts.push("••" + root.lastFour)
                    return parts.join("  ·  ")
                }
                font.pixelSize: Theme.fontCaption
                color: Theme.textSecondary
                elide: Text.ElideRight
            }
        }

        ColumnLayout {
            spacing: 3

            Text {
                Layout.alignment: Qt.AlignRight
                text: root.balanceText
                font.pixelSize: 18
                font.weight: Font.Bold
                color: root.isNegative ? Theme.expense : Theme.text
            }
            Text {
                Layout.alignment: Qt.AlignRight
                text: qsTr("%n transaction(s)", "", root.transactionCount)
                font.pixelSize: 12
                color: Theme.textTertiary
            }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
