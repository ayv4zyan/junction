import QtQuick
import QtQuick.Templates as T

// A short hint above the hovered control, on the floating surface; it appears after Motion.tooltipDelay.
T.ToolTip {
	id: control

	x: parent ? (parent.width - implicitWidth) / 2 : 0
	y: -implicitHeight - Space.xs

	implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
							implicitContentWidth + leftPadding + rightPadding)
	implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
							 implicitContentHeight + topPadding + bottomPadding)

	margins: Space.sm
	horizontalPadding: Space.sm
	verticalPadding: Space.xs
	delay: Motion.tooltipDelay

	closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

	contentItem: Text {
		text: control.text
		font: control.font
		wrapMode: Text.Wrap
		color: Colors.text
	}

	background: Floating { }

	enter: Transition {
		NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast; easing.type: Motion.enter }
	}
	exit: Transition {
		NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.out(Motion.fast); easing.type: Motion.exit }
	}
}
