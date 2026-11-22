pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction
import JunctionDesign
import "CanvasColors.js" as CanvasColors
import "Placement.js" as Placement

// The floating toolbar for the Selection: colors for anything, plus Path Style, arrowheads and
// Label when a single Connection is selected. It sits just above the selection (below it near
// the top edge, always inside the canvas), so the controls appear next to what they change.
// It hides while anything is dragged, the canvas is panned, a selection rectangle is drawn or a
// new Connection is pulled out, so it never chases the pointer.
RisingFloating {
    id: bar
    required property Editor editor
    required property CanvasView canvasView
    readonly property var connection: editor.selectedConnection
    readonly property bool isConnection: connection.valid
    shown: editor.selection.length > 0 && !editor.dragging && !canvasView.manipulating

    // The selection on screen, in the canvas's coordinates.
    readonly property rect target: {
        const b = editor.selectionBounds
        const topLeft = canvasView.toScreen(Qt.point(b.x, b.y))
        return Qt.rect(topLeft.x, topLeft.y, b.width * canvasView.zoom, b.height * canvasView.zoom)
    }
    // Where it sits, worked out only while it is shown: when it fades out (nothing selected any
    // more, or a drag starts) it stays where it was, instead of jumping to wherever an empty
    // selection would put it.
    property point position
    Binding on position {
        when: bar.shown
        value: Placement.floatingPosition(bar.target, Qt.size(bar.width, bar.height),
                                          Qt.rect(0, 0, bar.canvasView.width, bar.canvasView.height), Space.sm)
        restoreMode: Binding.RestoreNone
    }

    // Follows the selection 1:1 (also while scrolling the canvas): never animated.
    x: canvasView.x + position.x
    y: canvasView.y + position.y
    width: row.implicitWidth + 2 * Space.xs
    height: Sizes.toolbar

    rise: Space.xs   // it fades in, rising a few pixels, when something gets selected

    Row {
        id: row
        anchors.centerIn: parent

        Repeater {
            model: ["", "1", "2", "3", "4", "5", "6"]
            delegate: Swatch {
                required property string modelData
                anchors.verticalCenter: parent.verticalCenter
                fill: modelData ? CanvasColors.resolve(modelData, Colors.presets, Colors.line) : Colors.cardFill
                outlined: modelData === ""
                chosen: bar.editor.selectionColor === modelData
                tip: [qsTr("Default color"), qsTr("Red"), qsTr("Orange"), qsTr("Yellow"),
                      qsTr("Green"), qsTr("Cyan"), qsTr("Purple")][modelData ? parseInt(modelData) : 0]
                onClicked: bar.editor.setColor(modelData)
            }
        }

        Separator {
            visible: bar.isConnection
            anchors.verticalCenter: parent.verticalCenter
        }

        // Path style: one icon button per style; the current one is highlighted.
        Repeater {
            model: [{ style: CanvasEnums.PathStyle.Straight, icon: "style-straight", tip: qsTr("Straight") },
                    { style: CanvasEnums.PathStyle.Curved, icon: "style-curved", tip: qsTr("Curved") },
                    { style: CanvasEnums.PathStyle.Elbow, icon: "style-elbow", tip: qsTr("Elbow") }]
            delegate: IconButton {
                anchors.verticalCenter: parent?.verticalCenter
                required property var modelData
                visible: bar.isConnection
                iconName: modelData.icon
                tip: modelData.tip
                checked: bar.connection.style === modelData.style
                onClicked: bar.editor.setConnectionStyle(modelData.style)
            }
        }

        Separator {
            visible: bar.isConnection
            anchors.verticalCenter: parent.verticalCenter
        }

        // Arrowheads
        Repeater {
            model: [{ icon: "arrows-none", fromArrow: false, toArrow: false, tip: qsTr("No arrows") },
                    { icon: "arrows-end", fromArrow: false, toArrow: true, tip: qsTr("Arrow at end") },
                    { icon: "arrows-start", fromArrow: true, toArrow: false, tip: qsTr("Arrow at start") },
                    { icon: "arrows-both", fromArrow: true, toArrow: true, tip: qsTr("Arrows at both ends") }]
            delegate: IconButton {
                anchors.verticalCenter: parent?.verticalCenter
                required property var modelData
                visible: bar.isConnection
                iconName: modelData.icon
                tip: modelData.tip
                checked: bar.connection.fromArrow === modelData.fromArrow && bar.connection.toArrow === modelData.toArrow
                onClicked: bar.editor.setArrows(modelData.fromArrow, modelData.toArrow)
            }
        }

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            visible: bar.isConnection
            iconName: "label"
            tip: qsTr("Label")
            onClicked: bar.canvasView.editLabel(bar.connection.id)
        }
    }
}
