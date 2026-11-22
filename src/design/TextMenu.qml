import QtQuick
import QtQuick.Controls

// The right-click menu while typing: it acts on the text, not on what the text belongs to.
Menu {
    id: menu
    required property Item editor   // a TextEdit, TextInput or TextField

    // The menu takes keyboard focus while open. Editors don't stop editing while it is open
    // (they check `visible`), and get the focus back when it closes, unless editing ended
    // meanwhile (something else was clicked) and the editor is gone.
    onClosed: if (menu.editor.visible) menu.editor.forceActiveFocus()

    MenuEntry {
        text: qsTr("Cut")
        hint: "Ctrl+X"
        enabled: menu.editor.selectedText !== ""
        onTriggered: menu.editor.cut()
    }
    MenuEntry {
        text: qsTr("Copy")
        hint: "Ctrl+C"
        enabled: menu.editor.selectedText !== ""
        onTriggered: menu.editor.copy()
    }
    MenuEntry {
        text: qsTr("Paste")
        hint: "Ctrl+V"
        enabled: menu.editor.canPaste
        onTriggered: menu.editor.paste()
    }
    MenuSeparator {}
    MenuEntry {
        text: qsTr("Select all")
        hint: "Ctrl+A"
        onTriggered: menu.editor.selectAll()
    }
}
