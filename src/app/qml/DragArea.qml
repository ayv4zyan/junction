import QtQuick

// A MouseArea that reports drags as distances in canvas (world) units, measured from the
// press. Used for moving cards, resizing and dragging Waypoints.
//
// A drag only starts once the mouse has moved a few pixels, so a plain click (or the first
// click of a double-click) never changes anything; it is reported as `tapped` instead.
// A right-click never drags: it is reported as `menuRequested`.
MouseArea {
    id: area
    // The CanvasView this is part of. Typed Item: Qt 6.9 rejects a type naming the view that creates it.
    required property Item canvasView
    acceptedButtons: Qt.LeftButton | Qt.RightButton

    signal grabbed(var mouse)                         // pressed (not while Space pans)
    signal dragStarted()
    signal dragged(point delta, bool invertSnap)
    signal dragFinished()
    signal tapped(int modifiers)                      // released without dragging
    signal menuRequested()                            // right-clicked

    property point pressPoint
    property bool dragging: false

    onPressed: (mouse) => {
        if (canvasView.spaceHeld) {   // Space+drag pans the canvas, even over a card
            mouse.accepted = false
            return
        }
        canvasView.forceActiveFocus()
        if (mouse.button === Qt.RightButton) {
            menuRequested()
            return
        }
        pressPoint = mapToItem(canvasView, mouse.x, mouse.y)
        dragging = false
        grabbed(mouse)
    }
    onPositionChanged: (mouse) => {
        if (!(pressedButtons & Qt.LeftButton))
            return
        const p = mapToItem(canvasView, mouse.x, mouse.y)
        if (!dragging) {
            if (Math.abs(p.x - pressPoint.x) + Math.abs(p.y - pressPoint.y) < 4)
                return
            dragging = true
            dragStarted()
        }
        dragged(Qt.point((p.x - pressPoint.x) / canvasView.zoom, (p.y - pressPoint.y) / canvasView.zoom),
                (mouse.modifiers & Qt.AltModifier) !== 0)
    }
    onReleased: (mouse) => {
        if (mouse.button !== Qt.LeftButton)
            return
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
