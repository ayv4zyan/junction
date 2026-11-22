pragma Singleton
import QtQuick

// Every color in the app, for the light and the dark theme (dark when the system doesn't say).
// Text colors reach WCAG AA (4.5:1) on the surfaces they are used on; colored lines and
// outlines reach 3:1.
QtObject {
    // Follows the system setting. Not readonly: the gallery flips it to show both themes.
    property bool dark: Qt.styleHints.colorScheme !== Qt.ColorScheme.Light

    // ---- Surfaces
    readonly property color background: dark ? "#1e1e1e" : "#fafafa"   // the canvas and the toolbar
    readonly property color sidebar: dark ? "#181818" : "#f0f0f0"
    readonly property color panel: dark ? "#333333" : "#ffffff"        // everything floating (see Floating)
    readonly property color cardFill: dark ? "#262626" : "#ffffff"     // Cards and text fields

    // ---- Lines
    readonly property color grid: dark ? "#3a3a3a" : "#d4d4d4"
    readonly property color divider: dark ? "#333333" : "#dddddd"      // between areas of the window
    readonly property color cardBorder: dark ? "#4d4d4d" : "#c4c4c4"   // outlines: Cards, panels, fields, separators
    readonly property color line: dark ? "#8a8a8a" : "#8c8c8c"         // Connections

    // ---- Text
    readonly property color text: dark ? "#dcddde" : "#222222"
    readonly property color mutedText: dark ? "#a0a0a0" : "#666666"

    // ---- Accent: one purple, in three strengths
    readonly property color selection: "#7f6df2"                       // outlines, Handles, focus rings
    readonly property color accentFill: "#6a5ae0"                      // behind onAccent text
    readonly property color accentText: dark ? "#a395ff" : "#5b4bd6"   // purple text
    readonly property color onAccent: "#ffffff"

    // ---- Danger: only for what destroys something, and for errors
    readonly property color danger: dark ? "#ff7070" : "#c8283c"       // red text and outlines
    readonly property color dangerFill: "#d92d42"                      // behind onAccent text

    // ---- States
    readonly property color hover: Qt.alpha(text, 0.07)
    readonly property color pressed: Qt.alpha(text, 0.14)
    readonly property color current: Qt.alpha(selection, dark ? 0.22 : 0.12)   // the open Canvas, a checked tool

    // ---- Depth: one level, "floating"
    readonly property color shadow: Qt.alpha("black", dark ? 0.8 : 0.16)
    readonly property color scrim: Qt.alpha("black", dark ? 0.5 : 0.3)      // behind a modal dialog

    // The six colors a user can give things: red, orange, yellow, green, cyan, purple.
    readonly property var presets: dark
        ? ["#fb464c", "#e9973f", "#e0de71", "#44cf6e", "#53dfdd", "#a882ff"]
        : ["#e93147", "#ec7500", "#c9a400", "#08b94e", "#00a8a5", "#7852ee"]

    readonly property real disabledOpacity: 0.4   // a control that can't be used right now
    readonly property real groupTint: 0.08        // how strongly a Group tints the Canvas under it
    readonly property real cardTint: 0.12         // how strongly its color tints a Card
    readonly property real groupLabelTint: 0.3    // how strongly its color tints a Group's label bar

    // Handed to C++ for the painted layers and for export (see Palette in CanvasPainter.h).
    readonly property var forPainter: ({
        background: background, grid: grid, cardFill: cardFill, cardBorder: cardBorder,
        text: text, mutedText: mutedText, line: line, selection: selection, presets: presets,
        radius: Sizes.radius
    })
}
