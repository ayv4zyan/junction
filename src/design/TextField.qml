import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

// A single-line text field: an outline that turns accent while typing.
T.TextField {
    id: control

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    padding: Space.xs
    leftPadding: Space.sm
    rightPadding: Space.sm
    verticalAlignment: TextInput.AlignVCenter

    color: Colors.text
    selectionColor: Colors.accentFill
    selectedTextColor: Colors.onAccent
    placeholderTextColor: Colors.mutedText
    selectByMouse: true

    PlaceholderText {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        elide: Text.ElideRight
        renderType: control.renderType
        visible: !control.length && !control.preeditText && (!control.activeFocus || control.horizontalAlignment !== Qt.AlignHCenter)
    }

    background: Rectangle {
        implicitWidth: 160
        implicitHeight: Sizes.control
        radius: Sizes.radius
        color: Colors.cardFill
        border.width: control.activeFocus ? 1.5 : 1
        border.color: control.activeFocus ? Colors.selection : Colors.cardBorder
    }
}
