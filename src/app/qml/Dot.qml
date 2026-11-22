import QtQuick
import JunctionDesign

// A round dot that keeps the same size on screen at every zoom level: a Handle, a Waypoint or a
// Connection end. Round on purpose in a square UI: it marks a point, not a surface.
Rectangle {
    required property CanvasView canvasView
    property point center
    property bool filled: true
    property real screenRadius: 6

    readonly property real r: screenRadius / canvasView.zoom
    x: center.x - r
    y: center.y - r
    width: 2 * r
    height: 2 * r
    radius: r
    color: filled ? Colors.selection : Colors.background
    border.color: Colors.selection
    border.width: 1.5 / canvasView.zoom
}
