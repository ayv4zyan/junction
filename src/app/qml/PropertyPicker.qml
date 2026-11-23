pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import JunctionDesign

// One property in the selection toolbar, shown compactly: its current value (an icon or a color)
// and a chevron. Clicking it opens a second bar with every choice, on the side away from the
// selection (`opensUp`); picking a choice (it calls close()), Esc, or a click anywhere else
// closes it again.
//
//   PropertyPicker {
//       iconName: "style-curved"; tip: qsTr("Path style")
//       IconButton { iconName: "style-elbow"; onClicked: { ...; picker.close() } }
//       ...
//   }
ToolButton {
	id: picker

	property string iconName         // the current value as an icon, or else
	property color swatch            // the current value as a color,
	property bool swatchOutlined     // with an outline when it is light (the default color)
	property string tip
	property bool opensUp: true
	default property alias choices: choiceRow.data

	function close() { choiceBar.close() }

	focusPolicy: Qt.NoFocus
	checked: choiceBar.visible       // tinted while its choices are open
	padding: (Sizes.control - Sizes.icon) / 2
	onClicked: choiceBar.visible ? choiceBar.close() : choiceBar.open()
	onVisibleChanged: if (!visible) close()   // the toolbar went away, or this property did

	contentItem: Row {
		spacing: Space.xs
		Item {
			width: Sizes.icon
			height: Sizes.icon
			anchors.verticalCenter: parent.verticalCenter
			IconImage {
				visible: picker.iconName !== ""
				anchors.fill: parent
				source: picker.iconName ? "../icons/" + picker.iconName + ".svg" : ""
				sourceSize: Qt.size(Sizes.icon, Sizes.icon)
				color: Colors.text
			}
			Rectangle {   // looks like a Swatch
				visible: picker.iconName === ""
				anchors.fill: parent
				radius: Sizes.radius
				color: picker.swatch
				border.color: picker.swatchOutlined ? Colors.cardBorder : Qt.darker(picker.swatch, 1.15)
			}
		}
		IconImage {
			anchors.verticalCenter: parent.verticalCenter
			source: "../icons/chevron.svg"
			sourceSize: Qt.size(Sizes.iconSmall, Sizes.iconSmall)
			color: Colors.mutedText
			rotation: picker.opensUp ? -90 : 90   // points to where the choices open
		}
	}

	ToolTip.visible: hovered && !choiceBar.visible && tip !== ""
	ToolTip.delay: Motion.tooltipDelay
	ToolTip.text: tip

	// The second bar: as tall as the toolbar, centred on this picker, a small gap outside the
	// toolbar's edge. CloseOnPressOutsideParent: a click on the picker itself is left to
	// onClicked, so it closes the bar instead of closing and reopening it.
	Popup {
		id: choiceBar
		readonly property real edge: (Sizes.toolbar - Sizes.control) / 2 + Space.xs
		property real offset: 0   // how far from its place it is while it appears

		closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
		focus: true               // so Esc reaches it; focus goes back to the canvas on close
		margins: Space.sm         // stays inside the window
		padding: 0
		x: (picker.width - width) / 2
		y: picker.opensUp ? -edge - height + offset : picker.height + edge - offset
		width: choiceRow.implicitWidth + 2 * Space.xs
		height: Sizes.toolbar

		background: Floating {}
		contentItem: Item {
			Row {
				id: choiceRow
				anchors.centerIn: parent
			}
		}

		// It rises out of the toolbar, like the toolbar does out of the selection.
		enter: Transition {
			NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Motion.enter }
			NumberAnimation { property: "offset"; from: Space.xs; to: 0; duration: Motion.normal; easing.type: Motion.enter }
		}
		exit: Transition {
			NumberAnimation { property: "opacity"; to: 0; duration: Motion.out(Motion.normal); easing.type: Motion.exit }
		}
	}
}
