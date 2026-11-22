import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The one dialog for asking the user something, in two modes:
//   ask.confirm({ title, message, action: qsTr("Delete"), destructive: true, onAccepted: () => ... })
//   ask.text({ title, text, placeholder, action: qsTr("Save"), onAccepted: (text) => ... })
// Enter accepts (the action button shows the focus ring), Esc cancels. The action is red only
// when it destroys something.
Dialog {
    id: dialog

    property bool asksText: false
    property string message
    property string actionText
    property bool destructive: false
    property var handler: null

    function confirm(options) {
        asksText = false
        setUp(options)
        open()
    }

    function text(options) {
        asksText = true
        field.text = options.text ?? ""
        field.placeholderText = options.placeholder ?? ""
        setUp(options)
        open()
    }

    function setUp(options) {
        title = options.title ?? ""
        message = options.message ?? ""
        actionText = options.action ?? qsTr("OK")
        destructive = options.destructive === true
        handler = options.onAccepted ?? null
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: Math.min(400, (parent ? parent.width : 400) - 2 * Space.xl)

    contentItem: ColumnLayout {
        spacing: Space.md
        Label {
            visible: dialog.message !== ""
            Layout.fillWidth: true
            text: dialog.message
            wrapMode: Text.Wrap
            color: Colors.mutedText
        }
        TextField {
            id: field
            visible: dialog.asksText
            Layout.fillWidth: true
            onAccepted: dialog.accept()
            TextMenu { id: fieldMenu; editor: field }
            TapHandler { acceptedButtons: Qt.RightButton; onTapped: fieldMenu.popup() }
        }
    }

    footer: Item {
        implicitHeight: buttons.implicitHeight + Space.xl
        RowLayout {
            id: buttons
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Space.xl
            anchors.rightMargin: Space.xl
            spacing: Space.sm
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Cancel")
                onClicked: dialog.reject()
                Keys.onReturnPressed: dialog.reject()
                Keys.onEnterPressed: dialog.reject()
            }
            Button {
                id: actionButton
                text: dialog.actionText
                highlighted: true
                palette.highlight: dialog.destructive ? Colors.dangerFill : Colors.accentFill
                onClicked: dialog.accept()
                Keys.onReturnPressed: dialog.accept()
                Keys.onEnterPressed: dialog.accept()
            }
        }
    }

    onOpened: {
        if (asksText) {
            field.forceActiveFocus()
            field.selectAll()
        } else {
            actionButton.forceActiveFocus(Qt.TabFocusReason)   // a keyboard reason: shows the focus ring
        }
    }
    onAccepted: {
        const handler = dialog.handler
        dialog.handler = null
        if (handler)
            asksText ? handler(field.text) : handler()
    }
    onRejected: dialog.handler = null
}
