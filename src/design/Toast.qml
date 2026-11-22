import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A short message that goes away by itself, optionally with one action:
//   toast.show(message)
//   toast.show(message, { error: true })
//   toast.show(message, { action: qsTr("Show in folder"), onAction: () => ... })
// Errors get a red edge and stay longer; so does a toast with an action, which also stays while
// the pointer is over it. Clicking the message dismisses it.
Floating {
    id: toast

    property bool shown: false
    property bool error: false
    property string actionText
    property var actionHandler: null

    function show(message, options) {
        options = options ?? {}
        label.text = message
        error = options.error === true
        actionText = options.action ?? ""
        actionHandler = options.onAction ?? null
        shown = true
        timer.restart()
    }

    implicitWidth: Math.min(row.implicitWidth + 2 * Space.lg, parent ? parent.width - 2 * Space.xl : 480)
    implicitHeight: row.implicitHeight + 2 * Space.sm
    border.color: error ? Colors.danger : Colors.cardBorder
    border.width: error ? 1.5 : 1

    // Layering: it slides up a little as it fades in.
    property real offset: 0
    transform: Translate { y: toast.offset }
    state: shown ? "shown" : "hidden"
    states: [
        State { name: "shown"; PropertyChanges { toast.opacity: 1; toast.offset: 0; toast.visible: true } },
        State { name: "hidden"; PropertyChanges { toast.opacity: 0; toast.offset: Space.sm; toast.visible: false } }
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

    Timer {
        id: timer
        interval: toast.error || toast.actionText ? 6000 : 3000
        running: toast.shown && !hover.hovered
        onTriggered: toast.shown = false
    }
    HoverHandler { id: hover }
    TapHandler { onTapped: toast.shown = false }

    RowLayout {
        id: row
        anchors.centerIn: parent
        width: Math.min(implicitWidth, toast.width - 2 * Space.lg)
        spacing: Space.md

        Label {
            id: label
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
        Button {
            id: actionButton
            visible: toast.actionText !== ""
            flat: true
            horizontalPadding: Space.sm
            text: toast.actionText
            contentItem: Label {
                text: actionButton.text
                color: Colors.accentText
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                const action = toast.actionHandler
                toast.shown = false
                if (action)
                    action()
            }
        }
    }
}
