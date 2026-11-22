import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

// A grey button. `highlighted` fills it with palette.highlight: the accent for the main action
// (or red in a destructive AskDialog). `flat` has no fill until hovered.
T.Button {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Space.xs
    horizontalPadding: Space.md
    spacing: Space.sm
    icon.width: Sizes.icon
    icon.height: Sizes.icon
    opacity: enabled ? 1 : Colors.disabledOpacity

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        icon: control.icon
        defaultIconColor: control.highlighted ? Colors.onAccent : Colors.text
        text: control.text
        font: control.font
        color: defaultIconColor
    }

    background: Rectangle {
        readonly property color fill: control.palette.highlight
        implicitWidth: 72
        implicitHeight: Sizes.control
        radius: Sizes.radius
        color: control.highlighted ? (control.down ? Qt.darker(fill, 1.2) : control.hovered ? Qt.lighter(fill, 1.08) : fill)
             : control.flat ? (control.down ? Colors.pressed : control.hovered ? Colors.hover : "transparent")
             : Qt.alpha(Colors.text, control.down ? 0.24 : control.hovered ? 0.17 : 0.11)
        Behavior on color { ColorAnimation { duration: Motion.fast } }
        FocusRing { visible: control.visualFocus }
    }
}
