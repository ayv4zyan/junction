pragma ComponentBehavior: Bound
import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Junction
import JunctionDesign

// The window: the sidebar (the Library) on the left, the open Canvas with its toolbar on the
// right.
ApplicationWindow {
    id: window

    required property Editor editor
    // Created by main.cpp: the Library, its rows as the sidebar shows them, and the open Canvas.
    required property Library library
    required property LibraryModel libraryRows

    // Our shortcuts stay out of the way while text is being typed.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.visible
                                   && activeFocusItem.cursorPosition !== undefined
    // ...and while the Canvas can't be shown (its file can't be read).
    readonly property bool canEdit: !typing && window.editor.loadError === ""

    // Colors, font and the controls' look come from the JunctionDesign style (main.cpp).
    width: 1280
    height: 800
    visible: true

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

    // Kept between runs in the same place as always (QSettings' "snapToGrid").
    Settings {
        id: editorSettings
        property bool snapToGrid: true
    }

    Binding {
        target: window.editor
        property: "snapToGrid"
        value: editorSettings.snapToGrid
    }

    Connections {
        target: window.editor
        function onErrorOccurred(message) { toast.show(message, { error: true }) }
    }

    Connections {
        target: window.library
        function onErrorOccurred(message) { toast.show(message, { error: true }) }
    }

    Component.onCompleted: editor.start()

    Connections {
        target: window.libraryRows
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
        // Continuity: the canvas slides over instead of jumping. Not while dragging the divider.
        Behavior on width {
            enabled: !resizer.pressed
            NumberAnimation {
                duration: settings.sidebarOpen ? Motion.slow : Motion.out(Motion.slow)
                easing.type: settings.sidebarOpen ? Motion.enter : Motion.exit
            }
        }

        Sidebar {
            id: sidebar
            x: sidebarSlot.width - width
            width: settings.sidebarWidth
            height: parent.height
            visible: sidebarSlot.width > 0
            library: window.library
            libraryRows: window.libraryRows
            ask: askDialog
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
        color: Colors.divider
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
                spacing: Space.xs

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
                        MenuItem { text: qsTr("New canvas"); onTriggered: window.libraryRows.createCanvas(window.library.currentProject) }
                        MenuItem { text: qsTr("New project"); onTriggered: window.libraryRows.createProject() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Import canvas…"); onTriggered: importDialog.open() }
                        MenuItem { text: qsTr("Export as canvas file…"); onTriggered: window.exportAs("canvas") }
                        MenuItem { text: qsTr("Export PNG…"); onTriggered: window.exportAs("png") }
                        MenuItem { text: qsTr("Export SVG…"); onTriggered: window.exportAs("svg") }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Open library folder"); onTriggered: Qt.openUrlExternally(window.library.folder) }
                    }
                }
                IconButton { iconName: "undo"; tip: qsTr("Undo (Ctrl+Z)"); enabled: window.editor.canUndo; onClicked: window.editor.undo() }
                IconButton { iconName: "redo"; tip: qsTr("Redo (Ctrl+Shift+Z)"); enabled: window.editor.canRedo; onClicked: window.editor.redo() }

                // "Project / Canvas": click either to rename it. Long names never push the rest
                // of the toolbar away: they shrink with "…" (the tooltip shows them in full);
                // while renaming, the field takes the free space.
                InlineName {
                    id: projectName
                    Layout.leftMargin: Space.xs
                    Layout.fillWidth: true
                    Layout.minimumWidth: editing ? 160 : 40
                    Layout.maximumWidth: editing ? 100000 : implicitWidth
                    clickToEdit: true
                    text: window.library.currentProject
                    color: Colors.mutedText
                    tip: qsTr("Project — click to rename")
                    problemFor: (name) => window.library.projectNameProblem(name, window.library.currentProject)
                    onRenamed: (name) => window.library.renameProject(window.library.currentProject, name)
                    onFinished: window.focusCanvas()
                }
                Label {
                    text: "/"
                    color: Colors.mutedText
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
                    tip: window.editor.filePath + "\n" + qsTr("Click to rename (F2)")
                    problemFor: (name) => window.library.canvasNameProblem(window.library.currentProject, name, window.library.currentCanvas)
                    onRenamed: (name) => window.library.renameCanvas(window.library.currentProject, window.library.currentCanvas, name)
                    onFinished: window.focusCanvas()
                }
                Item { Layout.fillWidth: true }
                CheckBox {
                    focusPolicy: Qt.NoFocus   // keep keyboard focus on the canvas
                    text: qsTr("Snap to grid")
                    checked: editorSettings.snapToGrid
                    onToggled: editorSettings.snapToGrid = checked
                    ToolTip.visible: hovered
                    ToolTip.delay: Motion.tooltipDelay
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
            editor: window.editor
            enabled: window.editor.loadError === ""
        }

        SelectionBar {
            editor: window.editor
            canvas: canvasView
        }

        // A Canvas whose file can't be read: say why. It is never saved, so the file stays.
        Floating {
            visible: window.editor.loadError !== ""
            anchors.centerIn: canvasView
            width: Math.min(460, canvasView.width - 2 * Space.xl)
            height: errorColumn.implicitHeight + 2 * Space.xl
            ColumnLayout {
                id: errorColumn
                anchors.fill: parent
                anchors.margins: Space.xl
                spacing: Space.sm
                Label {
                    text: qsTr("This canvas can't be opened")
                    font.pixelSize: Fonts.title
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    text: window.editor.loadError
                    wrapMode: Text.Wrap
                    color: Colors.mutedText
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Junction won't change the file. Fix it in another editor, or delete the canvas from the sidebar.")
                    wrapMode: Text.Wrap
                    color: Colors.mutedText
                }
                Button {
                    Layout.topMargin: Space.md
                    text: qsTr("Open library folder")
                    onClicked: Qt.openUrlExternally(window.library.folder)
                }
            }
        }

        Toast {
            id: toast
            anchors.horizontalCenter: canvasView.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Space.xl
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
            window.libraryRows.importCanvases(canvasUrls(drop.urls))
        }
        Rectangle {
            anchors.fill: parent
            visible: fileDrop.containsDrag
            color: Qt.alpha(Colors.selection, 0.10)
            border.color: Colors.selection
            border.width: 2
            Label {
                anchors.centerIn: parent
                padding: Space.lg
                text: qsTr("Drop to add to “%1”").arg(window.library.currentProject)
                font.pixelSize: Fonts.title
                background: Floating { }
            }
        }
    }

    AskDialog { id: askDialog }

    // ---- Keyboard ----

    Shortcut { sequences: [StandardKey.Undo]; enabled: window.canEdit; onActivated: window.editor.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: window.canEdit; onActivated: window.editor.redo() }
    Shortcut { sequences: ["Delete", "Backspace"]; enabled: window.canEdit; onActivated: window.editor.deleteSelection() }
    Shortcut { sequences: [StandardKey.SelectAll]; enabled: window.canEdit; onActivated: window.editor.selectAll() }
    Shortcut { sequences: [StandardKey.Cut]; enabled: window.canEdit; onActivated: window.editor.cut() }
    Shortcut { sequences: [StandardKey.Copy]; enabled: window.canEdit; onActivated: window.editor.copy() }
    Shortcut { sequences: [StandardKey.Paste]; enabled: window.canEdit; onActivated: window.editor.paste(canvasView.pointerWorld()) }
    Shortcut { sequence: "Ctrl+D"; enabled: window.canEdit; onActivated: window.editor.duplicate() }
    Shortcut { sequence: "Ctrl+G"; enabled: window.canEdit; onActivated: canvasView.editText(window.editor.createGroup(canvasView.pointerWorld())) }
    // Arrow keys nudge the selection by one grid step.
    Shortcut { sequence: "Left"; enabled: window.canEdit; onActivated: window.editor.nudge(Qt.point(-1, 0)) }
    Shortcut { sequence: "Right"; enabled: window.canEdit; onActivated: window.editor.nudge(Qt.point(1, 0)) }
    Shortcut { sequence: "Up"; enabled: window.canEdit; onActivated: window.editor.nudge(Qt.point(0, -1)) }
    Shortcut { sequence: "Down"; enabled: window.canEdit; onActivated: window.editor.nudge(Qt.point(0, 1)) }
    Shortcut { sequence: "Ctrl+0"; onActivated: canvasView.resetZoom() }
    Shortcut { sequence: "Ctrl+1"; onActivated: canvasView.userFit() }

    // The Library. Each first takes the keyboard back to the canvas (see focusCanvas).
    Shortcut {
        sequences: [StandardKey.New]
        onActivated: { window.focusCanvas(); window.libraryRows.createCanvas(window.library.currentProject) }
    }
    Shortcut {
        sequence: "Ctrl+Shift+N"
        onActivated: { window.focusCanvas(); window.libraryRows.createProject() }
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
    Shortcut { sequence: "Alt+Up"; enabled: !window.typing; onActivated: { window.focusCanvas(); window.libraryRows.openAdjacent(-1) } }
    Shortcut { sequence: "Alt+Down"; enabled: !window.typing; onActivated: { window.focusCanvas(); window.libraryRows.openAdjacent(1) } }
    Shortcut {
        sequence: "F2"
        enabled: !window.typing
        onActivated: window.startRename(window.library.currentProject, window.library.currentCanvas)
    }
    // Saving is automatic; Ctrl+S just does it now.
    Shortcut { sequences: [StandardKey.Save]; onActivated: { window.focusCanvas(); window.editor.save() } }

    // ---- File dialogs ----

    FileDialog {
        id: importDialog
        title: qsTr("Import canvas into “%1”").arg(window.library.currentProject)
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Canvas (*.canvas)")]
        onAccepted: window.libraryRows.importCanvases(selectedFiles)
    }

    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        onAccepted: {
            const written = defaultSuffix === "canvas" ? window.editor.exportCanvas(selectedFile)
                                                       : window.editor.exportImage(selectedFile, Colors.forPainter)
            const file = written.toString()
            if (!file)
                return   // the Editor has reported why
            const folder = file.slice(0, file.lastIndexOf("/"))
            const name = decodeURIComponent(file.slice(file.lastIndexOf("/") + 1))
            toast.show(qsTr("Exported “%1”").arg(name),
                       { action: qsTr("Show in folder"), onAction: () => Qt.openUrlExternally(folder) })
        }
    }
}
