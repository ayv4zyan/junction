pragma Singleton
import QtQuick

// Every color in the app. Follows the system light/dark setting (dark when unknown).
QtObject {
    readonly property bool dark: Qt.styleHints.colorScheme !== Qt.ColorScheme.Light

    readonly property color background: dark ? "#1e1e1e" : "#fafafa"
    readonly property color grid: dark ? "#3a3a3a" : "#d4d4d4"
    readonly property color panel: dark ? "#2a2a2a" : "#ffffff"
    readonly property color cardFill: dark ? "#262626" : "#ffffff"
    readonly property color cardBorder: dark ? "#4d4d4d" : "#c4c4c4"
    readonly property color text: dark ? "#dcddde" : "#222222"
    readonly property color mutedText: dark ? "#8a8a8a" : "#777777"
    readonly property color line: dark ? "#8a8a8a" : "#8c8c8c"
    readonly property color selection: "#7f6df2"

    // JSON Canvas colors "1".."6": red, orange, yellow, green, cyan, purple.
    readonly property var presets: dark
        ? ["#fb464c", "#e9973f", "#e0de71", "#44cf6e", "#53dfdd", "#a882ff"]
        : ["#e93147", "#ec7500", "#c9a400", "#08b94e", "#00a8a5", "#7852ee"]

    function resolve(color, fallback) {
        if (!color)
            return fallback
        if (color.startsWith("#"))
            return color
        return presets[parseInt(color) - 1] ?? fallback
    }

    // Handed to C++ for the painted layers and for export.
    readonly property var palette: ({
        background: background, grid: grid, cardFill: cardFill, cardBorder: cardBorder,
        text: text, mutedText: mutedText, line: line, selection: selection, presets: presets
    })
}
