import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Controls as Controls
import QtQuick.Layouts
import PersonalFinance
import PersonalFinance.Core

// Modal bottom sheet with a grabber, title, close button and a scrollable body.
// Children declared inside the sheet are added to the body column.
Popup {
    id: root

    property string title: ""
    default property alias content: body.data
    // Pinned below the scrolling body (primary action and its feedback).
    property alias actions: actionColumn.data

    // Lift the sheet above the on-screen keyboard.
    readonly property real keyboardInset: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0
    readonly property real sheetMaxHeight: parent ? parent.height - App.platform.safeAreaTop - 12 : 700

    parent: Controls.Overlay.overlay
    modal: true
    dim: true
    focus: true
    padding: 0
    x: 0
    width: parent ? parent.width : 390
    height: Math.min(sheetMaxHeight, implicitContentHeight)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // 0 = fully shown, 1 = fully below the screen. Animating this (instead of
    // `y` with fixed end values) keeps `y` a live binding, so the sheet stays
    // glued to the bottom edge when its content height changes.
    property real slide: 1
    y: parent ? parent.height - (height + keyboardInset) * (1 - slide) : 0

    enter: Transition {
        NumberAnimation {
            target: root
            property: "slide"
            from: 1
            to: 0
            duration: 300
            easing.type: Easing.OutCubic
        }
    }
    exit: Transition {
        NumberAnimation {
            target: root
            property: "slide"
            to: 1
            duration: 220
            easing.type: Easing.InCubic
        }
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.bottomMargin: -40 // keeps the bottom corners square (off-screen)
            radius: 28
            color: Theme.surface
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            Layout.preferredWidth: 38
            Layout.preferredHeight: 5
            radius: 2.5
            color: Theme.border
        }

        RowLayout {
            id: header
            Layout.fillWidth: true
            Layout.leftMargin: Theme.pagePadding
            Layout.rightMargin: 12
            Layout.topMargin: 6
            Layout.bottomMargin: 10

            Text {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Theme.fontTitle
                font.weight: Font.Bold
                color: Theme.text
                elide: Text.ElideRight
            }

            AbstractButton {
                implicitWidth: 40
                implicitHeight: 40
                onClicked: root.close()
                background: Rectangle {
                    radius: 20
                    color: Theme.field
                }
                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        name: "close"
                        size: 18
                        color: Theme.textSecondary
                    }
                }
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: body.implicitHeight
            contentWidth: width
            contentHeight: body.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            ColumnLayout {
                id: body
                x: Theme.pagePadding
                width: flick.width - 2 * Theme.pagePadding
                spacing: 16
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
            visible: flick.interactive && actionColumn.visible
        }

        ColumnLayout {
            id: actionColumn
            Layout.fillWidth: true
            Layout.leftMargin: Theme.pagePadding
            Layout.rightMargin: Theme.pagePadding
            Layout.topMargin: 12
            spacing: 10
            visible: children.length > 0 // not visibleChildren: that depends on our own visibility
        }

        Item {
            Layout.preferredHeight: App.platform.safeAreaBottom + 16
        }
    }
}
