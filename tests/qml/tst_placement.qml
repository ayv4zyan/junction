import QtQuick
import QtTest
import "../../src/app/qml/Placement.js" as Placement

// Where the floating selection toolbar goes (Placement.js): a 200x40 toolbar, 8 px from things,
// in an 800x600 view.
TestCase {
	name: "Placement"

	function test_floatingPosition_data() {
		return [
			{ tag: "centred above", selection: Qt.rect(300, 200, 200, 100), expected: Qt.point(300, 152) },
			{ tag: "kept off the left edge", selection: Qt.rect(0, 200, 100, 60), expected: Qt.point(8, 152) },
			{ tag: "kept off the right edge", selection: Qt.rect(750, 200, 50, 60), expected: Qt.point(592, 152) },
			{ tag: "below when no room above", selection: Qt.rect(300, 20, 200, 60), expected: Qt.point(300, 88) },
			{ tag: "at the top when the selection fills the view", selection: Qt.rect(0, 0, 800, 600), expected: Qt.point(300, 8) },
			{ tag: "inside when the selection is scrolled above the view", selection: Qt.rect(300, -200, 200, 100), expected: Qt.point(300, 8) },
			{ tag: "inside when the selection is scrolled below the view", selection: Qt.rect(300, 700, 200, 100), expected: Qt.point(300, 552) },
		]
	}

	function test_floatingPosition(data) {
		const position = Placement.floatingPosition(data.selection, Qt.size(200, 40), Qt.rect(0, 0, 800, 600), 8)
		compare(position.x, data.expected.x)
		compare(position.y, data.expected.y)
	}
}
