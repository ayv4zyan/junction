pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction
import JunctionDesign
import "CanvasColors.js" as CanvasColors

// One Group: a tinted area with a label above it. Drag the label to move the Group together
// with every Card inside it; double-click the label to rename it.
Item {
    id: group
    required property CanvasView canvasView
    required property string nodeId
    required property real nodeX
    required property real nodeY
    required property real nodeWidth
    required property real nodeHeight
    required property string nodeLabel
    required property string nodeColor

    readonly property Editor editor: canvasView.editor
    readonly property bool selected: editor.selection.includes(nodeId)
    readonly property bool editing: canvasView.editingId === nodeId
    readonly property color accent: CanvasColors.resolve(nodeColor, Colors.presets, Colors.cardBorder)

    x: nodeX
    y: nodeY
    width: nodeWidth
    height: nodeHeight

    function finishEditing(text) {
        editor.setLabel(nodeId, text)
        canvasView.endTextEditing(nodeId)
    }

    Rectangle {
        anchors.fill: parent
        radius: Sizes.radius
        color: Qt.alpha(group.accent, Colors.groupTint)
        border.color: group.selected ? Colors.selection : group.accent
        border.width: group.selected ? 2 : 1.5
    }

    Rectangle {
        id: labelBar
        y: -height - Space.xs
        height: Sizes.control
        width: Math.max(labelText.implicitWidth, labelInput.implicitWidth) + 2 * Space.md
        radius: Sizes.radius
        color: group.selected ? Colors.accentFill : Qt.alpha(group.accent, 0.3)

        Text {
            id: labelText
            anchors.centerIn: parent
            visible: !group.editing
            text: group.nodeLabel || qsTr("Group")
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
            nodeId: group.nodeId
            anchors.fill: parent
            visible: !group.editing   // hidden, not disabled, so the label's I-beam shows while editing
            onDoubleClicked: group.canvasView.editText(group.nodeId)
        }
    }

    ResizeEdges {
        canvasView: group.canvasView
        nodeId: group.nodeId
    }
}
