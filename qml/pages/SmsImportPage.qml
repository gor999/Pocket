import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

// Bank SMS import: automation status, connected banks, setup steps, review list, history and errors.
// Everything here reads from App.smsImport, which works on the device's own database only.
Item {
    id: root

    signal back()

    // Re-evaluated whenever any data changes (App.revision) - same pattern as the other pages.
    readonly property var summary: { App.revision; return App.smsImport.summary() }
    readonly property var banks: { App.revision; return App.smsImport.banks() }
    readonly property var reviewItems: { App.revision; return App.smsImport.reviewItems() }
    readonly property var history: { App.revision; return App.smsImport.history(20) }
    readonly property var errors: { App.revision; return App.smsImport.errors(20) }

    property bool setupExpanded: summary.level === "notSetUp"

    function formatWhen(d) {
        if (!d)
            return ""
        const t = new Date(d)
        return isNaN(t.getTime()) ? "" : Qt.formatDateTime(t, "d MMM HH:mm")
    }
    // "Bank  ·  17 Nov 14:32", leaving out whatever is empty.
    function joinParts(parts) {
        return parts.filter(p => !!p).join("  \u00B7  ")
    }

    readonly property color levelColor: {
        switch (summary.level) {
        case "active": return Theme.income
        case "linkVerified": return Theme.accent
        default: return Theme.warning
        }
    }
    readonly property string levelTitle: {
        switch (summary.level) {
        case "active": return qsTr("Active")
        case "linkVerified": return qsTr("Connection works")
        case "stale": return qsTr("No bank SMS for a while")
        default: return qsTr("Not set up yet")
        }
    }
    readonly property string levelText: {
        switch (summary.level) {
        case "active": return qsTr("Bank SMS are imported automatically. New ones are added when you open the app.")
        case "linkVerified": return qsTr("The test message arrived. Waiting for the first bank SMS.")
        case "stale": return qsTr("Check in the Shortcuts app that your automations are still switched on.")
        default: return qsTr("Set up the Shortcuts automations once (steps below). After that, nothing needs to be copied or pasted.")
        }
    }

    component SectionTitle: Text {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.pagePadding + 4
        Layout.topMargin: 22
        font.pixelSize: 12
        font.weight: Font.DemiBold
        font.letterSpacing: 0.6
        color: Theme.textTertiary
    }

    component Caption: Text {
        Layout.fillWidth: true
        font.pixelSize: 12
        color: Theme.textTertiary
        wrapMode: Text.WordWrap
    }

    component InfoRow: RowLayout {
        property string label
        property string value
        Layout.fillWidth: true
        spacing: 10
        Text {
            Layout.fillWidth: true
            text: parent.label
            font.pixelSize: 14
            color: Theme.textSecondary
        }
        Text {
            text: parent.value
            font.pixelSize: 14
            font.weight: Font.Medium
            color: Theme.text
        }
    }

    component Step: RowLayout {
        property int number
        property string text
        Layout.fillWidth: true
        spacing: 12
        Rectangle {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 26
            Layout.preferredHeight: 26
            radius: 13
            color: Theme.accentSoft
            Text {
                anchors.centerIn: parent
                text: parent.parent.number
                font.pixelSize: 13
                font.weight: Font.Bold
                color: Theme.accent
            }
        }
        Text {
            Layout.fillWidth: true
            text: parent.text
            font.pixelSize: 14
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 40
        boundsBehavior: Flickable.DragAndOvershootBounds

        ColumnLayout {
            id: column
            width: flick.width
            spacing: 8

            Item { Layout.preferredHeight: App.platform.safeAreaTop }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: Theme.pagePadding
                spacing: 2

                AbstractButton {
                    implicitWidth: 44
                    implicitHeight: 44
                    onClicked: root.back()
                    background: null
                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: "chevron-left"
                            size: 26
                            strokeWidth: 2.2
                            color: Theme.accent
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Bank SMS import")
                    font.pixelSize: Theme.fontLargeTitle - 4
                    font.weight: Font.Bold
                    color: Theme.text
                    elide: Text.ElideRight
                }
            }

            // ---- Status -------------------------------------------------------------------------
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                Layout.topMargin: 6
                implicitHeight: statusColumn.implicitHeight + 32

                ColumnLayout {
                    id: statusColumn
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 12
                            radius: 6
                            color: root.levelColor
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.levelTitle
                            font.pixelSize: Theme.fontTitle - 3
                            font.weight: Font.DemiBold
                            color: Theme.text
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.levelText
                        font.pixelSize: 14
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                    InfoRow {
                        label: qsTr("Last bank SMS")
                        value: root.formatWhen(root.summary.lastMessageAt) || qsTr("none yet")
                    }
                    InfoRow {
                        label: qsTr("Last imported")
                        value: root.formatWhen(root.summary.lastImportAt) || qsTr("none yet")
                    }
                    InfoRow {
                        visible: !!root.formatWhen(root.summary.lastTestAt)
                        label: qsTr("Last test message")
                        value: root.formatWhen(root.summary.lastTestAt)
                    }
                    InfoRow {
                        label: qsTr("Imported / need review / errors")
                        value: root.summary.importedCount + " / " + root.summary.needsReviewCount + " / " + root.summary.errorCount
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.summary.pendingFiles > 0
                        spacing: 10
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("%n message(s) waiting to be added", "", root.summary.pendingFiles)
                            font.pixelSize: 14
                            color: Theme.warning
                        }
                        PrimaryButton {
                            implicitHeight: 38
                            implicitWidth: 110
                            text: qsTr("Add now")
                            onClicked: App.smsImport.ingestPending()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !root.summary.receiverAvailable
                        text: root.summary.receiverDetail
                        font.pixelSize: 13
                        color: Theme.expense
                        wrapMode: Text.WordWrap
                    }
                    Caption {
                        text: qsTr("iOS cannot start this app when an SMS arrives. The Shortcuts automation hands each message over right away; it is added to your transactions the next time the app is opened.")
                    }
                }
            }

            // ---- Needs review -------------------------------------------------------------------
            SectionTitle {
                visible: root.reviewItems.length > 0
                text: qsTr("NEEDS YOUR REVIEW (%1)").arg(root.reviewItems.length)
            }
            Repeater {
                model: root.reviewItems
                delegate: SmsReviewCard {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.pagePadding
                    Layout.rightMargin: Theme.pagePadding
                    item: modelData
                    onResolved: (categoryId, remember) => App.smsImport.resolveReview(modelData.id, categoryId, remember)
                    onDiscarded: App.smsImport.discardReview(modelData.id)
                }
            }

            // ---- Banks --------------------------------------------------------------------------
            SectionTitle { text: qsTr("BANKS") }
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                implicitHeight: banksColumn.implicitHeight + 8

                ColumnLayout {
                    id: banksColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    anchors.topMargin: 8
                    spacing: 0

                    Repeater {
                        model: root.banks
                        delegate: ColumnLayout {
                            id: bankRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: Theme.border
                                visible: bankRow.index > 0
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 12
                                Layout.bottomMargin: 12
                                spacing: 12
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text {
                                        Layout.fillWidth: true
                                        text: bankRow.modelData.name
                                        font.pixelSize: Theme.fontBody
                                        font.weight: Font.DemiBold
                                        color: Theme.text
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: bankRow.modelData.seen
                                            ? qsTr("%1 message(s) · last %2").arg(bankRow.modelData.receivedCount)
                                                  .arg(root.formatWhen(bankRow.modelData.lastReceivedAt))
                                            : qsTr("No message seen yet")
                                        font.pixelSize: 12
                                        color: Theme.textTertiary
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("Automation filter: message contains “%1”").arg(bankRow.modelData.filterPhrases.join("” or “"))
                                        font.pixelSize: 12
                                        color: Theme.textSecondary
                                        wrapMode: Text.WordWrap
                                    }
                                }
                                ToggleSwitch {
                                    checked: bankRow.modelData.enabled
                                    onToggled: value => App.smsImport.setBankEnabled(bankRow.modelData.id, value)
                                }
                            }
                        }
                    }
                }
            }
            Caption {
                Layout.leftMargin: Theme.pagePadding + 4
                Layout.rightMargin: Theme.pagePadding
                text: qsTr("Other banks: create an automation for them too and choose “Other / detect”. Messages the app cannot read are listed under Errors below instead of being guessed.")
            }

            // ---- Setup --------------------------------------------------------------------------
            SectionTitle { text: qsTr("SET UP (ONCE)") }
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                implicitHeight: setupColumn.implicitHeight + 32

                ColumnLayout {
                    id: setupColumn
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    MouseArea {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        onClicked: root.setupExpanded = !root.setupExpanded
                        RowLayout {
                            anchors.fill: parent
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Shortcuts automation for each bank")
                                font.pixelSize: Theme.fontBody
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            Icon {
                                name: "chevron-right"
                                size: 18
                                color: Theme.textTertiary
                                rotation: root.setupExpanded ? 90 : 0
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.setupExpanded
                        spacing: 12

                        Step { number: 1; text: qsTr("Open the Shortcuts app → Automation → + → Message.") }
                        Step { number: 2; text: qsTr("Sender: leave it empty. Message Contains: the bank word shown above (for example “IDBank”). Choose “Run Immediately”, then Next.") }
                        Step { number: 3; text: qsTr("Add the action “Import Bank SMS” (from Personal Finance). Set Message to the Shortcut Input (Content), and pick the bank.") }
                        Step { number: 4; text: qsTr("Repeat for every bank. To test, send yourself an SMS that starts with PF-TEST: this screen shows “Connection works” after you open the app.") }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            PrimaryButton {
                                Layout.fillWidth: true
                                implicitHeight: 44
                                text: qsTr("Open Shortcuts")
                                onClicked: Qt.openUrlExternally("shortcuts://")
                            }
                        }
                        Caption {
                            text: qsTr("iOS decides whether a banner is shown for an automation, and some banks send from sender names Shortcuts cannot select - that is why the filter is the text in the message. Details: docs/SHORTCUTS_SETUP.md")
                        }
                    }
                }
            }

            // ---- Options ------------------------------------------------------------------------
            SectionTitle { text: qsTr("OPTIONS") }
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                implicitHeight: optionsColumn.implicitHeight + 32

                ColumnLayout {
                    id: optionsColumn
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Create accounts automatically")
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                color: Theme.text
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("If no account matches the bank and card, add one.")
                                font.pixelSize: 12
                                color: Theme.textTertiary
                                wrapMode: Text.WordWrap
                            }
                        }
                        ToggleSwitch {
                            checked: root.summary.autoCreateAccounts
                            onToggled: value => App.smsImport.setAutoCreateAccounts(value)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Trust links from other apps")
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                color: Theme.text
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Any app can open a personalfinance:// link. Leave this off to review such messages first.")
                                font.pixelSize: 12
                                color: Theme.textTertiary
                                wrapMode: Text.WordWrap
                            }
                        }
                        ToggleSwitch {
                            checked: root.summary.trustUrlChannel
                            onToggled: value => App.smsImport.setTrustUrlChannel(value)
                        }
                    }
                    Caption {
                        text: qsTr("Receiver: %1").arg(root.summary.receiverName)
                    }
                }
            }

            // ---- History ------------------------------------------------------------------------
            SectionTitle { text: qsTr("RECENTLY IMPORTED") }
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                implicitHeight: root.history.length === 0 ? 76 : historyColumn.implicitHeight + 8

                Text {
                    anchors.centerIn: parent
                    visible: root.history.length === 0
                    text: qsTr("Nothing imported yet")
                    font.pixelSize: 14
                    color: Theme.textTertiary
                }

                ColumnLayout {
                    id: historyColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    anchors.topMargin: 8
                    spacing: 0

                    Repeater {
                        model: root.history
                        delegate: ColumnLayout {
                            id: historyRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: Theme.border
                                visible: historyRow.index > 0
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 10
                                Layout.bottomMargin: 10
                                spacing: 12

                                Rectangle {
                                    Layout.preferredWidth: 38
                                    Layout.preferredHeight: 38
                                    radius: 19
                                    color: historyRow.modelData.categoryName
                                        ? Theme.alpha(historyRow.modelData.categoryColor, Theme.dark ? 0.22 : 0.15) : Theme.field
                                    Text {
                                        anchors.centerIn: parent
                                        text: historyRow.modelData.categoryName ? historyRow.modelData.categoryIcon : "•"
                                        font.pixelSize: 18
                                        color: Theme.textTertiary
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text {
                                        Layout.fillWidth: true
                                        text: historyRow.modelData.merchant || historyRow.modelData.description || historyRow.modelData.bankName
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                        color: Theme.text
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.joinParts([historyRow.modelData.bankName,
                                                              root.formatWhen(historyRow.modelData.receivedAt),
                                                              historyRow.modelData.status === "duplicate" ? qsTr("duplicate, not added")
                                                              : historyRow.modelData.status === "needs_review" ? qsTr("needs review") : ""])
                                        font.pixelSize: 12
                                        color: historyRow.modelData.status === "duplicate" ? Theme.textTertiary : Theme.textSecondary
                                        wrapMode: Text.WordWrap
                                    }
                                }
                                Text {
                                    visible: !!historyRow.modelData.amountText
                                    text: (historyRow.modelData.type === "income" ? "+" : "−") + historyRow.modelData.amountText
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    color: historyRow.modelData.type === "income" ? Theme.income : Theme.text
                                    opacity: historyRow.modelData.status === "duplicate" ? 0.5 : 1
                                }
                            }
                        }
                    }
                }
            }

            // ---- Errors -------------------------------------------------------------------------
            SectionTitle {
                visible: root.errors.length > 0
                text: qsTr("COULD NOT IMPORT (%1)").arg(root.errors.length)
            }
            Repeater {
                model: root.errors
                delegate: Card {
                    id: errorCard
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.pagePadding
                    Layout.rightMargin: Theme.pagePadding
                    implicitHeight: errorColumn.implicitHeight + 28

                    ColumnLayout {
                        id: errorColumn
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                Layout.fillWidth: true
                                text: errorCard.modelData.errorMessage || qsTr("Could not be imported")
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: Theme.expense
                                wrapMode: Text.WordWrap
                            }
                            AbstractButton {
                                implicitWidth: 36
                                implicitHeight: 36
                                onClicked: App.smsImport.dismissError(errorCard.modelData.id)
                                background: null
                                contentItem: Item {
                                    Icon { anchors.centerIn: parent; name: "close"; size: 16; color: Theme.textTertiary }
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: !!errorCard.modelData.snippet
                            text: errorCard.modelData.snippet
                            font.pixelSize: 12
                            color: Theme.textSecondary
                            wrapMode: Text.WrapAnywhere
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }
                        Caption {
                            text: root.joinParts([errorCard.modelData.bankName, root.formatWhen(errorCard.modelData.receivedAt)])
                        }
                    }
                }
            }
            PrimaryButton {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 14
                implicitHeight: 42
                secondary: true
                visible: root.errors.length > 0 || root.history.length > 0
                text: qsTr("Clear log")
                onClicked: App.smsImport.clearHistory()
            }
            Caption {
                Layout.leftMargin: Theme.pagePadding + 4
                Layout.rightMargin: Theme.pagePadding
                Layout.topMargin: 6
                text: qsTr("Messages are processed on this iPhone only. One-time codes are discarded unread, and full card numbers are masked before anything is saved.")
            }
        }
    }
}
