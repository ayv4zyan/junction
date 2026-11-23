import QtQuick
import Junction

// Presses that hit nothing on the canvas: drag to pan (middle button, or Space held) or to
// select with a rectangle, click a Connection to select it, right-click for a menu,
// double-click to add a Card (or edit a Connection's label), and the wheel to scroll and zoom.
MouseArea {
	id: background
	// The CanvasView this is part of. Typed Item: Qt 6.9 rejects a type naming the view that creates it.
	required property Item canvasView
	required property Editor editor
	required property HitTest hitTest
	required property CanvasMenus menus

	enum Mode { Idle, Panning, Selecting }   // what the press started

	acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
	hoverEnabled: true
	cursorShape: mode === BackgroundInput.Panning ? Qt.ClosedHandCursor
			   : canvasView.spaceHeld ? Qt.OpenHandCursor
			   : overConnection ? Qt.PointingHandCursor : Qt.ArrowCursor
	property int mode: BackgroundInput.Idle
	property rect selectionRect   // the rectangle being dragged, in this item's coordinates
	property bool overConnection: false
	property point last
	property point selectionDragStart
	property var selectionBeforeDrag: []   // what stays selected besides the rectangle's catch (Shift/Ctrl)

	onPressed: (mouse) => {
		canvasView.forceActiveFocus()
		last = Qt.point(mouse.x, mouse.y)
		if (mouse.button === Qt.MiddleButton || canvasView.spaceHeld) {
			mode = BackgroundInput.Panning
			return
		}
		const hit = hitTest.connectionAt(canvasView.toWorld(last), canvasView.connectionTolerance)
		if (mouse.button === Qt.RightButton) {
			if (hit) {
				canvasView.openMenuFor(hit)
			} else {
				editor.selection.clear()
				background.menus.openAt(canvasView.toWorld(last))
			}
			return
		}
		const additive = canvasView.isToggle(mouse.modifiers)
		if (hit) {
			editor.selection.select(hit, additive)
			mode = BackgroundInput.Idle
			return
		}
		if (!additive)
			editor.selection.clear()
		mode = BackgroundInput.Selecting
		selectionRect = Qt.rect(last.x, last.y, 0, 0)
		selectionDragStart = last
		selectionBeforeDrag = editor.selection.ids
	}

	onPositionChanged: (mouse) => {
		if (!pressed) {
			overConnection = hitTest.connectionAt(canvasView.toWorld(Qt.point(mouse.x, mouse.y)), canvasView.connectionTolerance) !== ""
		} else if (mode === BackgroundInput.Panning) {
			canvasView.panBy(mouse.x - last.x, mouse.y - last.y)
			last = Qt.point(mouse.x, mouse.y)
		} else if (mode === BackgroundInput.Selecting) {
			selectionRect = Qt.rect(Math.min(selectionDragStart.x, mouse.x), Math.min(selectionDragStart.y, mouse.y),
									Math.abs(mouse.x - selectionDragStart.x), Math.abs(mouse.y - selectionDragStart.y))
			// The selection follows the rectangle as it is dragged, not only on release.
			const a = canvasView.toWorld(Qt.point(selectionRect.x, selectionRect.y))
			editor.selection.selectInRect(Qt.rect(a.x, a.y, selectionRect.width / canvasView.zoom,
												   selectionRect.height / canvasView.zoom), selectionBeforeDrag)
		}
	}

	onReleased: mode = BackgroundInput.Idle

	onDoubleClicked: (mouse) => {
		if (mouse.button !== Qt.LeftButton)
			return
		const world = canvasView.toWorld(Qt.point(mouse.x, mouse.y))
		const hit = hitTest.connectionAt(world, canvasView.connectionTolerance)
		if (hit)
			canvasView.editLabel(hit)
		else
			canvasView.editNew(editor.createCard(world))
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
