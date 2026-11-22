pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction
import JunctionDesign
import "CanvasColors.js" as CanvasColors

// The floating toolbar for the Selection: colors for anything, plus Path Style, arrowheads and
// Label when a single Connection is selected. It sits just above the selection (below it near
// the top edge, always inside the canvas), so the controls appear next to what they change.
// It hides while anything is dragged, the canvas is panned, a selection rectangle is drawn or a
// new Connection is pulled out, so it never chases the pointer.
Floating {
    id: bar
    required property Editor editor
    required property CanvasView canvas
    readonly property var connection: editor.selectedConnection
    readonly property bool isConnection: connection.id !== undefined
    readonly property bool shown: editor.selection.length > 0 && !editor.dragging && !canvas.manipulating

    // The selection on screen, in the canvas's coordinates.
    readonly property rect target: {
        const b = editor.selectionBounds
        const topLeft = canvas.toScreen(Qt.point(b.x, b.y))
        return Qt.rect(topLeft.x, topLeft.y, b.width * canvas.zoom, b.height * canvas.zoom)
    }
    // Where it sits, worked out only while it is shown: when it fades out (nothing selected any
    // more, or a drag starts) it stays where it was, instead of jumping to wherever an empty
    // selection would put it.
    property point position
    Binding on position {
        when: bar.shown
        value: bar.editor.floatingPosition(bar.target, Qt.size(bar.width, bar.height),
                                           Qt.rect(0, 0, bar.canvas.width, bar.canvas.height), Space.sm)
        restoreMode: Binding.RestoreNone
    }

    // Follows the selection 1:1 (also while scrolling the canvas): never animated.
    x: canvas.x + position.x
    y: canvas.y + position.y + offset
    width: row.implicitWidth + 2 * Space.xs
    height: Sizes.toolbar

    // Layering: it fades in, rising a few pixels, when something gets selected.
    property real offset: 0
    state: shown ? "shown" : "hidden"
    states: [
        State { name: "shown"; PropertyChanges { bar.opacity: 1; bar.offset: 0; bar.visible: true } },
        State { name: "hidden"; PropertyChanges { bar.opacity: 0; bar.offset: Space.xs; bar.visible: false } }
    ]
    transitions: [
        Transition {
            to: "shown"
            PropertyAction { property: "visible" }
            NumberAnimation { properties: "opacity,offset"; duration: Motion.normal; easing.type: Motion.enter }
        },
        Transition {
            to: "hidden"
            SequentialAnimation {
                NumberAnimation { properties: "opacity,offset"; duration: Motion.out(Motion.normal); easing.type: Motion.exit }
                PropertyAction { property: "visible" }
            }
        }
    ]

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
            model: [["straight", qsTr("Straight")], ["curved", qsTr("Curved")], ["elbow", qsTr("Elbow")]]
            delegate: IconButton {
                anchors.verticalCenter: parent?.verticalCenter
                required property var modelData
                visible: bar.isConnection
                iconName: "style-" + modelData[0]
                tip: modelData[1]
                checked: bar.connection.style === modelData[0]
                onClicked: bar.editor.setConnectionStyle(modelData[0])
            }
        }

        Separator {
            visible: bar.isConnection
            anchors.verticalCenter: parent.verticalCenter
        }

        // Arrowheads: [name, arrow at start, arrow at end, tooltip]
        Repeater {
            model: [["none", false, false, qsTr("No arrows")],
                    ["end", false, true, qsTr("Arrow at end")],
                    ["start", true, false, qsTr("Arrow at start")],
                    ["both", true, true, qsTr("Arrows at both ends")]]
            delegate: IconButton {
                anchors.verticalCenter: parent?.verticalCenter
                required property var modelData
                visible: bar.isConnection
                iconName: "arrows-" + modelData[0]
                tip: modelData[3]
                checked: bar.connection.fromArrow === modelData[1] && bar.connection.toArrow === modelData[2]
                onClicked: bar.editor.setArrows(modelData[1], modelData[2])
            }
        }

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            visible: bar.isConnection
            iconName: "label"
            tip: qsTr("Label")
            onClicked: bar.canvas.editLabel(bar.connection.id)
        }
    }
}
