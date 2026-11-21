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
        Button {
            text: qsTr("Cancel")
            onClicked: dialog.reject()
            Layout.bottomMargin: 16
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
