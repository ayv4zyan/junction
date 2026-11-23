import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A menu item that shows its keyboard shortcut on the right, greyed. `destructive` makes it red
// (Delete). The shortcut itself is defined elsewhere (a Shortcut); this only displays it.
MenuItem {
	id: entry
	property string hint
	property bool destructive: false

	contentItem: RowLayout {
		spacing: Space.xl
		Label {
			text: entry.text
			color: entry.highlighted ? Colors.onAccent : entry.destructive ? Colors.danger : Colors.text
			elide: Text.ElideRight
			Layout.fillWidth: true
		}
		Label {
			visible: entry.hint !== ""
			text: entry.hint
			color: entry.highlighted ? Colors.onAccent : Colors.mutedText
		}
	}

	background: Rectangle {
		implicitWidth: 200
		implicitHeight: Sizes.control
		color: !entry.highlighted ? "transparent" : entry.destructive ? Colors.dangerFill : Colors.accentFill
	}
}
