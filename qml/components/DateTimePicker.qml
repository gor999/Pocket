import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

// Wheel-style date and time picker in a bottom sheet.
SheetPopup {
    id: root

    property date value: new Date()
    readonly property int firstYear: new Date().getFullYear() - 10

    signal picked(date value)

    title: qsTr("Date and time")

    function openWith(d) {
        value = d
        open()
    }

    // Wheels are only laid out once the popup is showing, so position them then
    // (positionViewAtIndex snaps exactly, without animating from index 0).
    function syncWheels() {
        const d = value
        const targets = [[dayWheel, d.getDate() - 1],
                         [monthWheel, d.getMonth()],
                         [yearWheel, Math.max(0, Math.min(yearWheel.count - 1, d.getFullYear() - firstYear))],
                         [hourWheel, d.getHours()],
                         [minuteWheel, d.getMinutes()]]
        for (const [wheel, index] of targets) {
            wheel.currentIndex = index
            wheel.positionViewAtIndex(index, Tumbler.Center)
        }
    }

    onAboutToShow: {
        syncWheels()
        Qt.callLater(syncWheels)
    }

    function selectedDate() {
        const year = firstYear + yearWheel.currentIndex
        const month = monthWheel.currentIndex
        const daysInMonth = new Date(year, month + 1, 0).getDate()
        const day = Math.min(dayWheel.currentIndex + 1, daysInMonth)
        return new Date(year, month, day, hourWheel.currentIndex, minuteWheel.currentIndex, 0)
    }

    component Wheel: Tumbler {
        id: wheel
        property var labelFor: function (i) { return String(i) }
        property real wheelWidth: 70

        Layout.preferredWidth: wheelWidth
        Layout.fillWidth: wheelWidth < 0
        Layout.preferredHeight: 190
        visibleItemCount: 5
        delegate: Component {
            Text {
                required property int index
                required property var modelData
                text: wheel.labelFor(index)
                font.pixelSize: 19
                font.weight: Math.abs(Tumbler.displacement) < 0.5 ? Font.DemiBold : Font.Normal
                color: Theme.text
                opacity: 1.0 - Math.min(1.0, Math.abs(Tumbler.displacement) / (wheel.visibleItemCount / 2)) * 0.8
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 0

        Wheel {
            id: dayWheel
            model: 31
            labelFor: function (i) { return String(i + 1) }
            wheelWidth: 56
        }
        Wheel {
            id: monthWheel
            model: 12
            labelFor: function (i) { return Qt.locale(App.settings.localeName).monthName(i, Locale.ShortFormat) }
            wheelWidth: 90
        }
        Wheel {
            id: yearWheel
            model: 12
            wrap: false // years must not loop around
            labelFor: function (i) { return String(root.firstYear + i) }
            wheelWidth: 74
        }
        Item { Layout.fillWidth: true }
        Wheel {
            id: hourWheel
            model: 24
            labelFor: function (i) { return String(i).padStart(2, "0") }
            wheelWidth: 46
        }
        Text {
            text: ":"
            font.pixelSize: 19
            font.weight: Font.DemiBold
            color: Theme.textSecondary
        }
        Wheel {
            id: minuteWheel
            model: 60
            labelFor: function (i) { return String(i).padStart(2, "0") }
            wheelWidth: 46
        }
    }

    PrimaryButton {
        Layout.fillWidth: true
        text: qsTr("Done")
        onClicked: {
            root.picked(root.selectedDate())
            root.close()
        }
    }
}
