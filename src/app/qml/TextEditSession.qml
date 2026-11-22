import QtQuick
import JunctionDesign

// What every in-place text editor on the Canvas does (a Card's text, a Group's or a
// Connection's Label):
// - when the editor appears, it gets the keyboard and `startText`, all selected or with the
//   cursor at the end;
// - when it loses the keyboard, editing is done (`finished`), except while the right-click
//   text menu has it.
// A right-click on the area this item covers opens that menu. Enter and Esc end editing by
// giving the keyboard back to the canvas (the editors do that themselves).
Item {
    id: session
    required property Item editor   // a TextEdit or TextInput, shown only while editing
    property string startText
    property bool selectAll: true

    signal finished(string text)

    function openMenu() { menu.popup() }
    function closeMenu() { menu.close() }

    visible: editor.visible

    Connections {
        target: session.editor
        // Only a visible item can take the keyboard, so this waits for the editor to show.
        function onVisibleChanged() {
            if (!session.editor.visible)
                return
            session.editor.text = session.startText
            session.editor.forceActiveFocus()
            if (session.selectAll)
                session.editor.selectAll()
            else
                session.editor.cursorPosition = session.editor.length
        }
        function onActiveFocusChanged() {
            if (!session.editor.activeFocus && session.editor.visible && !menu.visible)
                session.finished(session.editor.text)
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
        editor: session.editor
    }
}
