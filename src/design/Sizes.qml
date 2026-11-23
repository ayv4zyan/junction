pragma Singleton
import QtQuick

// Heights, icon sizes and corners.
QtObject {
	readonly property int control: 28    // buttons, rows, fields, menu items, swatch targets
	readonly property int toolbar: 40    // the toolbar, the selection toolbar, the sidebar's search strip
	readonly property int icon: 16       // every icon
	readonly property int iconSmall: 12  // small indicators: chevrons, check marks
	readonly property int radius: 0      // square, everywhere (README.md)
}
