import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

Item {
    id: root

    signal back()
    signal openSmsImport()

    readonly property var smsSummary: { App.revision; return App.smsImport.summary() }

    component SectionTitle: Text {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.pagePadding + 4
        Layout.topMargin: 22
        font.pixelSize: 12
        font.weight: Font.DemiBold
        font.letterSpacing: 0.6
        color: Theme.textTertiary
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
                    text: qsTr("Settings")
                    font.pixelSize: Theme.fontLargeTitle
                    font.weight: Font.Bold
                    color: Theme.text
                }
            }

            SectionTitle { text: qsTr("AUTOMATION") }
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                implicitHeight: smsRow.implicitHeight + 32
                color: smsArea.pressed ? Qt.tint(Theme.card, Theme.pressed) : Theme.card

                RowLayout {
                    id: smsRow
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 20
                        color: Theme.accentSoft
                        Icon {
                            anchors.centerIn: parent
                            name: "transactions"
                            size: 20
                            color: Theme.accent
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Bank SMS import")
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                            color: Theme.text
                        }
                        Text {
                            Layout.fillWidth: true
                            text: {
                                if (root.smsSummary.needsReviewCount > 0)
                                    return qsTr("%n transaction(s) need your review", "", root.smsSummary.needsReviewCount)
                                switch (root.smsSummary.level) {
                                case "active": return qsTr("Active")
                                case "linkVerified": return qsTr("Connection works")
                                case "stale": return qsTr("No bank SMS for a while")
                                }
                                return qsTr("Not set up yet")
                            }
                            font.pixelSize: 13
                            color: root.smsSummary.needsReviewCount > 0 ? Theme.warning : Theme.textSecondary
                            elide: Text.ElideRight
                        }
                    }
                    Icon {
                        name: "chevron-right"
                        size: 18
                        color: Theme.textTertiary
                    }
                }
                MouseArea {
                    id: smsArea
                    anchors.fill: parent
                    onClicked: root.openSmsImport()
                }
            }

            SectionTitle { text: qsTr("APPEARANCE") }
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                implicitHeight: 68

                SegmentedControl {
                    anchors.fill: parent
                    anchors.margins: 14
                    model: [
                        { value: "system", label: qsTr("System") },
                        { value: "light", label: qsTr("Light") },
                        { value: "dark", label: qsTr("Dark") }
                    ]
                    currentValue: App.settings.theme
                    onPicked: value => App.settings.theme = value
                }
            }

            SectionTitle { text: qsTr("LANGUAGE") }
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                implicitHeight: langColumn.implicitHeight + 28

                ColumnLayout {
                    id: langColumn
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    SegmentedControl {
                        Layout.fillWidth: true
                        model: App.settings.languages().map(l => ({ value: l.code, label: l.name }))
                        currentValue: App.settings.language
                        onPicked: value => App.settings.language = value
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Dates, numbers and category names follow this language. Full translation of every screen is coming soon.")
                        font.pixelSize: 12
                        color: Theme.textTertiary
                        wrapMode: Text.WordWrap
                    }
                }
            }

            SectionTitle { text: qsTr("DEFAULT CURRENCY") }
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                implicitHeight: currencyColumn.implicitHeight + 28

                ColumnLayout {
                    id: currencyColumn
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    ChipRow {
                        Layout.fillWidth: true
                        wrap: true
                        model: App.settings.currencies().slice(0, 12).map(c => ({ value: c, label: c }))
                        currentValue: App.settings.defaultCurrency
                        onPicked: value => App.settings.defaultCurrency = value
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Shown first on the Home screen and preselected for new accounts.")
                        font.pixelSize: 12
                        color: Theme.textTertiary
                        wrapMode: Text.WordWrap
                    }
                }
            }

            SectionTitle { text: qsTr("PRIVACY") }
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pagePadding
                Layout.rightMargin: Theme.pagePadding
                implicitHeight: privacyRow.implicitHeight + 32

                RowLayout {
                    id: privacyRow
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 20
                        color: Theme.accentSoft
                        Icon {
                            anchors.centerIn: parent
                            name: "lock"
                            size: 20
                            color: Theme.accent
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("All your data stays on this iPhone. Nothing is uploaded and there is no account or server. Full card numbers, CVV codes, PINs and banking passwords are never stored.")
                        font.pixelSize: 14
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 24
                text: qsTr("Personal Finance %1").arg(Qt.application.version)
                font.pixelSize: 12
                color: Theme.textTertiary
            }
        }
    }
}
