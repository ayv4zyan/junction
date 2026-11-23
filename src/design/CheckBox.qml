import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

// A square check box: filled with the accent and a check mark when checked.
T.CheckBox {
	id: control

	implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
							implicitContentWidth + leftPadding + rightPadding)
	implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
							 implicitContentHeight + topPadding + bottomPadding,
							 implicitIndicatorHeight + topPadding + bottomPadding)

	padding: Space.xs
	spacing: Space.sm
	opacity: enabled ? 1 : Colors.disabledOpacity

	indicator: Rectangle {
		implicitWidth: Sizes.icon
		implicitHeight: Sizes.icon
		x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
		y: control.topPadding + (control.availableHeight - height) / 2
		radius: Sizes.radius
		color: control.checked ? Colors.accentFill : Colors.cardFill
		border.color: control.checked ? Colors.accentFill : control.hovered ? Colors.mutedText : Colors.cardBorder
		Behavior on color { ColorAnimation { duration: Motion.fast } }

		IconImage {
			anchors.centerIn: parent
			visible: control.checked
			source: "icons/check.svg"
			sourceSize: Qt.size(Sizes.iconSmall, Sizes.iconSmall)
			color: Colors.onAccent
		}
		FocusRing { visible: control.visualFocus }
	}

	contentItem: CheckLabel {
		leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
		rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0
		text: control.text
		font: control.font
		color: Colors.text
	}
}
