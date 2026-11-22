import QtQuick
import Junction

// Presses that hit nothing on the canvas: drag to pan (middle button, or Space held) or to
// select with a rectangle, click a Connection to select it, right-click for a menu,
// double-click to add a Card (or edit a Connection's label), and the wheel to scroll and zoom.
MouseArea {
    id: background
    required property CanvasView canvasView
    required property CanvasMenus menus

    enum Mode { Idle, Panning, Selecting }   // what the press started

    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
    hoverEnabled: true
    cursorShape: mode === CanvasBackground.Panning ? Qt.ClosedHandCursor
               : canvasView.spaceHeld ? Qt.OpenHandCursor
               : overConnection ? Qt.PointingHandCursor : Qt.ArrowCursor
    property int mode: CanvasBackground.Idle
    property rect selectionRect   // the rectangle being dragged, in this item's coordinates
    property bool overConnection: false
    property point last
    property point rubberStart
    property var rubberBase: []   // what stays selected besides the rectangle's catch (Shift/Ctrl)

    onPressed: (mouse) => {
        canvasView.forceActiveFocus()
        last = Qt.point(mouse.x, mouse.y)
        if (mouse.button === Qt.MiddleButton || canvasView.spaceHeld) {
            mode = CanvasBackground.Panning
            return
        }
        const hit = canvasView.editor.connectionAt(canvasView.toWorld(last), canvasView.connectionTolerance)
        if (mouse.button === Qt.RightButton) {
            if (hit) {
                canvasView.openMenuFor(hit)
            } else {
                canvasView.editor.clearSelection()
                background.menus.openAt(canvasView.toWorld(last))
            }
            return
        }
        const additive = canvasView.isToggle(mouse.modifiers)
        if (hit) {
            canvasView.editor.select(hit, additive)
            mode = CanvasBackground.Idle
            return
        }
        if (!additive)
            canvasView.editor.clearSelection()
        mode = CanvasBackground.Selecting
        selectionRect = Qt.rect(last.x, last.y, 0, 0)
        rubberStart = last
        rubberBase = canvasView.editor.selection
    }

    onPositionChanged: (mouse) => {
        if (!pressed) {
            overConnection = canvasView.editor.connectionAt(canvasView.toWorld(Qt.point(mouse.x, mouse.y)), canvasView.connectionTolerance) !== ""
        } else if (mode === CanvasBackground.Panning) {
            canvasView.panBy(mouse.x - last.x, mouse.y - last.y)
            last = Qt.point(mouse.x, mouse.y)
        } else if (mode === CanvasBackground.Selecting) {
            selectionRect = Qt.rect(Math.min(rubberStart.x, mouse.x), Math.min(rubberStart.y, mouse.y),
                                    Math.abs(mouse.x - rubberStart.x), Math.abs(mouse.y - rubberStart.y))
            // The selection follows the rectangle as it is dragged, not only on release.
            const a = canvasView.toWorld(Qt.point(selectionRect.x, selectionRect.y))
            canvasView.editor.selectInRect(Qt.rect(a.x, a.y, selectionRect.width / canvasView.zoom,
                                                   selectionRect.height / canvasView.zoom), rubberBase)
        }
    }

    onReleased: mode = CanvasBackground.Idle

    onDoubleClicked: (mouse) => {
        if (mouse.button !== Qt.LeftButton)
            return
        const world = canvasView.toWorld(Qt.point(mouse.x, mouse.y))
        const hit = canvasView.editor.connectionAt(world, canvasView.connectionTolerance)
        if (hit)
            canvasView.editLabel(hit)
        else
            canvasView.editText(canvasView.editor.createCard(world))
    }

    onWheel: (wheel) => {
        if (wheel.modifiers & Qt.ControlModifier) {
            canvasView.zoomAt(Qt.point(wheel.x, wheel.y), Math.pow(1.0015, wheel.angleDelta.y))
            return
        }
        const delta = canvasView.wheelDelta(wheel)
        let dx = delta.x
        let dy = delta.y
        if ((wheel.modifiers & Qt.ShiftModifier) && dx === 0) {
            dx = dy
            dy = 0
        }
        canvasView.panBy(dx, dy)
    }
}
