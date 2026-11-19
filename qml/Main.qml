import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

ApplicationWindow {
    id: window

    // Tabs 0-4 are in the bottom bar; 5 is Settings (opened from Home), 6 is Bank SMS import (from Settings).
    property int currentTab: 0
    property int returnTab: 0

    // Convenience API used by pages and by the developer screenshot tool.
    function newTransaction() { transactionSheet.openNew() }
    function editTransaction(t) { transactionSheet.openEdit(t) }
    function newAccount() { accountSheet.openNew() }
    function editAccount(a) { accountSheet.openEdit(a) }
    function openDatePicker() { transactionSheet.openDatePicker() }
    function closeSheets() {
        transactionSheet.close()
        accountSheet.close()
    }
    function openSettings() {
        returnTab = currentTab
        currentTab = 5
    }
    function closeSettings() { currentTab = returnTab }
    function openSmsImport() { currentTab = 6 }
    function closeSmsImport() { currentTab = 5 }

    visible: true
    width: 390 // iPhone logical size; on a device the window fills the screen
    height: 844
    title: qsTr("Personal Finance")
    color: Theme.background

    onWidthChanged: App.platform.refreshSafeArea()
    onHeightChanged: App.platform.refreshSafeArea()

    StackLayout {
        id: pages
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: bottomBar.visible ? bottomBar.top : parent.bottom
        currentIndex: window.currentTab

        DashboardPage {
            onOpenSettings: window.openSettings()
            onShowAllTransactions: window.currentTab = 1
            onAddAccount: window.newAccount()
        }
        TransactionsPage {}
        AccountsPage {}
        BudgetsPage {}
        AnalyticsPage {}
        SettingsPage {
            onBack: window.closeSettings()
            onOpenSmsImport: window.openSmsImport()
        }
        SmsImportPage {
            onBack: window.closeSmsImport()
        }
    }

    BottomBar {
        id: bottomBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        visible: window.currentTab < 5
        currentIndex: window.currentTab
        onTabSelected: index => window.currentTab = index
    }

    TransactionSheet { id: transactionSheet }
    AccountSheet { id: accountSheet }

    // Shown only if the database could not be opened or migrated.
    Rectangle {
        anchors.fill: parent
        visible: !App.ready
        color: Theme.background

        EmptyState {
            anchors.centerIn: parent
            width: parent.width
            icon: "lock"
            title: qsTr("Could not open your data")
            subtitle: App.startupError
        }
    }
}
