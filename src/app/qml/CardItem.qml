pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction
import JunctionDesign

// One Card: Markdown text, Handles on hover, drag to move, double-click to edit.
NodeItem {
    id: card
    required property int nodeKind   // a CanvasEnums.NodeKind
    required property string nodeText          // the Markdown, as typed
    required property string nodeDisplayText   // what the card shows (see NodeModel)

    readonly property bool isText: nodeKind === CanvasEnums.NodeKind.Text   // otherwise Unsupported: shown, never edited
    // Handles show while the pointer is on the card or close to it (on a Handle, or on the
    // outer half of a resize strip), and while a Connection is dragged from or onto it.
    readonly property bool showHandles: !editing && (hover.hovered || canvasView.connectFrom === nodeId
                                                     || canvasView.dropTarget === nodeId)

    function finishEditing(text) {
        editor.setCardText(nodeId, text)
        canvasView.endTextEditing(nodeId)
    }

    Rectangle {
        anchors.fill: parent
        radius: Sizes.radius
        color: card.nodeColor ? Qt.tint(Colors.cardFill, Qt.alpha(card.accent, Colors.cardTint)) : Colors.cardFill
        border.color: card.borderColor
        border.width: card.borderWidth
    }

    // While editing, a click in the card's padding keeps the cursor in the text, and a
    // right-click anywhere on the card opens the text menu. Hidden, not just disabled,
    // otherwise: Qt shows a disabled MouseArea's I-beam all the same.
    MouseArea {
        anchors.fill: parent
        visible: card.editing
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.IBeamCursor
        onPressed: (mouse) => {
            textEdit.forceActiveFocus()
            if (mouse.button === Qt.RightButton)
                textSession.openMenu()
        }
    }

    TextEditSession {
        id: textSession
        field: textEdit
        startText: card.nodeText
        selectAll: false
        onFinished: (text) => card.finishEditing(text)
    }

    // Cards have a fixed size, so text that doesn't fit scrolls: with the mouse wheel over the
    // card (like Obsidian), and by moving the cursor while editing.
    // The Flickable only clips and holds the scroll position; it isn't dragged (that would
    // fight with moving the card and selecting text).
    Flickable {
        id: scroller
        readonly property bool overflows: contentHeight > height + 1
        anchors.fill: parent
        anchors.margins: Space.md
        clip: true
        interactive: false
        contentWidth: width
        contentHeight: card.editing ? textEdit.height : textView.height

        function scrollBy(dy) {
            contentY = Math.max(0, Math.min(contentHeight - height, contentY + dy))
        }

        Text {
            id: textView
            width: scroller.width
            height: Math.max(implicitHeight, scroller.height)
            visible: !card.editing
            text: card.nodeDisplayText
            textFormat: card.isText ? Text.MarkdownText : Text.PlainText
            color: card.isText ? Colors.text : Colors.mutedText
            horizontalAlignment: card.isText ? Text.AlignLeft : Text.AlignHCenter
            verticalAlignment: card.isText ? Text.AlignTop : Text.AlignVCenter
            wrapMode: Text.Wrap
            font.pixelSize: Fonts.canvas
            // Cards are scaled with the canvas: drawn as curves, the text stays sharp at every zoom.
            renderType: Text.CurveRendering
        }

        TextEdit {
            id: textEdit
            width: scroller.width
            height: Math.max(implicitHeight, scroller.height)   // clicks below the last line still place the cursor
            visible: card.editing
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.PlainText
            color: Colors.text
            selectionColor: Colors.accentFill
            selectedTextColor: Colors.onAccent
            font.pixelSize: Fonts.canvas
            renderType: Text.CurveRendering
            onCursorRectangleChanged: {
                if (cursorRectangle.y < scroller.contentY)
                    scroller.contentY = cursorRectangle.y
                else if (cursorRectangle.y + cursorRectangle.height > scroller.contentY + scroller.height)
                    scroller.contentY = cursorRectangle.y + cursorRectangle.height - scroller.height
            }
            Keys.onEscapePressed: card.canvasView.forceActiveFocus()
        }

        // the text got shorter (edited, or switched back from editing): don't stay scrolled past it
        onContentHeightChanged: scrollBy(0)
    }

    // Scrollbar, shown only when the text doesn't fit.
    Rectangle {
        visible: scroller.overflows
        x: card.width - 7
        y: scroller.y + scroller.visibleArea.yPosition * scroller.height
        width: 3
        height: scroller.visibleArea.heightRatio * scroller.height
        color: Colors.mutedText
        opacity: 0.6
    }

    // Plain wheel scrolls the text; with Ctrl (zoom) or Shift (sideways) it goes to the canvas.
    WheelHandler {
        enabled: scroller.overflows
        acceptedModifiers: Qt.NoModifier
        // On Wayland, Qt reports mouse wheels as a "touchpad" device, so accept every device.
        acceptedDevices: PointerDevice.AllDevices
        target: null
        onWheel: (event) => scroller.scrollBy(-card.canvasView.wheelDelta(event).y / card.canvasView.zoom)   // the card is scaled by the zoom
    }

    HoverHandler {
        id: hover
        margin: card.canvasView.handleRadius
    }

    SelectMoveArea {
        id: body
        canvasView: card.canvasView
        nodeId: card.nodeId
        anchors.fill: parent
        visible: !card.editing   // hidden, not disabled, so the text's I-beam shows while editing
        hoverEnabled: true
        onDoubleClicked: { if (card.isText) card.canvasView.editText(card.nodeId) }
    }

    // Resizing by dragging a side or corner. Declared before the Handles so that a Handle in
    // the middle of a side wins over the side's resize strip.
    ResizeEdges {
        id: resizeEdges
        canvasView: card.canvasView
        nodeId: card.nodeId
    }

    // Handles: drag from one to create a Connection.
    Repeater {
        // Where each Handle sits, as a fraction of the card's size: top, right, bottom, left.
        model: [Qt.point(0.5, 0), Qt.point(1, 0.5), Qt.point(0.5, 1), Qt.point(0, 0.5)]
        delegate: Dot {
            id: handle
            required property point modelData
            canvasView: card.canvasView
            visible: card.showHandles
            filled: handleMouse.containsMouse
            center: Qt.point(modelData.x * card.width, modelData.y * card.height)

            MouseArea {
                id: handleMouse
                anchors.fill: parent
                anchors.margins: card ? -card.canvasView.dotMargin : 0   // card is null while being destroyed
                hoverEnabled: true
                cursorShape: Qt.CrossCursor
                onPressed: (mouse) => {
                    if (card.canvasView.spaceHeld) {   // Space+drag pans
                        mouse.accepted = false
                        return
                    }
                    card.canvasView.startConnection(card.nodeId, mapToItem(card.canvasView, mouse.x, mouse.y))
                }
                onPositionChanged: (mouse) => { if (pressed) card.canvasView.moveConnection(mapToItem(card.canvasView, mouse.x, mouse.y)) }
                onReleased: (mouse) => card.canvasView.finishConnection(mapToItem(card.canvasView, mouse.x, mouse.y))
                onCanceled: card.canvasView.cancelConnection()
            }
        }
    }
}
