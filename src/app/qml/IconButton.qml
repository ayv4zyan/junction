import QtQuick
import QtQuick.Controls
import JunctionDesign

// A square tool button showing one of the icons in icons/, with a tooltip. Used by the toolbar,
// the sidebar and the selection toolbar; its look comes from the style's ToolButton. Where
// `checked` is set it only shows state: clicking calls the Editor, and the Editor's new state
// updates `checked`.
ToolButton {
	property string iconName
	property string tip

	focusPolicy: Qt.NoFocus         // keep keyboard focus on the canvas
	display: AbstractButton.IconOnly
	icon.source: "../icons/" + iconName + ".svg"
	ToolTip.visible: hovered && tip !== ""
	ToolTip.delay: Motion.tooltipDelay
	ToolTip.text: tip
}
