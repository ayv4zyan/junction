#pragma once

namespace JunctionDesign {

// Registers the bundled Geist font and makes it the application font, so QML text and
// everything QPainter draws (the canvas layers, exported images) use the same typeface on
// every machine. Call once, before loading QML.
void installFonts();

} // namespace JunctionDesign
