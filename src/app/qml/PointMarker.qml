import QtQuick
import JunctionDesign

// A round marker that keeps the same size on screen at every zoom level, for a point you can
// grab: a Handle, a Waypoint or a Connection end. Round on purpose in a square UI: it marks a
// point, not a surface.
Rectangle {
	// The CanvasView this is part of. Typed Item: Qt 6.9 rejects a type naming the view that creates it.
	required property Item canvasView
	property point center
	property bool filled: true
	property real screenRadius: 6

	readonly property real worldRadius: screenRadius / canvasView.zoom
	x: center.x - worldRadius
	y: center.y - worldRadius
	width: 2 * worldRadius
	height: 2 * worldRadius
	radius: worldRadius
	color: filled ? Colors.selection : Colors.background
	border.color: Colors.selection
	border.width: 1.5 / canvasView.zoom
}
