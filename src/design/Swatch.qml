import QtQuick
import QtQuick.Controls

// One color to pick: a small square inside a full-size click target. A ring marks the chosen
// color; hovering grows the square a little.
AbstractButton {
    id: swatch

    property color fill
    property bool outlined: false   // a light fill (the default color) needs its border to show
    property bool chosen: false
    property string tip

    implicitWidth: Sizes.control
    implicitHeight: Sizes.control
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    background: Item {
        Rectangle {
            anchors.centerIn: parent
            width: Sizes.icon
            height: Sizes.icon
            radius: Sizes.radius
            color: swatch.fill
            border.color: swatch.outlined ? Colors.cardBorder : Qt.darker(swatch.fill, 1.15)
            scale: swatch.hovered ? 1.12 : 1
            Behavior on scale { NumberAnimation { duration: Motion.fast; easing.type: Motion.enter } }
        }
        Rectangle {
            visible: swatch.chosen
            anchors.centerIn: parent
            width: Sizes.icon + 6
            height: Sizes.icon + 6
            radius: Sizes.radius
            color: "transparent"
            border.color: Colors.text
            border.width: 1.5
        }
    }

    ToolTip.visible: hovered && tip !== ""
    ToolTip.delay: Motion.tooltipDelay
    ToolTip.text: tip
}
