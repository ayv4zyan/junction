import QtQuick

// The keyboard-focus outline: 1.5px of accent, just outside its parent. Shown only for keyboard
// focus (`visualFocus`), so it says where Enter and Space go.
Rectangle {
    anchors.fill: parent
    anchors.margins: -3
    color: "transparent"
    border.color: Colors.selection
    border.width: 1.5
}
