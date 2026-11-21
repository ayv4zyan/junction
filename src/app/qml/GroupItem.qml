pragma ComponentBehavior: Bound
import QtQuick
import Junction

// One Group: a tinted area with a label above it. Drag the label to move the Group together
// with every Card inside it; double-click the label to rename it.
Item {
    id: group
    required property var canvas
    required property string nodeId
    required property real nodeX
    required property real nodeY
    required property real nodeWidth
    required property real nodeHeight
    required property string text
    required property string nodeColor

    readonly property Editor editor: canvas.editor
    readonly property bool selected: editor.selection.includes(nodeId)
    readonly property bool editing: canvas.editingId === nodeId
    readonly property color accent: Theme.resolve(nodeColor, Theme.cardBorder)

    x: nodeX
    y: nodeY
    width: nodeWidth
    height: nodeHeight

    function finishEditing() {
        editor.setText(nodeId, labelInput.text)
        if (canvas.editingId === nodeId)
            canvas.editingId = ""
    }

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: Qt.alpha(group.accent, 0.08)
        border.color: group.selected ? Theme.selection : group.accent
        border.width: group.selected ? 2 : 1.5
    }

    Rectangle {
        id: labelBar
        y: -height - 6
        height: 30
        width: Math.max(labelText.implicitWidth, labelInput.implicitWidth) + 20
        radius: 6
        color: group.selected ? Theme.selection : Qt.alpha(group.accent, 0.3)

        Text {
            id: labelText
            anchors.centerIn: parent
            visible: !group.editing
            text: group.text || qsTr("Group")
            color: group.selected ? "white" : Theme.text
            font.pixelSize: 15
            font.bold: true
        }

        TextInput {
            id: labelInput
            anchors.centerIn: parent
            visible: group.editing
            color: Theme.text
            font.pixelSize: 15
            font.bold: true
            onVisibleChanged: {
                if (visible) {
                    text = group.text
                    forceActiveFocus()
                    selectAll()
                }
            }
            onAccepted: group.canvas.forceActiveFocus()
            onActiveFocusChanged: if (!activeFocus && group.editing) group.finishEditing()
            Keys.onEscapePressed: group.canvas.forceActiveFocus()
            HoverHandler { cursorShape: Qt.IBeamCursor }   // a plain TextInput sets none
        }

        DragArea {
            canvas: group.canvas
            anchors.fill: parent
            visible: !group.editing   // hidden, not disabled, so the label's I-beam shows while editing
            property bool moving: false

            // Press: Shift/Ctrl toggles; otherwise an unselected item becomes the selection.
            // Drag: moves the whole selection. Click: narrows the selection to this item.
            onGrabbed: (mouse) => {
                if (group.canvas.isToggle(mouse.modifiers))
                    group.editor.select(group.nodeId, true)
                else if (!group.selected)
                    group.editor.select(group.nodeId)
            }
            onDragStarted: {
                moving = group.selected
                if (moving)
                    group.editor.beginMove(group.nodeId)
            }
            onDragged: (delta, invertSnap) => { if (moving) group.editor.updateMove(delta, invertSnap) }
            onDragFinished: {
                if (moving)
                    group.editor.endMove()
                moving = false
            }
            onTapped: (modifiers) => {
                if (!group.canvas.isToggle(modifiers))
                    group.editor.select(group.nodeId)
            }
            onDoubleClicked: group.canvas.editingId = group.nodeId
        }
    }

    ResizeEdges {
        canvas: group.canvas
        nodeId: group.nodeId
    }
}
