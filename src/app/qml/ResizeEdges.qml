pragma ComponentBehavior: Bound
import QtQuick

// Invisible strips along the border of a Card or Group, like Obsidian: hovering one shows a
// resize cursor, dragging it moves that side, or both sides at a corner.
Item {
    id: edges
    required property CanvasView canvas
    required property string nodeId
    readonly property real thickness: 10 / canvas.zoom   // half inside the border, half outside
    property int hoveredCount: 0
    readonly property bool hovered: hoveredCount > 0

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
            readonly property real t: edges.thickness

            canvas: edges.canvas
            hoverEnabled: true
            x: horizontal < 0 ? -t / 2 : horizontal > 0 ? edges.width - t / 2 : t / 2
            y: vertical < 0 ? -t / 2 : vertical > 0 ? edges.height - t / 2 : t / 2
            width: horizontal === 0 ? edges.width - t : t
            height: vertical === 0 ? edges.height - t : t
            cursorShape: horizontal === 0 ? Qt.SizeVerCursor
                       : vertical === 0 ? Qt.SizeHorCursor
                       : horizontal === vertical ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor

            onContainsMouseChanged: edges.hoveredCount += containsMouse ? 1 : -1
            onTapped: (modifiers) => edges.canvas.editor.select(edges.nodeId, edges.canvas.isToggle(modifiers))
            onDragStarted: edges.canvas.editor.beginResize(edges.nodeId, strip.sides)
            onDragged: (delta, invertSnap) => edges.canvas.editor.updateResize(delta, invertSnap)
            onDragFinished: edges.canvas.editor.endResize()
            onMenuRequested: edges.canvas.openMenuFor(edges.nodeId)
        }
    }
}
