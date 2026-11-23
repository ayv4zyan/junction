import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

// One row of a Menu: accent fill when highlighted, a check when checked, a chevron for a submenu.
T.MenuItem {
	id: control

	implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
							implicitContentWidth + leftPadding + rightPadding)
	implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
							 implicitContentHeight + topPadding + bottomPadding,
							 implicitIndicatorHeight + topPadding + bottomPadding)

	padding: 0
	horizontalPadding: Space.md
	spacing: Space.sm
	icon.width: Sizes.icon
	icon.height: Sizes.icon
	icon.color: highlighted ? Colors.onAccent : Colors.text   // the default; an icon's own color wins
	opacity: enabled ? 1 : Colors.disabledOpacity

	contentItem: IconLabel {
		readonly property real arrowPadding: control.subMenu && control.arrow ? control.arrow.width + control.spacing : 0
		readonly property real indicatorPadding: control.checkable && control.indicator ? control.indicator.width + control.spacing : 0
		leftPadding: !control.mirrored ? indicatorPadding : arrowPadding
		rightPadding: control.mirrored ? indicatorPadding : arrowPadding
		spacing: control.spacing
		mirrored: control.mirrored
		display: control.display
		alignment: Qt.AlignLeft
		icon: control.icon
		text: control.text
		font: control.font
		color: control.highlighted ? Colors.onAccent : Colors.text
	}

	indicator: IconImage {
		x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
		y: control.topPadding + (control.availableHeight - height) / 2
		visible: control.checked
		source: control.checkable ? "icons/check.svg" : ""
		sourceSize: Qt.size(Sizes.iconSmall, Sizes.iconSmall)
		color: control.highlighted ? Colors.onAccent : Colors.text
	}

	arrow: IconImage {
		x: control.mirrored ? control.leftPadding : control.width - width - control.rightPadding
		y: control.topPadding + (control.availableHeight - height) / 2
		visible: control.subMenu
		mirror: control.mirrored
		source: control.subMenu ? "icons/chevron.svg" : ""
		sourceSize: Qt.size(Sizes.iconSmall, Sizes.iconSmall)
		color: control.highlighted ? Colors.onAccent : Colors.mutedText
	}

	background: Rectangle {
		implicitWidth: 200
		implicitHeight: Sizes.control
		radius: Sizes.radius
		color: control.highlighted ? Colors.accentFill : "transparent"
	}
}
