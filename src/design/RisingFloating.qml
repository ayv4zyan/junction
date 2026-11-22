import QtQuick

// A Floating that comes and goes: while `shown` it fades in, rising `rise` pixels into place,
// and it fades out the same way back. Layering: it arrives on top of what is under it.
Floating {
    id: floating
    property bool shown: false
    property real rise: Space.sm
    property real offset: 0   // how far below its place it is during the fade

    transform: Translate { y: floating.offset }
    state: shown ? "shown" : "hidden"
    states: [
        State { name: "shown"; PropertyChanges { floating.opacity: 1; floating.offset: 0; floating.visible: true } },
        State { name: "hidden"; PropertyChanges { floating.opacity: 0; floating.offset: floating.rise; floating.visible: false } }
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
}
