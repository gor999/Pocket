import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

Item {
    id: root

    signal openSettings()
    signal showAllTransactions()
    signal addAccount()


    // Balances per currency. Currencies are never added together: that would
    // need exchange rates, and the app must not guess.
    readonly property var totals: {
        App.revision
        return App.accounts.totalsByCurrency()
    }
    readonly property var heroTotal: {
        if (totals.length === 0)
            return null
        for (let i = 0; i < totals.length; ++i) {
            if (totals[i].currency === App.settings.defaultCurrency)
                return totals[i]
        }
        return totals[0]
    }
    readonly property var otherTotals: totals.filter(t => !heroTotal || t.currency !== heroTotal.currency)
    readonly property var summary: {
        App.revision
        return heroTotal ? App.analytics.monthSummary(heroTotal.currency) : null
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 32
        boundsBehavior: Flickable.DragAndOvershootBounds

        ColumnLayout {
            id: column
            width: flick.width
            spacing: 0

            Item { Layout.preferredHeight: App.platform.safeAreaTop + 8 }

            // Header
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding

                ColumnLayout {
                    spacing: 0
                    Text {
                        text: new Date().toLocaleDateString(Qt.locale(App.settings.localeName), "MMMM yyyy").toUpperCase()
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.8
                        color: Theme.textTertiary
                    }
                    Text {
                        text: qsTr("Overview")
                        font.pixelSize: Theme.fontLargeTitle
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                }
                Item { Layout.fillWidth: true }
                AbstractButton {
                    implicitWidth: 44
                    implicitHeight: 44
                    onClicked: root.openSettings()
                    background: Rectangle {
                        radius: 22
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.border
                    }
                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: "settings"
                            size: 22
                            color: Theme.text
                        }
                    }
                }
            }

            // No accounts yet
            EmptyState {
                Layout.fillWidth: true
                Layout.topMargin: 90
                visible: App.accountModel.count === 0
                icon: "accounts"
                title: qsTr("Welcome")
                subtitle: qsTr("Add your first bank account or cash wallet to start tracking your money. Everything stays on this device.")
                actionText: qsTr("Add account")
                onActionTriggered: root.addAccount()
            }

            // Hero balance card
            Rectangle {
                id: hero
                visible: root.heroTotal !== null
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                Layout.topMargin: 18
                implicitHeight: heroContent.implicitHeight + 52
                radius: 28
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.heroTop }
                    GradientStop { position: 1.0; color: Theme.heroBottom }
                }

                Rectangle {
                    width: 150
                    height: 150
                    radius: 75
                    x: hero.width - 175
                    y: 16
                    color: "transparent"
                    border.width: 26
                    border.color: "#FFFFFF"
                    opacity: 0.06
                }

                ColumnLayout {
                    id: heroContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 26
                    spacing: 6

                    Text {
                        text: qsTr("Total balance")
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: "#FFFFFF"
                        opacity: 0.78
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.heroTotal ? root.heroTotal.text : ""
                        font.pixelSize: 40
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                        fontSizeMode: Text.HorizontalFit
                        minimumPixelSize: 24
                    }
                    Rectangle {
                        visible: root.summary !== null
                        Layout.topMargin: 6
                        implicitWidth: netRow.implicitWidth + 24
                        implicitHeight: 32
                        radius: 16
                        color: "#2EFFFFFF"

                        RowLayout {
                            id: netRow
                            anchors.centerIn: parent
                            spacing: 6
                            Icon {
                                name: root.summary && root.summary.netMinor < 0 ? "arrow-down-left" : "arrow-up-right"
                                size: 14
                                strokeWidth: 2.2
                                color: "#FFFFFF"
                            }
                            Text {
                                text: root.summary ? qsTr("%1 this month").arg(root.summary.net) : ""
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: "#FFFFFF"
                            }
                        }
                    }
                    Flow {
                        visible: root.otherTotals.length > 0
                        Layout.fillWidth: true
                        Layout.topMargin: 10
                        spacing: 8

                        Repeater {
                            model: root.otherTotals
                            delegate: Rectangle {
                                required property var modelData
                                implicitWidth: otherText.implicitWidth + 22
                                implicitHeight: 30
                                radius: 15
                                color: "#26FFFFFF"

                                Text {
                                    id: otherText
                                    anchors.centerIn: parent
                                    text: modelData.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    color: "#FFFFFF"
                                }
                            }
                        }
                    }
                }
            }

            // Income / expenses this month
            RowLayout {
                visible: root.summary !== null
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                Layout.topMargin: 14
                spacing: 12

                component StatCard: Card {
                    id: stat
                    property string label
                    property string value
                    property string icon
                    property color tone

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 92

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 6

                        RowLayout {
                            spacing: 8
                            Rectangle {
                                Layout.preferredWidth: 26
                                Layout.preferredHeight: 26
                                radius: 13
                                color: Theme.alpha(stat.tone, 0.16)
                                Icon {
                                    anchors.centerIn: parent
                                    name: stat.icon
                                    size: 14
                                    strokeWidth: 2.2
                                    color: stat.tone
                                }
                            }
                            Text {
                                text: stat.label
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: Theme.textSecondary
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: stat.value
                            font.pixelSize: 19
                            font.weight: Font.Bold
                            color: Theme.text
                            fontSizeMode: Text.HorizontalFit
                            minimumPixelSize: 12
                        }
                    }
                }

                StatCard {
                    label: qsTr("Income")
                    value: root.summary ? root.summary.income : ""
                    icon: "arrow-down-left"
                    tone: Theme.income
                }
                StatCard {
                    label: qsTr("Expenses")
                    value: root.summary ? root.summary.expense : ""
                    icon: "arrow-up-right"
                    tone: Theme.expense
                }
            }

            // Recent transactions
            RowLayout {
                visible: App.accountModel.count > 0
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                Layout.topMargin: 26
                Layout.bottomMargin: 10

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Recent transactions")
                    font.pixelSize: 19
                    font.weight: Font.Bold
                    color: Theme.text
                }
                AbstractButton {
                    visible: App.recentModel.count > 0
                    text: qsTr("See all")
                    onClicked: root.showAllTransactions()
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: Theme.accent
                    }
                    background: null
                }
            }

            Card {
                visible: App.recentModel.count > 0
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                implicitHeight: recentColumn.implicitHeight

                Column {
                    id: recentColumn
                    width: parent.width

                    Repeater {
                        model: App.recentModel
                        delegate: TransactionDelegate {
                            showSeparator: index < App.recentModel.count - 1
                            onClicked: ApplicationWindow.window.editTransaction(App.recentModel.get(index))
                        }
                    }
                }
            }

            Text {
                visible: App.accountModel.count > 0 && App.recentModel.count === 0
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                text: qsTr("No transactions yet. Tap + on the Activity tab to add one.")
                font.pixelSize: 15
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }
    }
}
