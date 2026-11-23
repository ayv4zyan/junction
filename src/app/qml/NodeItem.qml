import QtQuick
import Junction
import JunctionDesign
import "CanvasColors.js" as CanvasColors

// What every Card and Group on the canvas has: it sits at its node's rectangle, can be
// selected and edited in place, and has its color. CardItem and GroupItem build on it.
Item {
	// The CanvasView this is part of. Typed Item: Qt 6.9 rejects a type naming the view that creates it.
	required property Item canvasView
	required property Editor editor
	required property string nodeId
	required property real nodeX
	required property real nodeY
	required property real nodeWidth
	required property real nodeHeight
	required property string nodeColor

	readonly property bool selected: editor.selection.ids.includes(nodeId)
	readonly property bool editing: canvasView.editingNodeId === nodeId
	readonly property color accent: CanvasColors.resolve(nodeColor, Colors.presets, Colors.cardBorder)
	// The outline: the selection color while selected, otherwise the node's own.
	readonly property color borderColor: selected ? Colors.selection : accent
	readonly property real borderWidth: selected ? 2 : 1.5

	x: nodeX
	y: nodeY
	width: nodeWidth
	height: nodeHeight
}
