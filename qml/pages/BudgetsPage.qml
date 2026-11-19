import QtQuick
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

// Phase 3: monthly and per-category budgets with 80/90/100% alerts.
Item {
    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Item { Layout.preferredHeight: App.platform.safeAreaTop - 4 }

        Text {
            Layout.leftMargin: Theme.pagePadding
            text: qsTr("Budgets")
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
                icon: "budgets"
                title: qsTr("Budgets are coming soon")
                subtitle: qsTr("Monthly and per-category limits with alerts at 80%, 90% and 100% will appear here.")
            }
        }
    }
}
