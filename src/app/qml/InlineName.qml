import QtQuick
import QtQuick.Controls
import Junction

// A name shown as text that turns into a text field to rename it, in the toolbar and in the
// sidebar. While typing, a name that can't be used is outlined in red with the reason below;
// Enter keeps the field open then. Esc, or clicking away with such a name, keeps the old name.
Item {
    id: root

    property string text
    property string richText                // shown instead of `text` when set (search matches)
    property var problemFor: (name) => ""   // why a name can't be used, "" if it can
    property bool clickToEdit: false        // the toolbar: a click renames
    property string tip                     // tooltip while hovering, with clickToEdit
    property alias font: label.font
    property color color: Theme.text
    property bool editing: false

    signal renamed(string name)   // a changed, usable name; sent after editing has ended
    signal finished()             // editing ended, either way

    function edit() {
        field.text = root.text
        editing = true
        field.forceActiveFocus()
        field.selectAll()
    }

    function end(keep) {
        if (!editing)
            return
        const name = field.text.trim()
        editing = false   // first: hiding the field takes its focus, which calls end() again
        root.finished()
        if (keep && name !== root.text && root.problemFor(name) === "")
            Qt.callLater(root.renamed, name)   // renaming rebuilds lists this may be part of
    }

    implicitWidth: Math.ceil(label.implicitWidth)   // layouts round down; a fraction short elides
    implicitHeight: Math.max(label.implicitHeight, 26)

    Label {
        id: label
        anchors.fill: parent
        visible: !root.editing
        verticalAlignment: Text.AlignVCenter
        text: root.richText || root.text
        textFormat: root.richText ? Text.StyledText : Text.PlainText
        color: root.color
        elide: Text.ElideRight
        leftPadding: root.clickToEdit ? 6 : 0
        rightPadding: root.clickToEdit ? 6 : 0
        background: Rectangle {
            visible: root.clickToEdit
            radius: 4
            color: mouse.containsMouse ? Theme.rowHover : "transparent"
        }
        ToolTip.visible: root.clickToEdit && mouse.containsMouse && (root.tip !== "" || label.truncated)
        ToolTip.delay: 500
        ToolTip.text: (label.truncated ? root.text + (root.tip ? "\n" : "") : "") + root.tip
        // `visible`, not `enabled`: Qt shows a disabled MouseArea's cursor all the same.
        // A plain arrow, like the toolbar buttons beside it: the name is a button until clicked.
        MouseArea {
            id: mouse
            anchors.fill: parent
            visible: root.clickToEdit
            hoverEnabled: true
            onClicked: root.edit()
        }
    }

    TextField {
        id: field
        readonly property string problem: root.editing ? root.problemFor(text.trim()) : ""
        visible: root.editing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        leftPadding: 5
        rightPadding: 5
        topPadding: 3
        bottomPadding: 3
        font: label.font
        selectByMouse: true
        background: Rectangle {
            radius: 4
            color: Theme.cardFill
            border.width: 1.5
            border.color: field.problem ? Theme.danger : Theme.selection
        }
        // Its own tooltip rather than the shared ToolTip.visible one, which other hovered
        // controls can take over; shown below the field so it never covers the row above.
        ToolTip {
            parent: field
            x: 0
            y: field.height + 4
            contentItem: Label {
                text: field.problem
                color: Theme.text
                wrapMode: Text.Wrap
            }
            background: Rectangle {
                radius: 5
                color: Theme.panel
                border.color: Theme.danger
            }
            visible: field.problem !== ""
            text: field.problem
            delay: 0
            timeout: -1
            closePolicy: Popup.NoAutoClose
        }
        onAccepted: if (!problem) root.end(true)
        onActiveFocusChanged: if (!activeFocus) root.end(!problem)
        Keys.onEscapePressed: root.end(false)
    }
}
