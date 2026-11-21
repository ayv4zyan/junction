pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Junction

ApplicationWindow {
    id: window
    property string initialFile

    // Our shortcuts stay out of the way while text is being typed.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.visible
                                   && activeFocusItem.cursorPosition !== undefined

    width: 1280
    height: 800
    visible: true
    color: Theme.background
    title: window.fileName(canvasEditor.filePath) + " — Junction"

    onClosing: canvasView.forceActiveFocus()   // commits text still being edited

    // When the window comes back, the text field that had focus may have been hidden
    // meanwhile; give the keyboard back to the canvas then.
    onActiveChanged: {
        if (active && (activeFocusItem === null || !activeFocusItem.visible))
            canvasView.forceActiveFocus()
    }

    function fileName(path) { return path.substring(path.lastIndexOf("/") + 1).replace(/\.canvas$/, "") }
    function exportAs(suffix) {
        exportDialog.defaultSuffix = suffix
        exportDialog.nameFilters = [suffix.toUpperCase() + " image (*." + suffix + ")"]
        exportDialog.open()
    }

    Editor {
        id: canvasEditor
        Component.onCompleted: start(window.initialFile)
        onErrorOccurred: (message) => {
            errorDialog.text = message
            errorDialog.open()
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6

            IconButton {
                iconName: "menu"
                tip: qsTr("Menu")
                onClicked: fileMenu.popup()
                Menu {
                    id: fileMenu
                    MenuItem { text: qsTr("New canvas"); onTriggered: canvasEditor.newCanvas() }
                    MenuItem { text: qsTr("Open…"); onTriggered: openDialog.open() }
                    MenuItem { text: qsTr("Save as…"); onTriggered: saveDialog.open() }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Export PNG…"); onTriggered: window.exportAs("png") }
                    MenuItem { text: qsTr("Export SVG…"); onTriggered: window.exportAs("svg") }
                }
            }
            IconButton { iconName: "undo"; tip: qsTr("Undo (Ctrl+Z)"); enabled: canvasEditor.canUndo; onClicked: canvasEditor.undo() }
            IconButton { iconName: "redo"; tip: qsTr("Redo (Ctrl+Shift+Z)"); enabled: canvasEditor.canRedo; onClicked: canvasEditor.redo() }
            // The Canvas name. Click it to rename the file. A long name never pushes the rest of
            // the toolbar: it takes its natural width while there's room, then shrinks with "…"
            // (the tooltip shows it in full).
            Label {
                id: titleLabel
                visible: !titleEditor.visible
                text: window.fileName(canvasEditor.filePath)
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 60
                Layout.maximumWidth: implicitWidth
                leftPadding: 6
                rightPadding: 6
                topPadding: 3
                bottomPadding: 3
                background: Rectangle {
                    radius: 4
                    color: titleMouse.containsMouse ? Qt.alpha(Theme.text, 0.08) : "transparent"
                }
                ToolTip.visible: titleMouse.containsMouse
                ToolTip.delay: 400
                ToolTip.text: (truncated ? text + "\n" : "") + canvasEditor.filePath + "\n" + qsTr("Click to rename")
                MouseArea {
                    id: titleMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.IBeamCursor
                    onClicked: titleEditor.open()
                }
            }
            // Opens across all the free space, so it doesn't grow while typing; a name longer than
            // that scrolls inside it.
            TextField {
                id: titleEditor
                visible: false
                Layout.fillWidth: true
                Layout.minimumWidth: 120
                function open() {
                    text = titleLabel.text
                    visible = true
                    forceActiveFocus()
                    selectAll()
                }
                // Enter and clicking elsewhere both end editing; losing focus does the rename.
                onAccepted: canvasView.forceActiveFocus()
                onActiveFocusChanged: {
                    if (!activeFocus && visible) {
                        visible = false
                        canvasEditor.rename(text)
                    }
                }
                Keys.onEscapePressed: {
                    text = titleLabel.text   // unchanged name: rename() does nothing
                    canvasView.forceActiveFocus()
                }
            }
            Item { Layout.fillWidth: true }
            CheckBox {
                focusPolicy: Qt.NoFocus   // keep keyboard focus on the canvas
                text: qsTr("Snap to grid")
                checked: canvasEditor.snapToGrid
                onToggled: canvasEditor.snapToGrid = checked
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Hold Alt while dragging to do the opposite")
            }
            ToolButton { focusPolicy: Qt.NoFocus; text: Math.round(canvasView.zoom * 100) + "%"; onClicked: canvasView.resetZoom() }
            ToolButton { focusPolicy: Qt.NoFocus; text: qsTr("Fit"); onClicked: canvasView.fitToContent() }
        }
    }

    CanvasView {
        id: canvasView
        anchors.fill: parent
        editor: canvasEditor
    }

    SelectionBar {
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        editor: canvasEditor
        canvas: canvasView
    }

    Shortcut { sequences: [StandardKey.Undo]; enabled: !window.typing; onActivated: canvasEditor.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: !window.typing; onActivated: canvasEditor.redo() }
    Shortcut { sequences: ["Delete", "Backspace"]; enabled: !window.typing; onActivated: canvasEditor.deleteSelection() }
    Shortcut { sequences: [StandardKey.SelectAll]; enabled: !window.typing; onActivated: canvasEditor.selectAll() }
    Shortcut { sequences: [StandardKey.Copy]; enabled: !window.typing; onActivated: canvasEditor.copy() }
    Shortcut { sequences: [StandardKey.Paste]; enabled: !window.typing; onActivated: canvasEditor.paste(canvasView.pointerWorld()) }
    Shortcut { sequence: "Ctrl+D"; enabled: !window.typing; onActivated: canvasEditor.duplicate() }
    Shortcut { sequence: "Ctrl+G"; enabled: !window.typing; onActivated: canvasView.editingId = canvasEditor.createGroup(canvasView.pointerWorld()) }
    // Arrow keys nudge the selection by one grid step.
    Shortcut { sequence: "Left"; enabled: !window.typing; onActivated: canvasEditor.nudge(Qt.point(-20, 0)) }
    Shortcut { sequence: "Right"; enabled: !window.typing; onActivated: canvasEditor.nudge(Qt.point(20, 0)) }
    Shortcut { sequence: "Up"; enabled: !window.typing; onActivated: canvasEditor.nudge(Qt.point(0, -20)) }
    Shortcut { sequence: "Down"; enabled: !window.typing; onActivated: canvasEditor.nudge(Qt.point(0, 20)) }
    Shortcut { sequence: "Ctrl+0"; onActivated: canvasView.resetZoom() }
    Shortcut { sequence: "Ctrl+1"; onActivated: canvasView.fitToContent() }
    // Taking focus away from a card being edited commits its text first.
    Shortcut { sequences: [StandardKey.New]; onActivated: { canvasView.forceActiveFocus(); canvasEditor.newCanvas() } }
    Shortcut { sequences: [StandardKey.Open]; onActivated: { canvasView.forceActiveFocus(); openDialog.open() } }
    Shortcut {
        sequences: [StandardKey.Save]
        onActivated: {
            canvasView.forceActiveFocus()
            if (canvasEditor.untitled)
                saveDialog.open()
            else
                canvasEditor.save()
        }
    }
    Shortcut { sequences: [StandardKey.SaveAs]; onActivated: { canvasView.forceActiveFocus(); saveDialog.open() } }

    FileDialog {
        id: openDialog
        nameFilters: [qsTr("Canvas (*.canvas)")]
        onAccepted: canvasEditor.open(selectedFile)
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "canvas"
        nameFilters: [qsTr("Canvas (*.canvas)")]
        onAccepted: canvasEditor.saveAs(selectedFile)
    }

    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        onAccepted: canvasEditor.exportImage(selectedFile, Theme.palette)
    }

    MessageDialog {
        id: errorDialog
        buttons: MessageDialog.Ok
    }
}
