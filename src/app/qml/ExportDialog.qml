import QtCore
import QtQuick
import QtQuick.Dialogs
import Junction
import JunctionDesign

// Asks where to export the open Canvas to: a copy of its .canvas file, or a PNG or SVG image of
// the selection (everything when nothing is selected). Says `exported` with the file written;
// the Exporter reports failures.
FileDialog {
	id: dialog
	required property Exporter exporter
	required property Editor editor   // for the selection, which is what an image shows
	required property string canvasName   // suggested as the file name

	signal exported(url file)

	function exportAs(suffix) {
		defaultSuffix = suffix
		nameFilters = suffix === "canvas" ? [qsTr("Canvas (*.canvas)")]
										  : [suffix.toUpperCase() + qsTr(" image (*.%1)").arg(suffix)]
		selectedFile = StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/"
					   + encodeURIComponent(canvasName) + "." + suffix
		open()
	}

	fileMode: FileDialog.SaveFile
	onAccepted: {
		const written = defaultSuffix === "canvas" ? exporter.exportCanvasFile(selectedFile)
												   : exporter.exportImage(selectedFile, Colors.forPainter, editor.selection.ids)
		if (written.toString() !== "")
			exported(written)
	}
}
