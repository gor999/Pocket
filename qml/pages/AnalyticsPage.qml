import QtQuick
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

// Phase 3: income vs expenses, daily spending, category distribution,
// month-over-month comparison and top categories.
Item {
    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Item { Layout.preferredHeight: App.platform.safeAreaTop - 4 }

        Text {
            Layout.leftMargin: Theme.pagePadding
            text: qsTr("Analytics")
            font.pixelSize: Theme.fontLargeTitle
            font.weight: Font.Bold
            color: Theme.text
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            EmptyState {
                anchors.centerIn: parent
                width: parent.width
                icon: "analytics"
                title: qsTr("Analytics are coming soon")
                subtitle: qsTr("Spending charts, category breakdowns and month-to-month comparisons will appear here.")
            }
        }
    }
}
