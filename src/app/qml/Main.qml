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
	required property CanvasSession canvasSession   // the Editor's: its file, history and whether it can be edited
	required property HitTest hitTest     // what is where on the open Canvas
	required property Exporter exporter   // writes it out as a file or an image
	// Created by main.cpp: the Library, its rows as the sidebar shows them, and the open Canvas.
	required property Library library
	required property LibraryRows libraryRows

	// Our shortcuts stay out of the way while text is being typed.
	readonly property bool typing: activeFocusItem !== null && activeFocusItem.visible
								   && activeFocusItem.cursorPosition !== undefined
	// ...and while the Canvas can't be shown (its file can't be read).
	readonly property bool canEdit: !typing && window.canvasSession.editable

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

	function toggleSidebar() {
		settings.sidebarOpen = !settings.sidebarOpen
		if (!settings.sidebarOpen && sidebar.activeFocus)
			focusCanvas()
	}

	// A new Project or Canvas: its name can be typed straight away, in the sidebar if it is
	// open, otherwise in the toolbar.
	function startRename(projectName, canvasName) {
		if (settings.sidebarOpen)
			sidebar.renameRow(projectName, canvasName)
		else if (canvasName)
			toolbar.renameCanvas()
		else
			toolbar.renameProject()
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

	// What goes wrong anywhere is said in the toast.
	function showError(message) { toast.show(message, { error: true }) }

	Component.onCompleted: {
		for (const source of [canvasSession, library, exporter])
			source.errorOccurred.connect(showError)
		library.load()   // the Editor then opens what was open last time
	}

	Connections {
		target: window.library
		function onCreatedEmpty(projectName, canvasName) { Qt.callLater(window.startRename, projectName, canvasName) }
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

		CanvasToolbar {
			id: toolbar
			width: parent.width
			z: 1
			library: window.library
			editor: window.editor
			canvasSession: window.canvasSession
			canvasView: canvasView
			sidebarOpen: settings.sidebarOpen
			snapToGrid: editorSettings.snapToGrid
			onSidebarToggleRequested: window.toggleSidebar()
			onCanvasFocusRequested: window.focusCanvas()
			onImportRequested: importDialog.open()
			onExportRequested: (suffix) => {
				window.focusCanvas()
				exportDialog.exportAs(suffix)
			}
			onSnapToggled: (snap) => editorSettings.snapToGrid = snap
		}

		CanvasView {
			id: canvasView
			anchors.top: toolbar.bottom
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.bottom: parent.bottom
			editor: window.editor
			hitTest: window.hitTest
			library: window.library
			ask: askDialog
			enabled: window.canvasSession.editable
		}

		SelectionBar {
			editor: window.editor
			canvasView: canvasView
		}

		LoadErrorPanel {
			anchors.centerIn: canvasView
			width: Math.min(460, canvasView.width - 2 * Space.xl)
			message: window.canvasSession.notEditableReason
			folder: window.library.folder
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
			window.library.importCanvases(canvasUrls(drop.urls))
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
	Shortcut { sequences: [StandardKey.SelectAll]; enabled: window.canEdit; onActivated: window.editor.selection.selectAll() }
	Shortcut { sequences: [StandardKey.Cut]; enabled: window.canEdit; onActivated: window.editor.cut() }
	Shortcut { sequences: [StandardKey.Copy]; enabled: window.canEdit; onActivated: window.editor.copy() }
	Shortcut { sequences: [StandardKey.Paste]; enabled: window.canEdit; onActivated: window.editor.paste(canvasView.pointerWorld()) }
	Shortcut { sequence: "Ctrl+D"; enabled: window.canEdit; onActivated: window.editor.duplicate() }
	Shortcut { sequence: "Ctrl+G"; enabled: window.canEdit; onActivated: canvasView.editNew(window.editor.createGroup(canvasView.pointerWorld())) }
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
	Shortcut { sequences: [StandardKey.Save]; onActivated: { window.focusCanvas(); window.canvasSession.save() } }

	// ---- File dialogs ----

	FileDialog {
		id: importDialog
		title: qsTr("Import canvas into “%1”").arg(window.library.currentProject)
		fileMode: FileDialog.OpenFiles
		nameFilters: [qsTr("Canvas (*.canvas)")]
		onAccepted: window.library.importCanvases(selectedFiles)
	}

	ExportDialog {
		id: exportDialog
		exporter: window.exporter
		editor: window.editor
		canvasName: window.library.currentCanvas
		onExported: (file) => {
			const path = file.toString()
			const folder = path.slice(0, path.lastIndexOf("/"))
			const name = decodeURIComponent(path.slice(path.lastIndexOf("/") + 1))
			toast.show(qsTr("Exported “%1”").arg(name),
					   { action: qsTr("Show in folder"), onAction: () => Qt.openUrlExternally(folder) })
		}
	}
}
