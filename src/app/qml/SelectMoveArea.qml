import QtQuick
import Junction

// The part of a Card or Group that selects and moves it.
// Press: Shift/Ctrl toggles; otherwise an unselected item becomes the selection.
// Drag: moves the whole selection. Click: narrows the selection to this item.
// Right-click: the selection's menu. Double-click is left to the item.
DragArea {
    id: area
    required property string nodeId
    readonly property Editor editor: canvas.editor
    readonly property bool selected: editor.selection.includes(nodeId)
    property bool moving: false

    onGrabbed: (mouse) => {
        if (area.canvas.isToggle(mouse.modifiers))
            editor.select(nodeId, true)
        else if (!selected)
            editor.select(nodeId)
    }
    onDragStarted: {
        moving = selected
        if (moving)
            editor.beginMove(nodeId)
    }
    onDragged: (delta, invertSnap) => { if (moving) editor.updateMove(delta, invertSnap) }
    onDragFinished: {
        if (moving)
            editor.endMove()
        moving = false
    }
    onTapped: (modifiers) => {
        if (!area.canvas.isToggle(modifiers))
            editor.select(nodeId)
    }
    onMenuRequested: area.canvas.openMenuFor(nodeId)
}
