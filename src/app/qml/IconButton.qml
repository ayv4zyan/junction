import QtQuick
import QtQuick.Controls
import Junction

// A square tool button showing one of the icons in icons/, with a tooltip. Used by the toolbar
// and the selection bar. Where `checked` is set it only shows state: clicking calls the Editor,
// and the Editor's new state updates `checked`.
ToolButton {
    property string iconName
    property string tip

    implicitWidth: implicitHeight   // square
    focusPolicy: Qt.NoFocus         // keep keyboard focus on the canvas
    display: AbstractButton.IconOnly
    icon.source: "../icons/" + iconName + ".svg"
    icon.width: 20
    icon.height: 20
    icon.color: Theme.text
    opacity: enabled ? 1 : 0.35
    ToolTip.visible: hovered
    ToolTip.delay: 400
    ToolTip.text: tip
}
