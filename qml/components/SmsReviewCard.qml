import QtQuick
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

// One imported transaction that waits for the user (see SmsImportService::reviewItems()).
Card {
    id: root

    property var item: ({})
    property bool expanded: false
    property int chosenCategory: 0
    property bool remember: true
    property var choices: []

    readonly property bool hasCategory: !!item.categoryName
    readonly property bool hasSuggestion: !!item.suggestedName
    readonly property bool isIncome: item.type === "income"
    readonly property string title: item.merchant ? item.merchant : (item.description ? item.description : item.bankName)

    signal resolved(int categoryId, bool remember)
    signal discarded()

    function reasonText(reason) {
        switch (reason) {
        case "category": return qsTr("Category unsure")
        case "parse": return qsTr("Details unclear - please check")
        case "possible_duplicate": return qsTr("May duplicate another transaction")
        case "untrusted_channel": return qsTr("Came from a link, not the automation")
        case "account": return qsTr("Several accounts fit - check the account")
        }
        return qsTr("Please check")
    }

    function formatWhen(d) {
        if (!d)
            return ""
        const t = new Date(d)
        return isNaN(t.getTime()) ? "" : Qt.formatDateTime(t, "d MMM HH:mm")
    }

    function toggleChooser() {
        if (!expanded) {
            choices = App.categoryModel.choicesFor(item.type)
            chosenCategory = item.suggestedId > 0 ? item.suggestedId : (item.categoryId > 0 ? item.categoryId : 0)
        }
        expanded = !expanded
    }

    implicitHeight: content.implicitHeight + 32

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                radius: 22
                color: root.hasCategory ? Theme.alpha(root.item.categoryColor, Theme.dark ? 0.22 : 0.15) : Theme.field
                Text {
                    anchors.centerIn: parent
                    text: root.hasCategory ? root.item.categoryIcon : "❓"
                    font.pixelSize: 21
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
                    text: [root.item.bankName, root.formatWhen(root.item.occurredAt)].filter(p => !!p).join("  ·  ")
                    font.pixelSize: 12
                    color: Theme.textTertiary
                    wrapMode: Text.WordWrap
                }
            }
            Text {
                text: (root.isIncome ? "+" : "−") + root.item.amountText
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
                color: root.isIncome ? Theme.income : Theme.text
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: Theme.warning
            }
            Text {
                Layout.fillWidth: true
                text: root.reasonText(root.item.reviewReason)
                    + (root.hasSuggestion && !root.hasCategory
                       ? "  ·  " + qsTr("Suggested: %1 %2").arg(root.item.suggestedIcon).arg(root.item.suggestedName)
                       : "")
                font.pixelSize: 13
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }

        // Category chooser
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.expanded
            spacing: 12

            ChipRow {
                Layout.fillWidth: true
                wrap: true
                model: root.choices
                currentValue: root.chosenCategory
                onPicked: value => root.chosenCategory = value
            }
            RowLayout {
                Layout.fillWidth: true
                visible: !!root.item.merchant
                spacing: 10
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Always use this category for %1").arg(root.item.merchant)
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
                ToggleSwitch {
                    checked: root.remember
                    onToggled: value => root.remember = value
                }
            }
            PrimaryButton {
                Layout.fillWidth: true
                implicitHeight: 46
                text: qsTr("Save category")
                enabled: root.chosenCategory > 0
                onClicked: root.resolved(root.chosenCategory, root.remember)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            PrimaryButton {
                Layout.fillWidth: true
                implicitHeight: 44
                text: root.hasCategory ? qsTr("Looks right")
                    : root.hasSuggestion ? qsTr("Use %1").arg(root.item.suggestedName)
                    : qsTr("Keep as is")
                onClicked: root.resolved(root.hasCategory ? 0 : (root.hasSuggestion ? root.item.suggestedId : 0), false)
            }
            PrimaryButton {
                Layout.fillWidth: true
                implicitHeight: 44
                secondary: true
                text: root.expanded ? qsTr("Cancel") : qsTr("Change")
                onClicked: root.toggleChooser()
            }
            PrimaryButton {
                implicitHeight: 44
                implicitWidth: 56
                destructive: true
                text: "×"
                onClicked: root.discarded()
            }
        }
    }
}
