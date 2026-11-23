pragma ComponentBehavior: Bound
import QtQuick
import Junction

// Invisible strips along the border of a Card or Group, like Obsidian: hovering one shows a
// resize cursor, dragging it moves that side, or both sides at a corner.
Item {
	id: edges
	// The CanvasView this is part of. Typed Item: Qt 6.9 rejects a type naming the view that creates it.
	required property Item canvasView
	required property Editor editor
	required property string nodeId
	readonly property real thickness: 10 / canvasView.zoom   // half inside the border, half outside

	anchors.fill: parent

	// The sides each strip moves: four corners and four sides.
	Repeater {
		model: [Qt.LeftEdge | Qt.TopEdge, Qt.TopEdge, Qt.RightEdge | Qt.TopEdge, Qt.RightEdge,
				Qt.RightEdge | Qt.BottomEdge, Qt.BottomEdge, Qt.LeftEdge | Qt.BottomEdge, Qt.LeftEdge]
		delegate: DragArea {
			id: strip
			required property int modelData
			readonly property int sides: modelData
			// -1 = the left (top) side moves, 1 = the right (bottom) one, 0 = neither
			readonly property int horizontal: sides & Qt.LeftEdge ? -1 : sides & Qt.RightEdge ? 1 : 0
			readonly property int vertical: sides & Qt.TopEdge ? -1 : sides & Qt.BottomEdge ? 1 : 0
			readonly property real across: edges.thickness   // the strip's width across the border

			canvasView: edges.canvasView
			hoverEnabled: true
			x: horizontal < 0 ? -across / 2 : horizontal > 0 ? edges.width - across / 2 : across / 2
			y: vertical < 0 ? -across / 2 : vertical > 0 ? edges.height - across / 2 : across / 2
			width: horizontal === 0 ? edges.width - across : across
			height: vertical === 0 ? edges.height - across : across
			cursorShape: horizontal === 0 ? Qt.SizeVerCursor
					   : vertical === 0 ? Qt.SizeHorCursor
					   : horizontal === vertical ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor

			onTapped: (modifiers) => edges.editor.selection.select(edges.nodeId, edges.canvasView.isToggle(modifiers))
			onDragStarted: edges.editor.beginResize(edges.nodeId, strip.sides)
			onDragged: (delta, invertSnap) => edges.editor.updateResize(delta, invertSnap)
			onDragFinished: edges.editor.endResize()
			onMenuRequested: edges.canvasView.openMenuFor(edges.nodeId)
		}
	}
}
