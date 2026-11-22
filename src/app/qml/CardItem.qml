pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction
import JunctionDesign
import "CanvasColors.js" as CanvasColors

// One Card: Markdown text, Handles on hover, drag to move, double-click to edit.
Item {
    id: card
    required property CanvasView canvas
    required property string nodeId
    required property string kind
    required property real nodeX
    required property real nodeY
    required property real nodeWidth
    required property real nodeHeight
    required property string text
    required property string markdown   // the text as displayed (see Node::markdown)
    required property string nodeColor

    readonly property Editor editor: canvas.editor
    readonly property bool selected: editor.selection.includes(nodeId)
    readonly property bool editing: canvas.editingId === nodeId
    readonly property color accent: CanvasColors.resolve(nodeColor, Colors.presets, Colors.cardBorder)
    property int hoveredHandles: 0
    readonly property bool showHandles: !editing && (body.containsMouse || resizeEdges.hovered || hoveredHandles > 0
                                                     || canvas.connectFrom === nodeId
                                                     || canvas.dropTarget === nodeId)

    x: nodeX
    y: nodeY
    width: nodeWidth
    height: nodeHeight

    function finishEditing() {
        editor.setText(nodeId, textEdit.text)
        canvas.endTextEditing(nodeId)
    }

    Rectangle {
        anchors.fill: parent
        radius: Sizes.radius
        color: card.nodeColor ? Qt.tint(Colors.cardFill, Qt.alpha(card.accent, Colors.cardTint)) : Colors.cardFill
        border.color: card.selected ? Colors.selection : card.accent
        border.width: card.selected ? 2 : 1.5
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
                textMenu.popup()
        }
    }

    TextMenu {
        id: textMenu
        editor: textEdit
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
            text: card.kind === "text" ? card.markdown : card.text
            textFormat: card.kind === "text" ? Text.MarkdownText : Text.PlainText
            color: card.kind === "text" ? Colors.text : Colors.mutedText
            horizontalAlignment: card.kind === "text" ? Text.AlignLeft : Text.AlignHCenter
            verticalAlignment: card.kind === "text" ? Text.AlignTop : Text.AlignVCenter
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
            // Shown when editing starts; only a visible item can take keyboard focus.
            onVisibleChanged: {
                if (visible) {
                    text = card.text
                    forceActiveFocus()
                    cursorPosition = length
                }
            }
            onCursorRectangleChanged: {
                if (cursorRectangle.y < scroller.contentY)
                    scroller.contentY = cursorRectangle.y
                else if (cursorRectangle.y + cursorRectangle.height > scroller.contentY + scroller.height)
                    scroller.contentY = cursorRectangle.y + cursorRectangle.height - scroller.height
            }
            onActiveFocusChanged: if (!activeFocus && card.editing && !textMenu.visible) card.finishEditing()
            Keys.onEscapePressed: card.canvas.forceActiveFocus()
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
        onWheel: (event) => scroller.scrollBy(-card.canvas.wheelDelta(event).y / card.canvas.zoom)   // the card is scaled by the zoom
    }

    SelectMoveArea {
        id: body
        canvas: card.canvas
        nodeId: card.nodeId
        anchors.fill: parent
        visible: !card.editing   // hidden, not disabled, so the text's I-beam shows while editing
        hoverEnabled: true
        onDoubleClicked: { if (card.kind === "text") card.canvas.editText(card.nodeId) }
    }

    // Resizing by dragging a side or corner. Declared before the Handles so that a Handle in
    // the middle of a side wins over the side's resize strip.
    ResizeEdges {
        id: resizeEdges
        canvas: card.canvas
        nodeId: card.nodeId
    }

    // Handles: drag from one to create a Connection.
    Repeater {
        model: ["top", "right", "bottom", "left"]
        delegate: Dot {
            id: handle
            required property string modelData
            canvas: card.canvas
            visible: card.showHandles
            filled: handleMouse.containsMouse
            center: Qt.point(modelData === "left" ? 0 : modelData === "right" ? card.width : card.width / 2,
                             modelData === "top" ? 0 : modelData === "bottom" ? card.height : card.height / 2)

            MouseArea {
                id: handleMouse
                anchors.fill: parent
                anchors.margins: card ? -4 / card.canvas.zoom : 0   // card is null while being destroyed
                hoverEnabled: true
                cursorShape: Qt.CrossCursor
                onContainsMouseChanged: card.hoveredHandles += containsMouse ? 1 : -1
                onPressed: (mouse) => {
                    if (card.canvas.spaceHeld) {   // Space+drag pans
                        mouse.accepted = false
                        return
                    }
                    card.canvas.startConnection(card.nodeId, mapToItem(card.canvas, mouse.x, mouse.y))
                }
                onPositionChanged: (mouse) => { if (pressed) card.canvas.moveConnection(mapToItem(card.canvas, mouse.x, mouse.y)) }
                onReleased: (mouse) => card.canvas.finishConnection(mapToItem(card.canvas, mouse.x, mouse.y))
                onCanceled: card.canvas.cancelConnection()
            }
        }
    }
}
