import QtQuick
import QtQuick.Effects

// The surface of everything that floats above the window: menus, tooltips, dialogs, toasts, the
// selection toolbar. A border plus one crisp shadow; in the dark theme it is also lighter than
// the canvas, since closer reads as lighter there. Nothing else in the app has a shadow, so a
// shadow always means "on top, and temporary".
Rectangle {
	color: Colors.panel
	border.color: Colors.cardBorder
	radius: Sizes.radius

	RectangularShadow {
		anchors.fill: parent
		z: -1                 // under the panel
		offset.y: Space.xs
		blur: Space.md
		radius: parent.radius
		color: Colors.shadow
	}
}
