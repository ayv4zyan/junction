import QtQuick

// A round handle that keeps the same size on screen at every zoom level.
Rectangle {
    required property var canvas
    property point center
    property bool filled: true
    property real screenRadius: 6

    readonly property real r: screenRadius / canvas.zoom
    x: center.x - r
    y: center.y - r
    width: 2 * r
    height: 2 * r
    radius: r
    color: filled ? Theme.selection : Theme.background
    border.color: Theme.selection
    border.width: 1.5 / canvas.zoom
}
