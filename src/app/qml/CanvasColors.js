.pragma library

// A Card, Group or Connection color as JSON Canvas stores it: "" (the default), a preset number
// "1".."6" (red, orange, yellow, green, cyan, purple), or "#rrggbb". `presets` comes from
// Colors.presets, so the presets follow the light/dark theme.
function resolve(color, presets, fallback) {
    if (!color)
        return fallback
    if (color.startsWith("#"))
        return color
    return presets[parseInt(color) - 1] ?? fallback
}
