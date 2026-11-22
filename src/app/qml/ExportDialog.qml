import QtCore
import QtQuick
import QtQuick.Dialogs
import Junction
import JunctionDesign

// Asks where to export the open Canvas (or the selection) to: a copy of its .canvas file, or a
// PNG or SVG image. Says `exported` with the file written; the Editor reports failures.
FileDialog {
    id: dialog
    required property Editor editor
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
        const written = defaultSuffix === "canvas" ? editor.exportCanvasFile(selectedFile)
                                                   : editor.exportImage(selectedFile, Colors.forPainter)
        if (written.toString() !== "")
            exported(written)
    }
}
