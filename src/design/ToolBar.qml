import QtQuick
import QtQuick.Templates as T

// The 40px strip above the canvas, with a line under it.
T.ToolBar {
	id: control

	implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
							implicitContentWidth + leftPadding + rightPadding)
	implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
							 implicitContentHeight + topPadding + bottomPadding)

	horizontalPadding: Space.sm

	background: Rectangle {
		implicitHeight: Sizes.toolbar
		color: Colors.background
		Rectangle {
			anchors.bottom: parent.bottom
			width: parent.width
			height: 1
			color: Colors.divider
		}
	}
}
