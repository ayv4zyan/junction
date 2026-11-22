import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

// A menu: a floating panel of 28px rows (MenuItem, MenuEntry, MenuSeparator).
T.Menu {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Space.xs
    margins: 0
    overlap: Space.xs

    delegate: MenuItem { }

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: Window.window
                     ? contentHeight + control.topPadding + control.bottomPadding > control.height
                     : false
        clip: true
        currentIndex: control.currentIndex
        ScrollIndicator.vertical: ScrollIndicator { }
    }

    background: Floating {
        implicitWidth: 200
        implicitHeight: Sizes.control
    }

    // Layering: it appears on top of the work, briefly growing into place.
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Motion.enter }
        NumberAnimation { property: "scale"; from: Motion.scaleFrom; to: 1; duration: Motion.normal; easing.type: Motion.enter }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.out(Motion.normal); easing.type: Motion.exit }
    }
}
