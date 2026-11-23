import QtQuick
import QtQuick.Templates as T

// A thin square bar that shows while scrolling (or hovered) and fades away after.
T.ScrollBar {
	id: control

	implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
							implicitContentWidth + leftPadding + rightPadding)
	implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
							 implicitContentHeight + topPadding + bottomPadding)

	padding: 2
	visible: control.policy !== T.ScrollBar.AlwaysOff
	minimumSize: orientation === Qt.Horizontal ? height / width : width / height

	contentItem: Rectangle {
		implicitWidth: control.interactive ? 6 : 2
		implicitHeight: control.interactive ? 6 : 2
		radius: Sizes.radius
		color: Qt.alpha(Colors.text, control.pressed ? 0.5 : control.hovered ? 0.35 : 0.22)
		opacity: 0

		states: State {
			name: "active"
			when: control.policy === T.ScrollBar.AlwaysOn || (control.active && control.size < 1.0)
			PropertyChanges { control.contentItem.opacity: 1 }
		}
		transitions: Transition {
			from: "active"
			SequentialAnimation {
				PauseAnimation { duration: 450 }
				NumberAnimation { target: control.contentItem; property: "opacity"; to: 0; duration: Motion.slow; easing.type: Motion.exit }
			}
		}
	}
}
