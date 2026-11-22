pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction
import JunctionDesign

// The Canvas's right-click menus.
// Right-clicking an item first makes it the selection, unless it is already part of it. Then:
// only Connections selected -> connectionMenu; any Card or Group -> selectionMenu.
// Right-clicking empty space opens canvasMenu.
Item {
    id: menus
    required property CanvasView canvasView
    required property AskDialog ask   // for editing several Labels at once
    readonly property Editor editor: canvasView.editor

    function openFor(id, waypointIndex) {
        if (!editor.selection.includes(id))
            editor.select(id)
        const summary = editor.selectionSummary
        if (summary.cards + summary.groups === 0) {
            connectionMenu.waypoint = waypointIndex ?? -1
            connectionMenu.popup()
        } else {
            selectionMenu.popup()
        }
    }

    // One Label is edited in place, like double-clicking it; several get the dialog.
    function editLabels() {
        const summary = editor.selectionSummary
        const targets = summary.labelTargets
        if (targets.length > 1)
            menus.ask.text({ title: qsTr("Edit %1 labels").arg(targets.length), text: summary.sharedLabel,
                       placeholder: qsTr("Label"), action: qsTr("Save"),
                       onAccepted: (text) => editor.setLabels(targets, text) })
        else if (summary.connections === 1)
            canvasView.editLabel(targets[0])
        else
            canvasView.editInPlace(targets[0])
    }

    function labelsText() {
        const count = editor.selectionSummary.labelTargets.length
        return count > 1 ? qsTr("Edit %1 labels…").arg(count) : qsTr("Edit label")
    }

    function openAt(world) {
        canvasMenu.at = world
        canvasMenu.popup()
    }

    // Right-click on empty space: make something here.
    Menu {
        id: canvasMenu
        property point at
        MenuEntry { text: qsTr("New card"); onTriggered: menus.canvasView.editNew(menus.editor.createCard(canvasMenu.at)) }
        MenuEntry { text: qsTr("New group"); onTriggered: menus.canvasView.editNew(menus.editor.createGroup(canvasMenu.at)) }
        MenuSeparator {}
        MenuEntry { text: qsTr("Paste"); hint: "Ctrl+V"; onTriggered: menus.editor.paste(canvasMenu.at) }
    }

    // Right-click on a selection holding a Card or Group.
    Menu {
        id: selectionMenu
        MenuEntry {
            text: qsTr("Edit text")
            enabled: menus.editor.selection.length === 1 && menus.editor.selectionSummary.editableCards === 1
            onTriggered: menus.canvasView.editInPlace(menus.editor.selection[0])
        }
        MenuEntry {
            text: menus.labelsText()
            enabled: menus.editor.selectionSummary.labelTargets.length > 0
            onTriggered: menus.editLabels()
        }
        MenuSeparator {}
        MenuEntry { text: qsTr("Cut"); hint: "Ctrl+X"; onTriggered: menus.editor.cut() }
        MenuEntry { text: qsTr("Copy"); hint: "Ctrl+C"; onTriggered: menus.editor.copy() }
        MenuEntry { text: qsTr("Duplicate"); hint: "Ctrl+D"; onTriggered: menus.editor.duplicate() }
        MenuEntry {
            text: qsTr("Group")
            hint: "Ctrl+G"
            onTriggered: menus.canvasView.editNew(menus.editor.createGroup(menus.canvasView.pointerWorld()))
        }
        MenuSeparator {}
        MenuEntry { text: qsTr("Delete"); hint: "Del"; destructive: true; onTriggered: menus.editor.deleteSelection() }
    }

    // Right-click on a selection of only Connections.
    Menu {
        id: connectionMenu
        property int waypoint: -1   // the Waypoint right-clicked, if any; set by openFor()
        MenuEntry {
            text: qsTr("Remove waypoint")
            visible: connectionMenu.waypoint >= 0
            height: visible ? implicitHeight : 0
            onTriggered: menus.editor.removeWaypoint(menus.editor.selection[0], connectionMenu.waypoint)
        }
        MenuEntry { text: menus.labelsText(); onTriggered: menus.editLabels() }
        MenuEntry {
            text: qsTr("Remove label")
            enabled: menus.editor.selectionSummary.labeledConnections > 0
            onTriggered: menus.editor.removeConnectionLabels()
        }
        MenuSeparator {}
        MenuEntry { text: qsTr("Delete"); hint: "Del"; destructive: true; onTriggered: menus.editor.deleteSelection() }
    }
}
