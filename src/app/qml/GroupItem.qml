pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction
import JunctionDesign

// One Group: a tinted area with a label above it. Drag the label to move the Group together
// with every Card inside it; double-click the label to rename it.
NodeItem {
    id: group
    required property string nodeLabel         // as typed, for editing
    required property string nodeDisplayText   // as shown (a default while the label is empty)

    function finishEditing(text) {
        editor.setLabels([nodeId], text)
        canvasView.endEditingInPlace(nodeId)
    }

    Rectangle {
        anchors.fill: parent
        radius: Sizes.radius
        color: Qt.alpha(group.accent, Colors.groupTint)
        border.color: group.borderColor
        border.width: group.borderWidth
    }

    Rectangle {
        id: labelBar
        y: -height - Space.xs
        height: Sizes.control
        width: Math.max(labelText.implicitWidth, labelInput.implicitWidth) + 2 * Space.md
        radius: Sizes.radius
        color: group.selected ? Colors.accentFill : Qt.alpha(group.accent, Colors.groupLabelTint)

        Text {
            id: labelText
            anchors.centerIn: parent
            visible: !group.editing
            text: group.nodeDisplayText
            color: group.selected ? Colors.onAccent : Colors.text
            font.pixelSize: Fonts.canvas
            renderType: Text.CurveRendering   // sharp at every zoom level, like Card text
            font.bold: true
        }

        TextInput {
            id: labelInput
            anchors.centerIn: parent
            visible: group.editing
            color: Colors.text
            font.pixelSize: Fonts.canvas
            renderType: Text.CurveRendering   // sharp at every zoom level, like Card text
            font.bold: true
            onAccepted: group.canvasView.forceActiveFocus()
            Keys.onEscapePressed: group.canvasView.forceActiveFocus()
            HoverHandler { cursorShape: Qt.IBeamCursor }   // a plain TextInput sets none
        }

        // Above the TextInput, which would take the right-click for the text menu itself.
        TextEditSession {
            anchors.fill: parent
            field: labelInput
            startText: group.nodeLabel
            onFinished: (text) => group.finishEditing(text)
        }

        SelectMoveArea {
            canvasView: group.canvasView
            editor: group.editor
            nodeId: group.nodeId
            anchors.fill: parent
            visible: !group.editing   // hidden, not disabled, so the label's I-beam shows while editing
            onDoubleClicked: group.canvasView.editInPlace(group.nodeId)
        }
    }

    ResizeEdges {
        canvasView: group.canvasView
        editor: group.editor
        nodeId: group.nodeId
    }
}
