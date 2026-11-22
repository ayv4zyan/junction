import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Junction

// Asks before something is deleted: ask(title, text, action) calls `action` on "Delete".
// Enter confirms, Esc cancels.
Dialog {
    id: dialog

    property string message
    property var action: null

    function ask(title, message, action) {
        dialog.title = title
        dialog.message = message
        dialog.action = action
        open()
    }

    anchors.centerIn: Overlay.overlay
    modal: true
    width: Math.min(420, parent.width - 40)
    padding: 20
    topPadding: 18

    // Dims the window behind, so the dialog sits above it. Its opacity follows the
    // dialog's, so it fades in and out with the transitions below.
    Overlay.modal: Rectangle {
        color: Qt.alpha("black", Theme.dark ? 0.5 : 0.3)
    }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 120; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90; easing.type: Easing.InCubic }
    }

    background: Rectangle {
        radius: 10
        color: Theme.panel
        border.color: Theme.cardBorder
    }
    header: Label {
        text: dialog.title
        font.pixelSize: 16
        font.weight: Font.DemiBold
        color: Theme.text
        elide: Text.ElideRight
        leftPadding: 20
        rightPadding: 20
        topPadding: 18
    }
    contentItem: Label {
        text: dialog.message
        wrapMode: Text.Wrap
        color: Theme.mutedText
    }
    footer: RowLayout {
        spacing: 8
        Item { Layout.fillWidth: true }
        // Same shape as Delete, but neutral: only the destructive action is red.
        Button {
            id: cancelButton
            text: qsTr("Cancel")
            Layout.bottomMargin: 16
            contentItem: Label {
                text: cancelButton.text
                color: Theme.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 80
                implicitHeight: 30
                radius: 4
                // A grey fill over the panel, in either theme.
                color: Qt.alpha(Theme.text, cancelButton.down ? 0.26 : cancelButton.hovered ? 0.19 : 0.13)
                border.width: cancelButton.visualFocus ? 2 : 0
                border.color: Theme.text
            }
            onClicked: dialog.reject()
            Keys.onReturnPressed: dialog.reject()
            Keys.onEnterPressed: dialog.reject()
        }
        Button {
            id: deleteButton
            text: qsTr("Delete")
            Layout.rightMargin: 20
            Layout.bottomMargin: 16
            contentItem: Label {
                text: deleteButton.text
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 80
                implicitHeight: 30
                radius: 4
                color: deleteButton.down ? Qt.darker(Theme.danger, 1.2)
                     : deleteButton.hovered ? Qt.lighter(Theme.danger, 1.1) : Theme.danger
                border.width: deleteButton.visualFocus ? 2 : 0
                border.color: Theme.text
            }
            onClicked: dialog.accept()
            Keys.onReturnPressed: dialog.accept()
            Keys.onEnterPressed: dialog.accept()
        }
    }

    onOpened: deleteButton.forceActiveFocus()
    onAccepted: {
        const action = dialog.action
        dialog.action = null
        if (action)
            action()
    }
    onRejected: dialog.action = null
}
