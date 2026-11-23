import QtQuick

// A thin line between groups of controls: vertical in a toolbar, horizontal elsewhere.
Rectangle {
	property bool vertical: true
	implicitWidth: vertical ? 1 : Sizes.control
	implicitHeight: vertical ? Space.lg + Space.xs : 1
	color: Colors.cardBorder
}
