import QtQuick
import QtQuick.Templates as T

// A line between groups of rows in a Menu.
T.MenuSeparator {
	id: control

	implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
							implicitContentWidth + leftPadding + rightPadding)
	implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
							 implicitContentHeight + topPadding + bottomPadding)

	horizontalPadding: Space.md
	verticalPadding: Space.xs

	contentItem: Rectangle {
		implicitWidth: 176
		implicitHeight: 1
		color: Colors.cardBorder
	}
}
