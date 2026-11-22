import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

// A toolbar button: no fill until hovered; square when it shows only an icon. `checked` (a
// current tool or style) gets the accent tint.
T.ToolButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: (Sizes.control - Sizes.icon) / 2
    horizontalPadding: display === T.AbstractButton.IconOnly ? padding : Space.sm
    spacing: Space.sm
    icon.width: Sizes.icon
    icon.height: Sizes.icon
    icon.color: Colors.text   // the default; an icon's own color wins
    opacity: enabled ? 1 : Colors.disabledOpacity

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: Colors.text
    }

    background: Rectangle {
        implicitWidth: Sizes.control
        implicitHeight: Sizes.control
        radius: Sizes.radius
        color: control.down ? Colors.pressed
             : control.checked ? Colors.current
             : control.hovered ? Colors.hover : "transparent"
        Behavior on color { ColorAnimation { duration: Motion.fast } }
        FocusRing { visible: control.visualFocus }
    }
}
