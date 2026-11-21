import QtQuick

// A MouseArea that reports drags as distances in canvas (world) units, measured from the
// press. Used for moving cards, resizing and dragging Waypoints.
//
// A drag only starts once the mouse has moved a few pixels, so a plain click (or the first
// click of a double-click) never changes anything; it is reported as `tapped` instead.
MouseArea {
    id: area
    required property var canvas

    signal grabbed(var mouse)                         // pressed (not while Space pans)
    signal dragStarted()
    signal dragged(point delta, bool invertSnap)
    signal dragFinished()
    signal tapped(int modifiers)                      // released without dragging

    property point pressPoint
    property bool dragging: false

    onPressed: (mouse) => {
        if (canvas.spaceHeld) {   // Space+drag pans the canvas, even over a card
            mouse.accepted = false
            return
        }
        canvas.forceActiveFocus()
        pressPoint = mapToItem(canvas, mouse.x, mouse.y)
        dragging = false
        grabbed(mouse)
    }
    onPositionChanged: (mouse) => {
        if (!pressed)
            return
        const p = mapToItem(canvas, mouse.x, mouse.y)
        if (!dragging) {
            if (Math.abs(p.x - pressPoint.x) + Math.abs(p.y - pressPoint.y) < 4)
                return
            dragging = true
            dragStarted()
        }
        dragged(Qt.point((p.x - pressPoint.x) / canvas.zoom, (p.y - pressPoint.y) / canvas.zoom),
                (mouse.modifiers & Qt.AltModifier) !== 0)
    }
    onReleased: (mouse) => {
        if (dragging) {
            dragging = false
            dragFinished()
        } else {
            tapped(mouse.modifiers)
        }
    }
    onCanceled: {
        if (dragging) {
            dragging = false
            dragFinished()
        }
    }
}
