pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction
import JunctionDesign

// Edits a Connection's label in place. It sits in canvas (world) coordinates, so it zooms
// with the canvas, and looks like the painted label it stands in for (CanvasPainter's font and
// padding); ConnectionLayer leaves that label out meanwhile (hiddenLabel).
// Opened by CanvasView.editLabel(). Enter, Esc or clicking away keeps the text.
Rectangle {
    id: box
    required property var canvas
    readonly property Editor editor: canvas.editor
    readonly property string connectionId: canvas.labelingId
    // The label stays where the Connection's middle is, even when its cards move (undo).
    readonly property point center: editor.selectedConnection.labelPosition ?? Qt.point(0, 0)

    visible: connectionId !== ""
    // The painted Label's padding (DesignValues::LabelPadding*, copies of these tokens).
    width: Math.max(input.contentWidth, placeholder.implicitWidth) + 2 * Space.sm
    height: input.contentHeight + 2 * Space.xs
    x: center.x - width / 2
    y: center.y - height / 2
    radius: Sizes.radius
    color: Colors.background
    border.color: Colors.selection
    border.width: 1.5 / canvas.zoom   // as thin as the Dots at every zoom level

    onVisibleChanged: {
        if (visible) {
            input.text = editor.selectedConnection.label
            input.forceActiveFocus()
            input.selectAll()
        }
    }

    function finish() {
        const id = connectionId
        canvas.labelingId = ""
        editor.setConnectionLabel(id, input.text)
    }

    // Selecting something else, or the Connection going away (undo), ends editing, also while
    // the text menu is open (then losing focus doesn't end it, see the TextInput).
    Connections {
        target: box.editor
        function onSelectionChanged() {
            if (box.visible && box.editor.selectedConnection.id !== box.connectionId) {
                textMenu.close()
                box.finish()
                box.canvas.forceActiveFocus()
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
        onAccepted: box.canvas.forceActiveFocus()
        onActiveFocusChanged: if (!activeFocus && box.visible && !textMenu.visible) box.finish()
        Keys.onEscapePressed: box.canvas.forceActiveFocus()
        HoverHandler { cursorShape: Qt.IBeamCursor }   // a plain TextInput sets none
    }

    // A right-click opens the text menu. Above the TextInput, which would take the
    // right-click itself; left clicks still go through to it.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onPressed: textMenu.popup()
    }

    TextMenu {
        id: textMenu
        editor: input
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
