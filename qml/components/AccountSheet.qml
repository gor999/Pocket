import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

// Create or edit an account. SECURITY: the card field accepts at most the last
// four digits; full card numbers, CVV and PINs are never collected or stored.
SheetPopup {
    id: root

    property bool editing: false
    property int accountId: 0
    property int transactionCount: 0
    property string currency: App.settings.defaultCurrency
    property string cardNetwork: ""

    property string errorText: ""
    property bool confirmingDelete: false

    readonly property bool currencyLocked: editing && transactionCount > 0

    title: editing ? qsTr("Edit account") : qsTr("New account")

    function resetFeedback() {
        errorText = ""
        confirmingDelete = false
    }

    function openNew() {
        editing = false
        accountId = 0
        transactionCount = 0
        currency = App.settings.defaultCurrency
        cardNetwork = ""
        bankInput.text = ""
        nameInput.text = ""
        lastFourInput.text = ""
        balanceInput.text = ""
        resetFeedback()
        open()
    }

    function openEdit(a) {
        editing = true
        accountId = a.accountId
        transactionCount = a.transactionCount
        currency = a.currency
        cardNetwork = a.cardNetwork
        bankInput.text = a.bankName
        nameInput.text = a.name
        lastFourInput.text = a.lastFour
        balanceInput.text = a.initialBalance
        resetFeedback()
        open()
    }

    function submit() {
        resetFeedback()
        const result = App.accounts.save({
            id: accountId,
            bankName: bankInput.text,
            name: nameInput.text,
            currency: currency,
            cardNetwork: cardNetwork,
            lastFour: lastFourInput.text,
            initialBalance: balanceInput.text
        })
        if (result.ok)
            close()
        else
            errorText = result.error
    }

    function archive() {
        const result = App.accounts.archiveAccount(accountId, true)
        if (result.ok)
            close()
        else
            errorText = result.error
    }

    function remove() {
        const result = App.accounts.deleteAccount(accountId)
        if (result.ok)
            close()
        else
            errorText = result.error
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8

        FormField {
            id: bankInput
            Layout.fillWidth: true
            label: qsTr("Bank")
            placeholderText: qsTr("e.g. Ameriabank")
            inputMethodHints: Qt.ImhNoPredictiveText
        }
        // Suggestions only: any bank name can be typed above.
        ChipRow {
            Layout.fillWidth: true
            model: App.accounts.suggestedBanks().map(b => ({ value: b, label: b }))
            currentValue: bankInput.text
            onPicked: value => bankInput.text = value
        }
    }

    FormField {
        id: nameInput
        Layout.fillWidth: true
        label: qsTr("Account name")
        placeholderText: qsTr("e.g. Salary card")
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            text: qsTr("Currency")
            font.pixelSize: Theme.fontCaption
            font.weight: Font.Medium
            color: Theme.textSecondary
        }
        ChipRow {
            Layout.fillWidth: true
            enabled: !root.currencyLocked
            model: App.settings.currencies().map(c => ({ value: c, label: c }))
            currentValue: root.currency
            onPicked: value => root.currency = value
        }
        Text {
            Layout.fillWidth: true
            visible: root.currencyLocked
            text: qsTr("The currency cannot be changed because this account already has transactions.")
            font.pixelSize: 12
            color: Theme.textTertiary
            wrapMode: Text.WordWrap
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            text: qsTr("Card network")
            font.pixelSize: Theme.fontCaption
            font.weight: Font.Medium
            color: Theme.textSecondary
        }
        ChipRow {
            Layout.fillWidth: true
            model: App.accounts.cardNetworks()
            currentValue: root.cardNetwork
            onPicked: value => root.cardNetwork = value
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        FormField {
            id: lastFourInput
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            label: qsTr("Last 4 digits (optional)")
            placeholderText: "1234"
            inputMethodHints: Qt.ImhDigitsOnly
            maximumLength: 4
            validator: RegularExpressionValidator { regularExpression: /^[0-9]{0,4}$/ }
        }
        FormField {
            id: balanceInput
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            label: qsTr("Initial balance")
            placeholderText: "0"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Icon {
            name: "lock"
            size: 16
            color: Theme.textTertiary
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Only the last four digits are stored, on this device. Never enter a full card number, CVV or PIN.")
            font.pixelSize: 12
            color: Theme.textTertiary
            wrapMode: Text.WordWrap
        }
    }

    actions: [
        Text {
            Layout.fillWidth: true
            visible: root.errorText.length > 0
            text: root.errorText
            font.pixelSize: 14
            color: Theme.expense
            wrapMode: Text.WordWrap
        },

        PrimaryButton {
            Layout.fillWidth: true
            text: root.editing ? qsTr("Save changes") : qsTr("Add account")
            onClicked: root.submit()
        }
    ]

    PrimaryButton {
        Layout.fillWidth: true
        visible: root.editing
        secondary: true
        text: qsTr("Archive account")
        onClicked: root.archive()
    }

    PrimaryButton {
        Layout.fillWidth: true
        visible: root.editing
        destructive: true
        text: root.confirmingDelete
              ? qsTr("Tap again to delete with %n transaction(s)", "", root.transactionCount)
              : qsTr("Delete account")
        onClicked: {
            if (root.confirmingDelete)
                root.remove()
            else
                root.confirmingDelete = true
        }
    }
}
