import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

Item {
    id: root

    readonly property bool filtering: App.transactionModel.searchText.length > 0
                                      || App.transactionModel.typeFilter !== ""
                                      || App.transactionModel.accountFilter > 0

    property var accountFilterChoices: []
    function refreshAccountChoices() {
        const list = [{ value: 0, label: qsTr("All accounts") }]
        for (let i = 0; i < App.accountModel.count; ++i) {
            const a = App.accountModel.get(i)
            list.push({ value: a.accountId, label: a.name })
        }
        accountFilterChoices = list
    }
    Component.onCompleted: refreshAccountChoices()
    Connections {
        target: App.accountModel
        function onModelReset() { root.refreshAccountChoices() }
    }

    function dayLabel(key) {
        const day = new Date(key + "T00:00:00")
        const today = new Date()
        today.setHours(0, 0, 0, 0)
        const diff = Math.round((today - day) / 86400000)
        if (diff === 0)
            return qsTr("Today")
        if (diff === 1)
            return qsTr("Yesterday")
        return day.toLocaleDateString(Qt.locale(App.settings.localeName), "d MMMM, dddd")
    }

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
                text: qsTr("Activity")
                font.pixelSize: Theme.fontLargeTitle
                font.weight: Font.Bold
                color: Theme.text
            }
            Text {
                visible: App.transactionModel.totalCount > 0
                text: qsTr("%n transaction(s)", "", App.transactionModel.totalCount)
                font.pixelSize: Theme.fontCaption
                color: Theme.textTertiary
            }
        }

        // Search
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.pagePadding
            Layout.rightMargin: Theme.pagePadding
            implicitHeight: 46
            radius: Theme.radiusField
            color: Theme.field

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                spacing: 8

                Icon {
                    name: "search"
                    size: 20
                    color: Theme.textTertiary
                }
                TextField {
                    id: searchInput
                    Layout.fillWidth: true
                    background: null
                    placeholderText: qsTr("Search merchant or note")
                    placeholderTextColor: Theme.textTertiary
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    inputMethodHints: Qt.ImhNoPredictiveText
                    onTextChanged: debounce.restart()
                }
                AbstractButton {
                    visible: searchInput.text.length > 0
                    implicitWidth: 32
                    implicitHeight: 32
                    onClicked: searchInput.clear()
                    background: null
                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: "close"
                            size: 16
                            color: Theme.textTertiary
                        }
                    }
                }
            }

            Timer {
                id: debounce
                interval: 200
                onTriggered: App.transactionModel.searchText = searchInput.text
            }
        }

        SegmentedControl {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.pagePadding
            Layout.rightMargin: Theme.pagePadding
            model: [
                { value: "", label: qsTr("All") },
                { value: "expense", label: qsTr("Expenses") },
                { value: "income", label: qsTr("Income") }
            ]
            currentValue: App.transactionModel.typeFilter
            onPicked: value => App.transactionModel.typeFilter = value
        }

        ChipRow {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.pagePadding
            visible: App.accountModel.count > 1
            model: root.accountFilterChoices
            currentValue: App.transactionModel.accountFilter
            onPicked: value => App.transactionModel.accountFilter = value
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: App.transactionModel.count > 0
            clip: true
            model: App.transactionModel
            boundsBehavior: Flickable.DragAndOvershootBounds
            cacheBuffer: 400

            section.property: "dayKey"
            section.criteria: ViewSection.FullString
            section.delegate: Item {
                required property string section
                width: list.width
                height: 40

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.pagePadding
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 6
                    text: root.dayLabel(parent.section).toUpperCase()
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.6
                    color: Theme.textTertiary
                }
            }

            delegate: TransactionDelegate {
                onClicked: ApplicationWindow.window.editTransaction(App.transactionModel.get(index))
            }

            footer: Item { height: 96 } // clearance for the + button
        }

        // Spacer so the empty state sits in the middle
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: App.transactionModel.count === 0

            EmptyState {
                anchors.centerIn: parent
                width: parent.width
                icon: root.filtering ? "search" : "transactions"
                title: root.filtering ? qsTr("No results") : qsTr("No transactions yet")
                subtitle: root.filtering ? qsTr("Try a different search or clear the filters.")
                                         : qsTr("Record your first expense or income with the + button.")
            }
        }
    }

    AbstractButton {
        id: fab
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        implicitWidth: 60
        implicitHeight: 60
        scale: down ? 0.92 : 1
        onClicked: ApplicationWindow.window.newTransaction()

        Behavior on scale {
            NumberAnimation { duration: 90 }
        }
        background: Rectangle {
            radius: 30
            color: Theme.accent
        }
        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                name: "plus"
                size: 28
                strokeWidth: 2.4
                color: Theme.textOnAccent
            }
        }
    }
}
