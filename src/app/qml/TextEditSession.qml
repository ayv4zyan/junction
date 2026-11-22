import QtQuick
import JunctionDesign

// What every in-place text editor on the Canvas does (a Card's text, a Group's or a
// Connection's Label):
// - when the field appears, it gets the keyboard and `startText`, all selected or with the
//   cursor at the end;
// - when it loses the keyboard, editing is done (`finished`), except while the right-click
//   text menu has it.
// A right-click on the area this item covers opens that menu. Enter and Esc end editing by
// giving the keyboard back to the canvas (the editors do that themselves).
Item {
    id: session
    required property Item field   // the TextEdit or TextInput, shown only while editing
    property string startText
    property bool selectAll: true

    signal finished(string text)

    function openMenu() { menu.popup() }
    function closeMenu() { menu.close() }

    visible: field.visible

    Connections {
        target: session.field
        // Only a visible item can take the keyboard, so this waits for the field to show.
        function onVisibleChanged() {
            if (!session.field.visible)
                return
            session.field.text = session.startText
            session.field.forceActiveFocus()
            if (session.selectAll)
                session.field.selectAll()
            else
                session.field.cursorPosition = session.field.length
        }
        function onActiveFocusChanged() {
            if (!session.field.activeFocus && session.field.visible && !menu.visible)
                session.finished(session.field.text)
        }
    }

    // Takes right-clicks only; left clicks go through to the editor.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onPressed: menu.popup()
    }

    TextMenu {
        id: menu
        editor: session.field
    }
}
