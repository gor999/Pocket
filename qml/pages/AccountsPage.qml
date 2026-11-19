import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Item { Layout.preferredHeight: App.platform.safeAreaTop - 4 }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.pagePadding
            Layout.rightMargin: Theme.pagePadding

            Text {
                Layout.fillWidth: true
                text: qsTr("Accounts")
                font.pixelSize: Theme.fontLargeTitle
                font.weight: Font.Bold
                color: Theme.text
            }
            AbstractButton {
                implicitWidth: 44
                implicitHeight: 44
                scale: down ? 0.92 : 1
                onClicked: ApplicationWindow.window.newAccount()
                background: Rectangle {
                    radius: 22
                    color: Theme.accent
                }
                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        name: "plus"
                        size: 22
                        strokeWidth: 2.4
                        color: Theme.textOnAccent
                    }
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: App.accountModel.count > 0
            clip: true
            spacing: 12
            topMargin: 6
            bottomMargin: 24
            model: App.accountModel
            boundsBehavior: Flickable.DragAndOvershootBounds

            delegate: AccountCard {
                x: Theme.pagePadding
                width: list.width - 2 * Theme.pagePadding
                onClicked: ApplicationWindow.window.editAccount(App.accountModel.get(index))
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: App.accountModel.count === 0

            EmptyState {
                anchors.centerIn: parent
                width: parent.width
                icon: "accounts"
                title: qsTr("No accounts yet")
                subtitle: qsTr("Add cards and accounts from any Armenian or international bank, or a cash wallet.")
                actionText: qsTr("Add account")
                onActionTriggered: ApplicationWindow.window.newAccount()
            }
        }
    }
}
