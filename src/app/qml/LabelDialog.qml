import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Junction

// Sets one Label on several Groups and Connections at once:
// ask(count, text, action) calls `action(newText)` on "Save". Enter saves, Esc cancels.
// Looks like ConfirmDialog; Save uses the accent color, not red, since nothing is destroyed.
Dialog {
    id: dialog

    property var action: null

    function ask(count, text, action) {
        dialog.title = qsTr("Edit %1 labels").arg(count)
        field.text = text
        dialog.action = action
        open()
    }

    anchors.centerIn: Overlay.overlay
    modal: true
    width: Math.min(420, parent.width - 40)
    padding: 20
    topPadding: 18

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
    contentItem: TextField {
        id: field
        placeholderText: qsTr("Label")
        ContextMenu.menu: TextMenu { editor: field }
        onAccepted: dialog.accept()
    }
    footer: RowLayout {
        spacing: 8
        Item { Layout.fillWidth: true }
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
                color: Qt.alpha(Theme.text, cancelButton.down ? 0.26 : cancelButton.hovered ? 0.19 : 0.13)
                border.width: cancelButton.visualFocus ? 2 : 0
                border.color: Theme.text
            }
            onClicked: dialog.reject()
        }
        Button {
            id: saveButton
            text: qsTr("Save")
            Layout.rightMargin: 20
            Layout.bottomMargin: 16
            contentItem: Label {
                text: saveButton.text
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 80
                implicitHeight: 30
                radius: 4
                color: saveButton.down ? Qt.darker(Theme.selection, 1.2)
                     : saveButton.hovered ? Qt.lighter(Theme.selection, 1.1) : Theme.selection
                border.width: saveButton.visualFocus ? 2 : 0
                border.color: Theme.text
            }
            onClicked: dialog.accept()
        }
    }

    onOpened: {
        field.forceActiveFocus()
        field.selectAll()
    }
    onAccepted: {
        const action = dialog.action
        dialog.action = null
        if (action)
            action(field.text)
    }
    onRejected: dialog.action = null
}
