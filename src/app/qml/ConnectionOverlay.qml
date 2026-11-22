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
    required property var canvas
    readonly property Editor editor: canvas.editor
    readonly property var info: editor.selectedConnection
    readonly property bool active: info.id !== undefined
    // The label's box, or an empty one (also for a moment while the selection changes).
    readonly property rect labelBox: info.labelRect ?? Qt.rect(0, 0, 0, 0)
    property bool draggingWaypoint: false

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
        const points = info.midpoints ?? []
        const centre = info.labelPosition
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

    onInfoChanged: {
        sync(midpoints, info.midpoints)
        sync(waypoints, info.waypoints)
    }

    Repeater {
        model: midpoints
        delegate: Dot {
            id: midpoint
            required property int index
            required property real px
            required property real py
            readonly property bool covered: overlay.onLabel(px, py)        // under the label's text
            readonly property bool isLabel: index === overlay.labelSpan   // the label stands in for it
            canvas: overlay.canvas
            center: Qt.point(px, py)
            filled: false
            screenRadius: 5
            opacity: overlay.draggingWaypoint || covered ? 0 : 1   // the label's own dot still takes the mouse

            DragArea {
                readonly property real margin: 4 / overlay.canvas.zoom
                canvas: overlay.canvas
                visible: !midpoint.covered || midpoint.isLabel
                // the dot plus a margin, or the whole label (in the dot's coordinates)
                x: midpoint.isLabel ? overlay.labelBox.x - midpoint.x : -margin
                y: midpoint.isLabel ? overlay.labelBox.y - midpoint.y : -margin
                width: midpoint.isLabel ? overlay.labelBox.width : midpoint.width + 2 * margin
                height: midpoint.isLabel ? overlay.labelBox.height : midpoint.height + 2 * margin
                cursorShape: Qt.SizeAllCursor   // dragging it adds a Waypoint
                onDragStarted: { overlay.draggingWaypoint = true; overlay.editor.beginWaypointDrag(overlay.info.id, midpoint.index, true) }
                onDragged: (delta, invertSnap) => overlay.editor.updateWaypointDrag(delta, invertSnap)
                onDragFinished: { overlay.editor.endWaypointDrag(); overlay.draggingWaypoint = false }
                // the middle dot sits on the label, so double-clicking it edits the label
                onDoubleClicked: overlay.canvas.editLabel(overlay.info.id)
                onMenuRequested: overlay.canvas.openMenuFor(overlay.info.id)
            }
        }
    }

    Repeater {
        model: waypoints
        delegate: Dot {
            id: waypoint
            required property int index
            required property real px
            required property real py
            canvas: overlay.canvas
            center: Qt.point(px, py)

            DragArea {
                canvas: overlay.canvas
                anchors.fill: parent
                anchors.margins: -4 / overlay.canvas.zoom
                cursorShape: Qt.SizeAllCursor
                onDragStarted: { overlay.draggingWaypoint = true; overlay.editor.beginWaypointDrag(overlay.info.id, waypoint.index, false) }
                onDragged: (delta, invertSnap) => overlay.editor.updateWaypointDrag(delta, invertSnap)
                onDragFinished: { overlay.editor.endWaypointDrag(); overlay.draggingWaypoint = false }
                onDoubleClicked: overlay.editor.removeWaypoint(overlay.info.id, waypoint.index)
                onMenuRequested: overlay.canvas.openMenuFor(overlay.info.id, waypoint.index)
            }
        }
    }

    Repeater {
        model: overlay.active ? 2 : 0
        delegate: Dot {
            id: end
            required property int index
            readonly property bool fromEnd: index === 0
            property bool dragging: false
            property point dragPoint
            canvas: overlay.canvas
            center: dragging ? dragPoint : (fromEnd ? overlay.info.start : overlay.info.end) ?? Qt.point(0, 0)

            DragArea {
                canvas: overlay.canvas
                anchors.fill: parent
                anchors.margins: -4 / overlay.canvas.zoom
                cursorShape: Qt.CrossCursor   // re-attaching an end, like connecting from a Handle
                property point origin
                onDragStarted: { origin = end.center; end.dragPoint = origin; end.dragging = true }
                onDragged: (delta) => {
                    end.dragPoint = Qt.point(origin.x + delta.x, origin.y + delta.y)
                    overlay.canvas.dropTarget = overlay.editor.cardAt(end.dragPoint, overlay.canvas.handleRadius)
                }
                onDragFinished: {
                    const target = overlay.editor.cardAt(end.dragPoint, overlay.canvas.handleRadius)
                    if (end.dragging && target)
                        overlay.editor.reattach(overlay.info.id, end.fromEnd, target,
                                                overlay.editor.handleAt(target, end.dragPoint, overlay.canvas.handleRadius))
                    end.dragging = false
                    overlay.canvas.dropTarget = ""
                }
                onMenuRequested: overlay.canvas.openMenuFor(overlay.info.id)
            }
        }
    }
}
