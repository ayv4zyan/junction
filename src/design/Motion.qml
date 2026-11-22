pragma Singleton
import QtQuick

// Crisp motion: short, no overshoot. Things coming in ease out (fast start, gentle stop); things
// leaving ease in and take 2/3 of the time. Every animation needs a purpose (README.md), and
// anything the pointer moves directly (dragging, panning, zooming) is never animated.
QtObject {
    readonly property int fast: 100      // hover, press, small state changes
    readonly property int normal: 150    // fades: menus, tooltips, toasts, the selection toolbar
    readonly property int slow: 220      // bigger moves: the sidebar, dialogs, rows making room
    readonly property int enter: Easing.OutCubic
    readonly property int exit: Easing.InCubic
    readonly property real scaleFrom: 0.98   // what dialogs and menus grow from as they appear

    function out(duration) { return Math.round(duration * 2 / 3) }   // the exit for an entrance
}
