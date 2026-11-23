import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

// A dialog: title on top, content, then the footer (see AskDialog). It dims the window behind
// it and grows briefly into place, so it reads as on top of the work and temporary.
T.Dialog {
	id: control

	implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
							implicitContentWidth + leftPadding + rightPadding,
							implicitHeaderWidth,
							implicitFooterWidth)
	implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
							 implicitContentHeight + topPadding + bottomPadding
							 + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
							 + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

	padding: Space.xl
	topPadding: Space.sm     // the title above already leaves room

	background: Floating { }

	header: Label {
		text: control.title
		visible: control.title !== ""
		elide: Label.ElideRight
		font.pixelSize: Fonts.title
		font.weight: Font.DemiBold
		padding: Space.xl
		bottomPadding: 0
	}

	T.Overlay.modal: Rectangle {
		color: Colors.scrim
		Behavior on opacity { NumberAnimation { duration: Motion.slow } }
	}
	T.Overlay.modeless: Rectangle {
		color: "transparent"
	}

	enter: Transition {
		NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.slow; easing.type: Motion.enter }
		NumberAnimation { property: "scale"; from: Motion.scaleFrom; to: 1; duration: Motion.slow; easing.type: Motion.enter }
	}
	exit: Transition {
		NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.out(Motion.slow); easing.type: Motion.exit }
	}
}
