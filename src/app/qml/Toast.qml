import QtQuick
import QtQuick.Controls
import Junction

// A short message at the bottom of the window that goes away by itself (or when clicked).
// Errors stay a little longer and get a red edge.
Rectangle {
    id: toast

    function show(message, isError) {
        label.text = message
        toast.isError = isError === true
        toast.opacity = 1
        timer.interval = toast.isError ? 6000 : 3000
        timer.restart()
    }

    property bool isError: false

    width: Math.min(label.implicitWidth + 32, parent.width - 40)
    height: label.implicitHeight + 20
    radius: 8
    color: Theme.panel
    border.color: isError ? Theme.danger : Theme.cardBorder
    border.width: isError ? 1.5 : 1
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 180 } }

    Label {
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, toast.parent.width - 72)
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        color: Theme.text
    }

    Timer {
        id: timer
        onTriggered: toast.opacity = 0
    }

    MouseArea {
        anchors.fill: parent
        onClicked: toast.opacity = 0
    }
}
