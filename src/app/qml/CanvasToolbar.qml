pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Junction
import JunctionDesign

// Above the Canvas: sidebar and file menu, undo and redo, "Project / Canvas" (click to
// rename), snap to grid and zoom.
ToolBar {
	id: toolbar
	required property Library library
	required property Editor editor
	required property CanvasSession canvasSession
	required property CanvasView canvasView
	required property bool sidebarOpen
	required property bool snapToGrid

	signal sidebarToggleRequested()
	signal canvasFocusRequested()   // when done here: the keyboard goes back to the canvas
	signal importRequested()
	signal exportRequested(string suffix)
	signal snapToggled(bool snap)

	function renameProject() { projectName.edit() }
	function renameCanvas() { canvasName.edit() }

	RowLayout {
		anchors.fill: parent
		spacing: Space.xs

		IconButton {
			visible: !toolbar.sidebarOpen
			iconName: "sidebar"
			tip: qsTr("Show sidebar (Ctrl+\\)")
			onClicked: toolbar.sidebarToggleRequested()
		}
		IconButton {
			iconName: "menu"
			tip: qsTr("Menu")
			onClicked: {
				toolbar.canvasFocusRequested()
				fileMenu.popup()
			}
			Menu {
				id: fileMenu
				MenuItem { text: qsTr("New canvas"); onTriggered: toolbar.library.createCanvas(toolbar.library.currentProject) }
				MenuItem { text: qsTr("New project"); onTriggered: toolbar.library.createProject() }
				MenuSeparator {}
				MenuItem { text: qsTr("Import canvas…"); onTriggered: toolbar.importRequested() }
				MenuItem { text: qsTr("Export as canvas file…"); onTriggered: toolbar.exportRequested("canvas") }
				MenuItem { text: qsTr("Export PNG…"); onTriggered: toolbar.exportRequested("png") }
				MenuItem { text: qsTr("Export SVG…"); onTriggered: toolbar.exportRequested("svg") }
				MenuSeparator {}
				MenuItem { text: qsTr("Open library folder"); onTriggered: Qt.openUrlExternally(toolbar.library.folder) }
			}
		}
		IconButton { iconName: "undo"; tip: qsTr("Undo (Ctrl+Z)"); enabled: toolbar.canvasSession.canUndo; onClicked: toolbar.editor.undo() }
		IconButton { iconName: "redo"; tip: qsTr("Redo (Ctrl+Shift+Z)"); enabled: toolbar.canvasSession.canRedo; onClicked: toolbar.editor.redo() }

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
			text: toolbar.library.currentProject
			color: Colors.mutedText
			tip: qsTr("Project — click to rename")
			problemFor: (name) => toolbar.library.projectNameProblem(name, toolbar.library.currentProject)
			onRenamed: (name) => toolbar.library.renameProject(toolbar.library.currentProject, name)
			onFinished: toolbar.canvasFocusRequested()
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
			text: toolbar.library.currentCanvas
			font.weight: Font.DemiBold
			tip: toolbar.canvasSession.filePath + "\n" + qsTr("Click to rename (F2)")
			problemFor: (name) => toolbar.library.canvasNameProblem(toolbar.library.currentProject, name, toolbar.library.currentCanvas)
			onRenamed: (name) => toolbar.library.renameCanvas(toolbar.library.currentProject, toolbar.library.currentCanvas, name)
			onFinished: toolbar.canvasFocusRequested()
		}
		Item { Layout.fillWidth: true }
		CheckBox {
			focusPolicy: Qt.NoFocus   // keep keyboard focus on the canvas
			text: qsTr("Snap to grid")
			checked: toolbar.snapToGrid
			onToggled: toolbar.snapToggled(checked)
			ToolTip.visible: hovered
			ToolTip.delay: Motion.tooltipDelay
			ToolTip.text: qsTr("Hold Alt while dragging to do the opposite")
		}
		ToolButton { focusPolicy: Qt.NoFocus; text: Math.round(toolbar.canvasView.zoom * 100) + "%"; onClicked: toolbar.canvasView.resetZoom() }
		ToolButton { focusPolicy: Qt.NoFocus; text: qsTr("Fit"); onClicked: toolbar.canvasView.userFit() }
	}
}
