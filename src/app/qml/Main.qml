pragma ComponentBehavior: Bound
import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Junction

// The window: the sidebar (the Library) on the left, the open Canvas with its toolbar on the
// right.
ApplicationWindow {
    id: window

    readonly property LibraryModel library: canvasEditor.library

    // Our shortcuts stay out of the way while text is being typed.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.visible
                                   && activeFocusItem.cursorPosition !== undefined
    // ...and while the Canvas can't be shown (its file can't be read).
    readonly property bool canEdit: !typing && canvasEditor.loadError === ""

    width: 1280
    height: 800
    visible: true
    color: Theme.background

    // The stock controls (toolbar, menus, tooltips, checkboxes, scroll bars) are drawn from the
    // palette, which otherwise comes from the system (e.g. the GTK theme) and can disagree with
    // Theme. Every control and popup in the window inherits this one.
    palette {
        window: Theme.background
        windowText: Theme.text
        base: Theme.panel
        alternateBase: Theme.cardFill
        text: Theme.text
        button: Theme.panel
        buttonText: Theme.text
        brightText: "white"
        placeholderText: Theme.mutedText
        highlight: Theme.selection
        highlightedText: "white"
        link: Theme.selection
        toolTipBase: Theme.panel
        toolTipText: Theme.text
        light: Qt.lighter(Theme.panel, 1.5)
        midlight: Qt.lighter(Theme.panel, 1.2)
        mid: Theme.cardBorder
        dark: Theme.divider
        shadow: Theme.dark ? "black" : "#b0b0b0"

        disabled.windowText: Theme.mutedText
        disabled.text: Theme.mutedText
        disabled.buttonText: Theme.mutedText
    }

    title: (library.currentCanvas ? library.currentCanvas + " · " + library.currentProject + " — " : "") + "Junction"

    onClosing: canvasView.forceActiveFocus()   // commits text still being edited

    // When the window comes back, the text field that had focus may have been hidden
    // meanwhile; give the keyboard back to the canvas then.
    onActiveChanged: {
        if (active && (activeFocusItem === null || !activeFocusItem.visible))
            canvasView.forceActiveFocus()
    }

    // Everything that switches Canvas or changes files first takes the keyboard back to the
    // canvas, which ends any text editing there (and so saves it to the Canvas it belongs to).
    function focusCanvas() { canvasView.forceActiveFocus() }

    function exportAs(suffix) {
        focusCanvas()
        exportDialog.defaultSuffix = suffix
        exportDialog.nameFilters = suffix === "canvas" ? [qsTr("Canvas (*.canvas)")]
                                                       : [suffix.toUpperCase() + qsTr(" image (*.%1)").arg(suffix)]
        exportDialog.selectedFile = StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/"
                                    + encodeURIComponent(library.currentCanvas) + "." + suffix
        exportDialog.open()
    }

    function toggleSidebar() {
        settings.sidebarOpen = !settings.sidebarOpen
        if (!settings.sidebarOpen && sidebar.activeFocus)
            focusCanvas()
    }

    // A new Project or Canvas: its name can be typed straight away, in the sidebar if it is
    // open, otherwise in the toolbar.
    function startRename(project, canvas) {
        if (settings.sidebarOpen)
            sidebar.renameRow(project, canvas)
        else if (canvas)
            canvasName.edit()
        else
            projectName.edit()
    }

    Settings {
        id: settings
        category: "Window"
        property bool sidebarOpen: true
        property real sidebarWidth: 260
    }

    Editor {
        id: canvasEditor
        Component.onCompleted: start()
        onErrorOccurred: (message) => toast.show(message, true)
    }

    Connections {
        target: window.library
        function onRenameRequested(project, canvas) { Qt.callLater(window.startRename, project, canvas) }
    }

    // ---- Sidebar ----

    // Slides in and out from the left; its contents keep their width meanwhile.
    Item {
        id: sidebarSlot
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: settings.sidebarOpen ? settings.sidebarWidth : 0
        clip: true
        Behavior on width {
            enabled: !resizer.pressed
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }

        Sidebar {
            id: sidebar
            x: sidebarSlot.width - width
            width: settings.sidebarWidth
            height: parent.height
            visible: sidebarSlot.width > 0
            library: window.library
            confirm: confirmDialog
            headerHeight: toolbar.height
            onHideRequested: window.toggleSidebar()
            onCanvasFocusRequested: window.focusCanvas()
        }
    }

    Rectangle {
        id: divider
        anchors.left: sidebarSlot.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: sidebarSlot.width > 0 ? 1 : 0
        color: Theme.divider
    }

    // Drag the divider to resize the sidebar.
    MouseArea {
        id: resizer
        property real startX
        property real startWidth
        visible: settings.sidebarOpen
        anchors.horizontalCenter: divider.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 7
        z: 1
        cursorShape: Qt.SplitHCursor
        onPressed: (event) => {
            startX = mapToItem(null, event.x, 0).x
            startWidth = settings.sidebarWidth
        }
        onPositionChanged: (event) => {
            const delta = mapToItem(null, event.x, 0).x - startX
            settings.sidebarWidth = Math.round(Math.max(200, Math.min(480, startWidth + delta, window.width - 320)))
        }
    }

    // ---- The open Canvas ----

    Item {
        id: main
        anchors.left: divider.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        ToolBar {
            id: toolbar
            width: parent.width
            z: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 2

                IconButton {
                    visible: !settings.sidebarOpen
                    iconName: "sidebar"
                    tip: qsTr("Show sidebar (Ctrl+\\)")
                    onClicked: window.toggleSidebar()
                }
                IconButton {
                    iconName: "menu"
                    tip: qsTr("Menu")
                    onClicked: {
                        window.focusCanvas()
                        fileMenu.popup()
                    }
                    Menu {
                        id: fileMenu
                        MenuItem { text: qsTr("New canvas"); onTriggered: window.library.createCanvas(window.library.currentProject) }
                        MenuItem { text: qsTr("New project"); onTriggered: window.library.createProject() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Import canvas…"); onTriggered: importDialog.open() }
                        MenuItem { text: qsTr("Export as canvas file…"); onTriggered: window.exportAs("canvas") }
                        MenuItem { text: qsTr("Export PNG…"); onTriggered: window.exportAs("png") }
                        MenuItem { text: qsTr("Export SVG…"); onTriggered: window.exportAs("svg") }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Open library folder"); onTriggered: Qt.openUrlExternally(window.library.folder) }
                    }
                }
                IconButton { iconName: "undo"; tip: qsTr("Undo (Ctrl+Z)"); enabled: canvasEditor.canUndo; onClicked: canvasEditor.undo() }
                IconButton { iconName: "redo"; tip: qsTr("Redo (Ctrl+Shift+Z)"); enabled: canvasEditor.canRedo; onClicked: canvasEditor.redo() }

                // "Project / Canvas": click either to rename it. Long names never push the rest
                // of the toolbar away: they shrink with "…" (the tooltip shows them in full);
                // while renaming, the field takes the free space.
                InlineName {
                    id: projectName
                    Layout.leftMargin: 4
                    Layout.fillWidth: true
                    Layout.minimumWidth: editing ? 160 : 40
                    Layout.maximumWidth: editing ? 100000 : implicitWidth
                    clickToEdit: true
                    text: window.library.currentProject
                    color: Theme.mutedText
                    tip: qsTr("Project — click to rename")
                    problemFor: (name) => window.library.projectNameProblem(name, window.library.currentProject)
                    onRenamed: (name) => window.library.renameProject(window.library.currentProject, name)
                    onFinished: window.focusCanvas()
                }
                Label {
                    text: "/"
                    color: Theme.mutedText
                    visible: !projectName.editing && !canvasName.editing
                }
                InlineName {
                    id: canvasName
                    Layout.fillWidth: true
                    Layout.minimumWidth: editing ? 160 : 60
                    Layout.maximumWidth: editing ? 100000 : implicitWidth
                    clickToEdit: true
                    text: window.library.currentCanvas
                    font.weight: Font.DemiBold
                    tip: canvasEditor.filePath + "\n" + qsTr("Click to rename (F2)")
                    problemFor: (name) => window.library.canvasNameProblem(window.library.currentProject, name, window.library.currentCanvas)
                    onRenamed: (name) => window.library.renameCanvas(window.library.currentProject, window.library.currentCanvas, name)
                    onFinished: window.focusCanvas()
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
                ToolButton { focusPolicy: Qt.NoFocus; text: qsTr("Fit"); onClicked: canvasView.userFit() }
            }
        }

        CanvasView {
            id: canvasView
            anchors.top: toolbar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            editor: canvasEditor
            enabled: canvasEditor.loadError === ""
        }

        SelectionBar {
            anchors.top: canvasView.top
            anchors.topMargin: 12
            anchors.horizontalCenter: canvasView.horizontalCenter
            editor: canvasEditor
            canvas: canvasView
        }

        // A Canvas whose file can't be read: say why. It is never saved, so the file stays.
        Rectangle {
            visible: canvasEditor.loadError !== ""
            anchors.centerIn: canvasView
            width: Math.min(460, canvasView.width - 40)
            height: errorColumn.implicitHeight + 40
            radius: 10
            color: Theme.panel
            border.color: Theme.cardBorder
            ColumnLayout {
                id: errorColumn
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10
                Label {
                    text: qsTr("This canvas can't be opened")
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Label {
                    Layout.fillWidth: true
                    text: canvasEditor.loadError
                    wrapMode: Text.Wrap
                    color: Theme.mutedText
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Junction won't change the file. Fix it in another editor, or delete the canvas from the sidebar.")
                    wrapMode: Text.Wrap
                    color: Theme.mutedText
                }
                Button {
                    text: qsTr("Open library folder")
                    onClicked: Qt.openUrlExternally(window.library.folder)
                }
            }
        }

        Toast {
            id: toast
            anchors.horizontalCenter: canvasView.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            z: 2
        }
    }

    // .canvas files dropped on the window are added to the open Project.
    DropArea {
        id: fileDrop
        anchors.fill: parent
        function canvasUrls(urls) { return urls.filter((url) => url.toString().toLowerCase().endsWith(".canvas")) }
        onEntered: (drag) => {
            if (drag.hasUrls && canvasUrls(drag.urls).length > 0)
                drag.accept(Qt.CopyAction)
            else
                drag.accepted = false
        }
        onDropped: (drop) => {
            window.focusCanvas()
            window.library.importCanvases(canvasUrls(drop.urls))
        }
        Rectangle {
            anchors.fill: parent
            visible: fileDrop.containsDrag
            color: Qt.alpha(Theme.selection, 0.10)
            border.color: Theme.selection
            border.width: 2
            Label {
                anchors.centerIn: parent
                padding: 14
                text: qsTr("Drop to add to “%1”").arg(window.library.currentProject)
                font.pixelSize: 16
                color: Theme.text
                background: Rectangle { radius: 8; color: Theme.panel; border.color: Theme.cardBorder }
            }
        }
    }

    ConfirmDialog { id: confirmDialog }

    // ---- Keyboard ----

    Shortcut { sequences: [StandardKey.Undo]; enabled: window.canEdit; onActivated: canvasEditor.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: window.canEdit; onActivated: canvasEditor.redo() }
    Shortcut { sequences: ["Delete", "Backspace"]; enabled: window.canEdit; onActivated: canvasEditor.deleteSelection() }
    Shortcut { sequences: [StandardKey.SelectAll]; enabled: window.canEdit; onActivated: canvasEditor.selectAll() }
    Shortcut { sequences: [StandardKey.Cut]; enabled: window.canEdit; onActivated: canvasEditor.cut() }
    Shortcut { sequences: [StandardKey.Copy]; enabled: window.canEdit; onActivated: canvasEditor.copy() }
    Shortcut { sequences: [StandardKey.Paste]; enabled: window.canEdit; onActivated: canvasEditor.paste(canvasView.pointerWorld()) }
    Shortcut { sequence: "Ctrl+D"; enabled: window.canEdit; onActivated: canvasEditor.duplicate() }
    Shortcut { sequence: "Ctrl+G"; enabled: window.canEdit; onActivated: canvasView.editingId = canvasEditor.createGroup(canvasView.pointerWorld()) }
    // Arrow keys nudge the selection by one grid step.
    Shortcut { sequence: "Left"; enabled: window.canEdit; onActivated: canvasEditor.nudge(Qt.point(-20, 0)) }
    Shortcut { sequence: "Right"; enabled: window.canEdit; onActivated: canvasEditor.nudge(Qt.point(20, 0)) }
    Shortcut { sequence: "Up"; enabled: window.canEdit; onActivated: canvasEditor.nudge(Qt.point(0, -20)) }
    Shortcut { sequence: "Down"; enabled: window.canEdit; onActivated: canvasEditor.nudge(Qt.point(0, 20)) }
    Shortcut { sequence: "Ctrl+0"; onActivated: canvasView.resetZoom() }
    Shortcut { sequence: "Ctrl+1"; onActivated: canvasView.userFit() }

    // The Library. Each first takes the keyboard back to the canvas (see focusCanvas).
    Shortcut {
        sequences: [StandardKey.New]
        onActivated: { window.focusCanvas(); window.library.createCanvas(window.library.currentProject) }
    }
    Shortcut {
        sequence: "Ctrl+Shift+N"
        onActivated: { window.focusCanvas(); window.library.createProject() }
    }
    Shortcut { sequence: "Ctrl+\\"; onActivated: window.toggleSidebar() }
    Shortcut {
        sequences: [StandardKey.Find, "Ctrl+P"]
        onActivated: {
            settings.sidebarOpen = true
            sidebar.focusSearch()
        }
    }
    // Previous and next Canvas of the Project.
    Shortcut { sequence: "Alt+Up"; enabled: !window.typing; onActivated: { window.focusCanvas(); window.library.openAdjacent(-1) } }
    Shortcut { sequence: "Alt+Down"; enabled: !window.typing; onActivated: { window.focusCanvas(); window.library.openAdjacent(1) } }
    Shortcut {
        sequence: "F2"
        enabled: !window.typing
        onActivated: window.startRename(window.library.currentProject, window.library.currentCanvas)
    }
    // Saving is automatic; Ctrl+S just does it now.
    Shortcut { sequences: [StandardKey.Save]; onActivated: { window.focusCanvas(); canvasEditor.save() } }

    // ---- File dialogs ----

    FileDialog {
        id: importDialog
        title: qsTr("Import canvas into “%1”").arg(window.library.currentProject)
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Canvas (*.canvas)")]
        onAccepted: window.library.importCanvases(selectedFiles)
    }

    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        onAccepted: {
            if (defaultSuffix === "canvas")
                canvasEditor.exportCanvas(selectedFile)
            else
                canvasEditor.exportImage(selectedFile, Theme.palette)
        }
    }
}
