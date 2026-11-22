import QtQuick
import QtQuick.Templates as T

// The window: background, font, and a palette from Colors for anything this style doesn't
// draw itself (Fusion's fallback controls, file dialogs).
T.ApplicationWindow {
    color: Colors.background
    font.family: Fonts.family
    font.pixelSize: Fonts.body

    palette {
        window: Colors.background
        windowText: Colors.text
        base: Colors.cardFill
        alternateBase: Colors.panel
        text: Colors.text
        button: Colors.panel
        buttonText: Colors.text
        brightText: Colors.onAccent
        placeholderText: Colors.mutedText
        highlight: Colors.accentFill       // highlighted Buttons are filled with it
        highlightedText: Colors.onAccent
        link: Colors.accentText
        toolTipBase: Colors.panel
        toolTipText: Colors.text
        light: Colors.panel
        midlight: Colors.panel
        mid: Colors.cardBorder
        dark: Colors.divider
        shadow: Colors.shadow

        disabled.windowText: Colors.mutedText
        disabled.text: Colors.mutedText
        disabled.buttonText: Colors.mutedText
    }
}
