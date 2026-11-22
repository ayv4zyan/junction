pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction
import JunctionDesign

// Edits a Connection's label in place. It sits in canvas (world) coordinates, so it zooms
// with the canvas, and looks like the painted label it stands in for (CanvasPainter's font and
// padding); ConnectionLayer leaves that label out meanwhile (editedLabelConnection).
// Opened by CanvasView.editLabel(). Enter, Esc or clicking away keeps the text.
Rectangle {
    id: box
    // The CanvasView this is part of. Typed Item: Qt 6.9 rejects a type naming the view that creates it.
    required property Item canvasView
    required property Editor editor
    readonly property string connectionId: canvasView.editingLabelId
    // The label stays where the Connection's middle is, even when its cards move (undo).
    readonly property point center: editor.selection.connection?.labelPosition ?? Qt.point(0, 0)

    visible: connectionId !== ""
    // The painted Label's padding (DesignValues::LabelPadding*, copies of these tokens).
    width: Math.max(input.contentWidth, placeholder.implicitWidth) + 2 * Space.sm
    height: input.contentHeight + 2 * Space.xs
    x: center.x - width / 2
    y: center.y - height / 2
    radius: Sizes.radius
    color: Colors.background
    border.color: Colors.selection
    border.width: 1.5 / canvasView.zoom   // as thin as the Dots at every zoom level

    function finish(text) {
        const id = connectionId
        canvasView.endLabelEditing()
        editor.setLabels([id], text)
    }

    // Selecting something else, or the Connection going away (undo), ends editing, also while
    // the text menu is open (then losing focus doesn't end it, see the TextInput).
    Connections {
        target: box.editor.selection
        function onIdsChanged() {
            if (box.visible && box.editor.selection.connection?.id !== box.connectionId) {
                textSession.closeMenu()
                box.finish(input.text)
                box.canvasView.forceActiveFocus()
            }
        }
    }

    // A click in the padding around the text keeps editing (it would otherwise reach the
    // Connection's handles underneath). Under the TextInput, which takes clicks on the text.
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.IBeamCursor
        onPressed: input.forceActiveFocus()
    }

    TextInput {
        id: input
        anchors.centerIn: parent
        font.pixelSize: Fonts.body
        renderType: Text.CurveRendering   // sharp at every zoom level, like the painted label
        color: Colors.text
        selectionColor: Colors.accentFill
        selectedTextColor: Colors.onAccent
        onAccepted: box.canvasView.forceActiveFocus()
        Keys.onEscapePressed: box.canvasView.forceActiveFocus()
        HoverHandler { cursorShape: Qt.IBeamCursor }   // a plain TextInput sets none
    }

    // Above the TextInput, which would take the right-click for the text menu itself.
    TextEditSession {
        id: textSession
        anchors.fill: parent
        field: input
        startText: box.editor.selection.connection?.label ?? ""
        onFinished: (text) => box.finish(text)
    }

    Text {
        id: placeholder
        anchors.centerIn: parent
        visible: input.text === ""
        text: qsTr("Label")
        color: Colors.mutedText
        font.pixelSize: Fonts.body
        renderType: Text.CurveRendering
    }
}
