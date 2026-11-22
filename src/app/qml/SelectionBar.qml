pragma ComponentBehavior: Bound
import QtQuick
import Junction
import JunctionDesign
import "CanvasColors.js" as CanvasColors
import "Placement.js" as Placement

// The floating toolbar for the Selection: a color picker for anything, plus Path Style and
// arrowhead pickers and Label when a single Connection is selected. Each picker shows the current
// value and opens its choices in a second bar (PropertyPicker). It sits just above the selection
// (below it near the top edge, always inside the canvas), so the controls appear next to what
// they change.
// It hides while anything is dragged, the canvas is panned, a selection rectangle is drawn or a
// new Connection is pulled out, so it never chases the pointer.
RisingFloating {
    id: bar
    required property Editor editor
    required property CanvasView canvasView
    readonly property var connection: editor.selection.connection   // null unless one Connection is selected
    readonly property bool isConnection: connection !== null
    shown: editor.selection.ids.length > 0 && !editor.dragging && !canvasView.manipulating

    // The selection on screen, in the canvas's coordinates.
    readonly property rect target: {
        const b = editor.selection.bounds
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

    // Above the selection the pickers open upwards, below it downwards: away from what they change.
    readonly property bool above: position.y < target.y

    // The current arrowheads as one of the four arrow icons.
    readonly property string arrowsIcon: !connection ? "arrows-none"
        : connection.fromArrow ? (connection.toArrow ? "arrows-both" : "arrows-start")
        : (connection.toArrow ? "arrows-end" : "arrows-none")

    Row {
        id: row
        anchors.centerIn: parent

        PropertyPicker {
            id: colorPicker
            anchors.verticalCenter: parent.verticalCenter
            readonly property string current: bar.editor.selection.color
            swatch: current ? CanvasColors.resolve(current, Colors.presets, Colors.line) : Colors.cardFill
            swatchOutlined: current === ""
            tip: qsTr("Color")
            opensUp: bar.above

            Repeater {
                model: [{ color: "", tip: qsTr("Default color") }, { color: "1", tip: qsTr("Red") },
                        { color: "2", tip: qsTr("Orange") }, { color: "3", tip: qsTr("Yellow") },
                        { color: "4", tip: qsTr("Green") }, { color: "5", tip: qsTr("Cyan") },
                        { color: "6", tip: qsTr("Purple") }]
                delegate: Swatch {
                    required property var modelData
                    anchors.verticalCenter: parent?.verticalCenter
                    fill: modelData.color ? CanvasColors.resolve(modelData.color, Colors.presets, Colors.line) : Colors.cardFill
                    outlined: modelData.color === ""
                    chosen: colorPicker.current === modelData.color
                    tip: modelData.tip
                    onClicked: {
                        bar.editor.setColor(modelData.color)
                        colorPicker.close()
                    }
                }
            }
        }

        Separator {
            visible: bar.isConnection
            anchors.verticalCenter: parent.verticalCenter
        }

        PropertyPicker {
            id: stylePicker
            readonly property var styles: [
                { style: CanvasEnums.PathStyle.Straight, icon: "style-straight", tip: qsTr("Straight") },
                { style: CanvasEnums.PathStyle.Curved, icon: "style-curved", tip: qsTr("Curved") },
                { style: CanvasEnums.PathStyle.Elbow, icon: "style-elbow", tip: qsTr("Elbow") }]
            anchors.verticalCenter: parent.verticalCenter
            visible: bar.isConnection
            iconName: styles.find(s => s.style === bar.connection?.style)?.icon ?? "style-straight"
            tip: qsTr("Path style")
            opensUp: bar.above

            Repeater {
                model: stylePicker.styles
                delegate: IconButton {
                    required property var modelData
                    anchors.verticalCenter: parent?.verticalCenter
                    iconName: modelData.icon
                    tip: modelData.tip
                    checked: bar.connection?.style === modelData.style
                    onClicked: {
                        bar.editor.setConnectionStyle(modelData.style)
                        stylePicker.close()
                    }
                }
            }
        }

        PropertyPicker {
            id: arrowsPicker
            anchors.verticalCenter: parent.verticalCenter
            visible: bar.isConnection
            iconName: bar.arrowsIcon
            tip: qsTr("Arrowheads")
            opensUp: bar.above

            Repeater {
                model: [{ icon: "arrows-none", fromArrow: false, toArrow: false, tip: qsTr("No arrows") },
                        { icon: "arrows-end", fromArrow: false, toArrow: true, tip: qsTr("Arrow at end") },
                        { icon: "arrows-start", fromArrow: true, toArrow: false, tip: qsTr("Arrow at start") },
                        { icon: "arrows-both", fromArrow: true, toArrow: true, tip: qsTr("Arrows at both ends") }]
                delegate: IconButton {
                    required property var modelData
                    anchors.verticalCenter: parent?.verticalCenter
                    iconName: modelData.icon
                    tip: modelData.tip
                    checked: bar.arrowsIcon === modelData.icon
                    onClicked: {
                        bar.editor.setArrows(modelData.fromArrow, modelData.toArrow)
                        arrowsPicker.close()
                    }
                }
            }
        }

        Separator {
            visible: bar.isConnection
            anchors.verticalCenter: parent.verticalCenter
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
