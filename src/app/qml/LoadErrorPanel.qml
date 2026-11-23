import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import JunctionDesign

// Shown over a Canvas whose file can't be read: says why. That file is never saved, so it
// stays as it was.
Floating {
	id: panel
	required property string message   // the load error; the panel shows while there is one
	required property url folder       // the Library folder, to fix or delete the file there

	visible: message !== ""
	height: errorColumn.implicitHeight + 2 * Space.xl
	ColumnLayout {
		id: errorColumn
		anchors.fill: parent
		anchors.margins: Space.xl
		spacing: Space.sm
		Label {
			text: qsTr("This canvas can't be opened")
			font.pixelSize: Fonts.title
			font.weight: Font.DemiBold
		}
		Label {
			Layout.fillWidth: true
			text: panel.message
			wrapMode: Text.Wrap
			color: Colors.mutedText
		}
		Label {
			Layout.fillWidth: true
			text: qsTr("Junction won't change the file. Fix it in another editor, or delete the canvas from the sidebar.")
			wrapMode: Text.Wrap
			color: Colors.mutedText
		}
		Button {
			Layout.topMargin: Space.md
			text: qsTr("Open library folder")
			onClicked: Qt.openUrlExternally(panel.folder)
		}
	}
}
