pragma ComponentBehavior: Bound
import QtQuick
import Junction

// Handles of the selected Connection, in canvas coordinates:
//  - hollow dots in the middle of each span: drag to create a Waypoint there,
//    double-click to edit the label. A dot under the label would cover its text, so there
//    the label itself is the handle and the dot is not shown.
//  - filled dots on Waypoints: drag to move, double-click to remove
//  - dots on both ends: drag onto a card to re-attach (onto a Handle to pin that side)
Item {
    id: overlay
    required property CanvasView canvasView
    readonly property Editor editor: canvasView.editor
    readonly property var connection: editor.selectedConnection   // see Editor::selectedConnection()
    readonly property bool active: connection.valid
    readonly property rect labelBox: connection.labelRect   // empty without a label

    // Dragging a Waypoint (`inserted`: a new one at midpoint `index`); one undo step.
    component WaypointDragArea: DragArea {
        id: waypointDrag
        required property Editor editor
        required property string connectionId
        required property int index
        required property bool inserted
        cursorShape: Qt.SizeAllCursor
        onDragStarted: editor.beginWaypointDrag(connectionId, index, inserted)
        onDragged: (delta, invertSnap) => waypointDrag.editor.updateWaypointDrag(delta, invertSnap)
        onDragFinished: editor.endWaypointDrag()
    }

    // The dots come from ListModels that are updated in place. Rebuilding a Repeater's model
    // would destroy the dot under the mouse in the middle of a drag.
    ListModel { id: midpoints }
    ListModel { id: waypoints }

    function sync(model, points) {
        points = points ?? []
        while (model.count > points.length)
            model.remove(model.count - 1)
        for (let i = 0; i < points.length; ++i) {
            if (i < model.count)
                model.set(i, { px: points[i].x, py: points[i].y })
            else
                model.append({ px: points[i].x, py: points[i].y })
        }
    }

    // Whether a point is on the label (empty when there is none).
    function onLabel(x, y) {
        const r = labelBox
        return r.width > 0
               && x >= r.x && x <= r.x + r.width && y >= r.y && y <= r.y + r.height
    }

    // The span whose middle the label sits on (the midpoint on the label nearest its centre), or
    // -1. Only that span's dot hands its drag to the whole label; a long label can cover the
    // dots of neighbouring spans too, and those are just hidden.
    readonly property int labelSpan: {
        const points = connection.midpoints
        const centre = connection.labelPosition
        let best = -1
        let bestDistance = Infinity
        for (let i = 0; centre && i < points.length; ++i) {
            const distance = Math.hypot(points[i].x - centre.x, points[i].y - centre.y)
            if (onLabel(points[i].x, points[i].y) && distance < bestDistance) {
                best = i
                bestDistance = distance
            }
        }
        return best
    }

    onConnectionChanged: {
        sync(midpoints, connection.midpoints)
        sync(waypoints, connection.waypoints)
    }

    Repeater {
        model: midpoints
        delegate: PointMarker {
            id: midpoint
            required property int index
            required property real px
            required property real py
            readonly property bool covered: overlay.onLabel(px, py)        // under the label's text
            readonly property bool isLabel: index === overlay.labelSpan   // the label stands in for it
            canvasView: overlay.canvasView
            center: Qt.point(px, py)
            filled: false
            screenRadius: 5
            opacity: overlay.editor.dragging || covered ? 0 : 1   // the label's own dot still takes the mouse

            // What grabs it, in the dot's coordinates: the whole label when that stands in for
            // it, otherwise the dot plus a margin.
            readonly property rect grabArea: {
                if (isLabel)
                    return Qt.rect(overlay.labelBox.x - x, overlay.labelBox.y - y, overlay.labelBox.width, overlay.labelBox.height)
                const margin = overlay.canvasView.markerMargin
                return Qt.rect(-margin, -margin, width + 2 * margin, height + 2 * margin)
            }

            WaypointDragArea {
                canvasView: overlay.canvasView
                editor: overlay.editor
                connectionId: overlay.connection.id
                index: midpoint.index
                inserted: true   // dragging it adds a Waypoint
                visible: !midpoint.covered || midpoint.isLabel
                x: midpoint.grabArea.x
                y: midpoint.grabArea.y
                width: midpoint.grabArea.width
                height: midpoint.grabArea.height
                // the middle dot sits on the label, so double-clicking it edits the label
                onDoubleClicked: overlay.canvasView.editLabel(overlay.connection.id)
                onMenuRequested: overlay.canvasView.openMenuFor(overlay.connection.id)
            }
        }
    }

    Repeater {
        model: waypoints
        delegate: PointMarker {
            id: waypoint
            required property int index
            required property real px
            required property real py
            canvasView: overlay.canvasView
            center: Qt.point(px, py)

            WaypointDragArea {
                canvasView: overlay.canvasView
                editor: overlay.editor
                connectionId: overlay.connection.id
                index: waypoint.index
                inserted: false
                anchors.fill: parent
                anchors.margins: -overlay.canvasView.markerMargin
                onDoubleClicked: overlay.editor.removeWaypoint(overlay.connection.id, waypoint.index)
                onMenuRequested: overlay.canvasView.openMenuFor(overlay.connection.id, waypoint.index)
            }
        }
    }

    Repeater {
        model: overlay.active ? 2 : 0
        delegate: PointMarker {
            id: end
            required property int index
            readonly property bool fromEnd: index === 0
            property bool dragging: false
            property point dragPoint
            canvasView: overlay.canvasView
            center: dragging ? dragPoint : (fromEnd ? overlay.connection.start : overlay.connection.end) ?? Qt.point(0, 0)

            DragArea {
                canvasView: overlay.canvasView
                anchors.fill: parent
                anchors.margins: -overlay.canvasView.markerMargin
                cursorShape: Qt.CrossCursor   // re-attaching an end, like connecting from a Handle
                property point origin
                onDragStarted: { origin = end.center; end.dragPoint = origin; end.dragging = true }
                onDragged: (delta) => {
                    end.dragPoint = Qt.point(origin.x + delta.x, origin.y + delta.y)
                    overlay.canvasView.showDropTarget(overlay.canvasView.attachmentAt(end.dragPoint, "").node)
                }
                onDragFinished: {
                    const at = overlay.canvasView.attachmentAt(end.dragPoint, "")
                    if (end.dragging && at.node)
                        overlay.editor.reattach(overlay.connection.id, end.fromEnd, at.node, at.side)
                    end.dragging = false
                    overlay.canvasView.showDropTarget("")
                }
                onMenuRequested: overlay.canvasView.openMenuFor(overlay.connection.id)
            }
        }
    }
}
