pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction

// Floating toolbar for the selection: colors for anything, plus path style, arrows and
// label when a single Connection is selected.
Rectangle {
    id: bar
    required property Editor editor
    required property var canvas
    readonly property var connection: editor.selectedConnection
    readonly property bool isConnection: connection.id !== undefined

    visible: editor.selection.length > 0
    width: row.implicitWidth + 16
    height: 44
    radius: 10
    color: Theme.panel
    border.color: Theme.cardBorder

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6

        Repeater {
            model: ["", "1", "2", "3", "4", "5", "6"]
            delegate: Rectangle {
                required property string modelData
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                radius: 11
                color: modelData ? Theme.resolve(modelData, Theme.line) : Theme.cardFill
                // a ring around the color the selection already has
                border.color: bar.editor.selectionColor === modelData ? Theme.text : Theme.cardBorder
                border.width: bar.editor.selectionColor === modelData ? 2.5 : 1.5
                ToolTip.visible: swatchMouse.containsMouse && !modelData
                ToolTip.text: qsTr("Default color")
                MouseArea {
                    id: swatchMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: bar.editor.setColor(parent.modelData)
                }
            }
        }

        Rectangle {
            visible: bar.isConnection
            width: 1
            height: 24
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.cardBorder
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

        Rectangle {
            visible: bar.isConnection
            width: 1
            height: 24
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.cardBorder
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
