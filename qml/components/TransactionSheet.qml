import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

// Create or edit a transaction. All validation and saving happens in C++
// (App.transactions.save); this file only collects input.
SheetPopup {
    id: root

    property bool editing: false
    property int transactionId: 0
    property string type: "expense"
    property int accountId: 0
    property int categoryId: 0
    property date when: new Date()

    property var accountChoices: []
    property var categoryChoices: []
    property string errorText: ""
    property bool duplicateWarning: false
    property bool confirmingDelete: false

    readonly property string currencyCode: {
        const row = App.accountModel.indexOfId(accountId)
        return row >= 0 ? App.accountModel.get(row).currency : ""
    }

    title: editing ? qsTr("Edit transaction") : qsTr("New transaction")

    function openDatePicker() { picker.openWith(root.when) }

    function refreshChoices() {
        const accounts = []
        for (let i = 0; i < App.accountModel.count; ++i) {
            const a = App.accountModel.get(i)
            accounts.push({ value: a.accountId, label: a.title })
        }
        accountChoices = accounts
        categoryChoices = App.categoryModel.choicesFor(type)
    }

    function resetFeedback() {
        errorText = ""
        duplicateWarning = false
        confirmingDelete = false
    }

    function openNew() {
        editing = false
        transactionId = 0
        type = "expense"
        categoryId = 0
        when = new Date()
        amountField.text = ""
        merchantInput.text = ""
        noteInput.text = ""
        refreshChoices()
        accountId = accountChoices.length > 0 ? accountChoices[0].value : 0
        resetFeedback()
        open()
    }

    function openEdit(t) {
        editing = true
        transactionId = t.id
        type = t.type
        accountId = t.accountId
        categoryId = t.categoryId
        when = t.occurredAt
        amountField.text = t.amount
        merchantInput.text = t.merchant
        noteInput.text = t.description
        refreshChoices()
        resetFeedback()
        open()
    }

    function submit(force) {
        resetFeedback()
        const result = App.transactions.save({
            id: transactionId,
            accountId: accountId,
            categoryId: categoryId,
            type: type,
            amount: amountField.text,
            occurredAt: when,
            merchant: merchantInput.text,
            description: noteInput.text,
            force: force === true
        })
        if (result.ok) {
            close()
        } else if (result.duplicate) {
            duplicateWarning = true
        } else {
            errorText = result.error
        }
    }

    function remove() {
        const result = App.transactions.deleteTransaction(transactionId)
        if (result.ok)
            close()
        else
            errorText = result.error
    }

    onTypeChanged: {
        categoryChoices = App.categoryModel.choicesFor(type)
        // Drop a category that does not apply to the new type.
        if (!categoryChoices.some(c => c.value === categoryId))
            categoryId = 0
    }

    SegmentedControl {
        Layout.fillWidth: true
        model: [
            { value: "expense", label: qsTr("Expense") },
            { value: "income", label: qsTr("Income") }
        ]
        currentValue: root.type
        onPicked: value => root.type = value
    }

    // Amount
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            TextField {
                id: amountField
                implicitWidth: Math.max(80, contentWidth + 24)
                placeholderText: "0"
                placeholderTextColor: Theme.textTertiary
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                font.pixelSize: 44
                font.weight: Font.Bold
                color: root.type === "income" ? Theme.income : Theme.text
                horizontalAlignment: TextInput.AlignHCenter
                background: null
                onAccepted: merchantInput.field.forceActiveFocus()
            }
            Text {
                text: root.currencyCode
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: Theme.textSecondary
            }
        }
    }

    // Account
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            text: qsTr("Account")
            font.pixelSize: Theme.fontCaption
            font.weight: Font.Medium
            color: Theme.textSecondary
        }
        ChipRow {
            Layout.fillWidth: true
            visible: root.accountChoices.length > 0
            model: root.accountChoices
            currentValue: root.accountId
            onPicked: value => root.accountId = value
        }
        Text {
            visible: root.accountChoices.length === 0
            Layout.fillWidth: true
            text: qsTr("Add an account first, then you can record transactions.")
            font.pixelSize: 14
            color: Theme.expense
            wrapMode: Text.WordWrap
        }
    }

    // Category
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            text: qsTr("Category")
            font.pixelSize: Theme.fontCaption
            font.weight: Font.Medium
            color: Theme.textSecondary
        }
        ChipRow {
            Layout.fillWidth: true
            wrap: true
            model: root.categoryChoices
            currentValue: root.categoryId
            onPicked: value => root.categoryId = (value === root.categoryId ? 0 : value)
        }
    }

    FormField {
        id: merchantInput
        Layout.fillWidth: true
        label: qsTr("Merchant")
        placeholderText: qsTr("Where did it happen?")
        onAccepted: noteInput.field.forceActiveFocus()
    }

    FormField {
        id: noteInput
        Layout.fillWidth: true
        label: qsTr("Note")
        placeholderText: qsTr("Optional")
    }

    // Date and time
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6

        Text {
            text: qsTr("Date and time")
            font.pixelSize: Theme.fontCaption
            font.weight: Font.Medium
            color: Theme.textSecondary
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 50
            radius: Theme.radiusField
            color: Theme.field

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Icon {
                    name: "calendar"
                    size: 20
                    color: Theme.textSecondary
                }
                Text {
                    Layout.fillWidth: true
                    text: root.when.toLocaleDateString(Qt.locale(App.settings.localeName), "ddd, d MMM yyyy") + "  ·  "
                          + root.when.toLocaleTimeString(Qt.locale(App.settings.localeName), "HH:mm")
                    font.pixelSize: Theme.fontBody
                    color: Theme.text
                }
                Icon {
                    name: "chevron-right"
                    size: 18
                    color: Theme.textTertiary
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.openDatePicker()
            }
        }
    }

    actions: [
        // Feedback
        Rectangle {
            Layout.fillWidth: true
            visible: root.duplicateWarning
            radius: Theme.radiusField
            color: Theme.alpha(Theme.warning, 0.16)
            implicitHeight: dupColumn.implicitHeight + 24

            ColumnLayout {
                id: dupColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Possible duplicate: a transaction with the same amount on this account was recorded within 10 minutes.")
                    font.pixelSize: 14
                    color: Theme.text
                    wrapMode: Text.WordWrap
                }
                PrimaryButton {
                    Layout.fillWidth: true
                    implicitHeight: 44
                    secondary: true
                    text: qsTr("Save anyway")
                    onClicked: root.submit(true)
                }
            }
        },

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
            enabled: root.accountChoices.length > 0
            text: root.editing ? qsTr("Save changes") : qsTr("Add transaction")
            onClicked: root.submit(false)
        }
    ]

    PrimaryButton {
        Layout.fillWidth: true
        visible: root.editing
        destructive: true
        text: root.confirmingDelete ? qsTr("Tap again to delete") : qsTr("Delete transaction")
        onClicked: {
            if (root.confirmingDelete)
                root.remove()
            else
                root.confirmingDelete = true
        }
    }

    onClosed: picker.close()

    DateTimePicker {
        id: picker
        onPicked: value => root.when = value
    }

}
