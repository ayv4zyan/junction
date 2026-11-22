import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A menu item that shows its keyboard shortcut on the right, greyed. The shortcut itself is
// defined in Main.qml; this only displays it.
MenuItem {
    id: entry
    property string hint

    contentItem: RowLayout {
        spacing: 24
        opacity: entry.enabled ? 1 : 0.4   // greyed out when it can't be used
        Label {
            text: entry.text
            color: entry.highlighted ? entry.palette.highlightedText : entry.palette.text
            Layout.fillWidth: true
        }
        Label {
            visible: entry.hint !== ""
            text: entry.hint
            color: entry.highlighted ? entry.palette.highlightedText : entry.palette.text
            opacity: 0.6
        }
    }
}
