pragma Singleton
import QtQuick

// The typeface and its sizes. Geist is bundled and made the application font at startup
// (JunctionDesign::installFonts), so text looks the same on every machine and in exports.
QtObject {
	readonly property string family: "Geist"
	readonly property int small: 11      // counts, hints
	readonly property int body: 13       // all UI text
	readonly property int title: 15      // dialog titles, messages on an empty canvas
	readonly property int canvas: 15     // Card text and Group labels: the diagram, not the UI
}
