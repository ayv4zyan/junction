pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import Junction
import JunctionDesign

// The Library: a search box, then every Project with its Canvases (see SidebarRow), and their
// menus. Rows can be dragged to reorder them (see SidebarDrag).
Rectangle {
	id: sidebar

	required property Library library
	required property LibraryRows libraryRows   // the rows shown
	required property AskDialog ask
	property real headerHeight: 40
	property alias list: list

	readonly property int rowHeight: Sizes.control
	// Row layout, shared with SidebarRow: a highlight inset from the sides, then a Project's
	// chevron; a Canvas row starts where the Project's name does.
	readonly property int rowInset: Space.sm
	readonly property int chevronSize: Sizes.icon + Space.xs
	readonly property int canvasIndent: rowInset + Space.xs + chevronSize + Space.xs
	property int menuRow: -1               // the row whose menu is open
	property var menuRowData: ({})

	signal hideRequested()
	signal canvasFocusRequested()           // done here: the keyboard goes back to the canvas

	function focusCanvas() { canvasFocusRequested() }

	function focusSearch() {
		search.forceActiveFocus()
		search.selectAll()
	}

	// Starts renaming a row (a Project row when `canvas` is empty), scrolling it into view.
	function renameRow(project, canvas) {
		search.text = ""
		if (canvas)
			library.setExpanded(project, true)
		Qt.callLater(() => {
			const i = libraryRows.rowOf(project, canvas)
			if (i < 0)
				return
			list.positionViewAtIndex(i, ListView.Contain)
			list.forceLayout()
			const item = list.itemAtIndex(i) as SidebarRow
			if (item)
				item.rename()
		})
	}

	// The name with the searched-for part in bold, or "" when not searching.
	function highlight(name) {
		const query = libraryRows.filter.trim()
		const at = name.toLowerCase().indexOf(query.toLowerCase())
		if (!query || at < 0)
			return ""
		const escape = (s) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
		return escape(name.slice(0, at)) + "<b><font color=\"" + Colors.accentText + "\">"
			 + escape(name.slice(at, at + query.length)) + "</font></b>" + escape(name.slice(at + query.length))
	}

	// ---- Menus and deleting ----

	function openMenu(row, item, x, y) {
		menuRow = row.index
		menuRowData = libraryRows.rowAt(row.index)
		if (row.isProject)
			projectMenu.popup(item, x, y)
		else
			canvasMenu.popup(item, x, y)
	}

	function askToDeleteProject(project, count) {
		ask.confirm({
			title: qsTr("Delete “%1”?").arg(project),
			message: count === 1 ? qsTr("“%1” and its canvas will be moved to the trash.").arg(project)
								 : qsTr("“%1” and its %2 canvases will be moved to the trash.").arg(project).arg(count),
			action: qsTr("Delete"), destructive: true,
			onAccepted: () => library.deleteProject(project)
		})
	}

	// A Project keeps at least one Canvas, so deleting its only one deletes the Project.
	function askToDeleteCanvas(project, canvas, count) {
		if (count === 1) {
			ask.confirm({
				title: qsTr("Delete project “%1”?").arg(project),
				message: qsTr("“%1” is the only canvas in “%2”, so the project will be moved to the trash with it.")
							 .arg(canvas).arg(project),
				action: qsTr("Delete"), destructive: true,
				onAccepted: () => library.deleteProject(project)
			})
			return
		}
		ask.confirm({
			title: qsTr("Delete “%1”?").arg(canvas),
			message: qsTr("“%1” will be moved to the trash.").arg(canvas),
			action: qsTr("Delete"), destructive: true,
			onAccepted: () => library.deleteCanvas(project, canvas)
		})
	}

	color: Colors.sidebar

	// ---- Search and new Project ----

	Item {
		id: top
		width: parent.width
		height: sidebar.headerHeight

		RowLayout {
			anchors.fill: parent
			anchors.leftMargin: Space.sm
			anchors.rightMargin: Space.sm
			spacing: Space.xs

			TextField {
				id: search
				Layout.fillWidth: true
				Layout.rightMargin: Space.xs
				placeholderText: qsTr("Search")
				leftPadding: Space.sm + Sizes.icon + Space.sm   // room for the magnifier
				rightPadding: clearButton.visible ? clearButton.width + Space.xs : Space.sm
				onTextChanged: sidebar.libraryRows.filter = text
				// Enter opens the first Canvas found; Esc gives up.
				function openFirst() {
					if (sidebar.libraryRows.openFirstMatch()) {
						text = ""
						sidebar.focusCanvas()
					}
				}
				Keys.onReturnPressed: openFirst()
				Keys.onEnterPressed: openFirst()
				Keys.onEscapePressed: {
					text = ""
					sidebar.focusCanvas()
				}
				ToolTip.visible: hovered && !activeFocus && text === ""
				ToolTip.delay: Motion.tooltipDelay
				ToolTip.text: qsTr("Search canvases and projects (Ctrl+F)")

				IconImage {
					anchors.left: parent.left
					anchors.leftMargin: Space.sm
					anchors.verticalCenter: parent.verticalCenter
					source: "../icons/search.svg"
					sourceSize: Qt.size(Sizes.icon, Sizes.icon)
					color: Colors.mutedText
				}
				ToolButton {
					id: clearButton
					visible: search.text !== ""
					anchors.right: parent.right
					anchors.rightMargin: Space.xs / 2
					anchors.verticalCenter: parent.verticalCenter
					implicitWidth: Sizes.control - Space.xs
					implicitHeight: Sizes.control - Space.xs
					padding: (implicitWidth - Sizes.iconSmall) / 2
					focusPolicy: Qt.NoFocus
					display: AbstractButton.IconOnly
					icon.source: "../icons/close.svg"
					icon.width: Sizes.iconSmall
					icon.height: Sizes.iconSmall
					icon.color: Colors.mutedText
					onClicked: search.text = ""
				}
			}
			IconButton {
				iconName: "plus"
				tip: qsTr("New project (Ctrl+Shift+N)")
				onClicked: {
					sidebar.focusCanvas()
					sidebar.library.createProject()
				}
			}
			IconButton {
				iconName: "sidebar"
				tip: qsTr("Hide sidebar (Ctrl+\\)")
				onClicked: sidebar.hideRequested()
			}
		}
	}

	Rectangle {
		anchors.top: top.bottom
		width: parent.width
		height: 1
		color: Colors.divider
	}

	// ---- The rows ----

	ListView {
		id: list

		// Kept across model resets, which would otherwise jump back to the top.
		property real keptY: 0
		function clampY(y) { return Math.max(originY, Math.min(y, originY + contentHeight - height)) }

		anchors.top: top.bottom
		anchors.topMargin: Space.xs
		anchors.bottom: parent.bottom
		width: parent.width
		model: sidebar.libraryRows
		clip: true
		boundsBehavior: Flickable.StopAtBounds
		highlightFollowsCurrentItem: false
		currentIndex: -1
		// While dragging every row stays created: the drop position is computed from them,
		// and the dragged row must survive being scrolled out of view.
		cacheBuffer: rowDrag.active ? 100000 : 600
		ScrollBar.vertical: ScrollBar {}
		footer: Item { width: 1; height: Space.sm }

		delegate: SidebarRow {
			sidebar: sidebar
			library: sidebar.library
			libraryRows: sidebar.libraryRows
			rowDrag: rowDrag
		}

		// A new row fades in; rows making room for it slide (continuity).
		add: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Motion.enter } }
		displaced: Transition { NumberAnimation { property: "y"; duration: Motion.normal; easing.type: Motion.enter } }

		Connections {
			target: sidebar.libraryRows
			function onModelAboutToBeReset() { list.keptY = list.contentY }
			function onModelReset() {
				list.forceLayout()
				list.contentY = list.clampY(list.keptY)
			}
		}

		Label {
			anchors.centerIn: parent
			width: parent.width - 2 * Space.lg
			visible: list.count === 0 && sidebar.libraryRows.filter !== ""
			horizontalAlignment: Text.AlignHCenter
			wrapMode: Text.Wrap
			color: Colors.mutedText
			text: qsTr("Nothing matches “%1”").arg(sidebar.libraryRows.filter.trim())
		}
	}

	SidebarDrag {
		id: rowDrag
		anchors.fill: parent
		library: sidebar.library
		libraryRows: sidebar.libraryRows
		list: list
		canvasIndent: sidebar.canvasIndent
		projectIndent: sidebar.rowInset + Space.xs
	}

	// ---- Menus ----

	Menu {
		id: projectMenu
		onClosed: sidebar.menuRow = -1
		MenuItem {
			text: qsTr("New canvas")
			onTriggered: Qt.callLater(sidebar.library.createCanvas, sidebar.menuRowData.project)
		}
		MenuItem {
			text: qsTr("Rename")
			onTriggered: sidebar.renameRow(sidebar.menuRowData.project, "")
		}
		MenuItem {
			text: qsTr("Duplicate")
			onTriggered: Qt.callLater(sidebar.library.duplicateProject, sidebar.menuRowData.project)
		}
		MenuSeparator {}
		MenuEntry {
			text: qsTr("Delete…")
			destructive: true
			onTriggered: sidebar.askToDeleteProject(sidebar.menuRowData.project, sidebar.menuRowData.canvasCount)
		}
	}

	Menu {
		id: canvasMenu
		onClosed: sidebar.menuRow = -1
		MenuItem {
			text: qsTr("Rename")
			onTriggered: sidebar.renameRow(sidebar.menuRowData.project, sidebar.menuRowData.name)
		}
		MenuItem {
			text: qsTr("Duplicate")
			onTriggered: Qt.callLater(sidebar.library.duplicateCanvas, sidebar.menuRowData.project, sidebar.menuRowData.name)
		}
		Menu {
			id: moveMenu
			title: qsTr("Move to")
			enabled: sidebar.library.projectNames.length > 1 && sidebar.menuRowData.canvasCount > 1
			Instantiator {
				model: sidebar.library.projectNames
				delegate: MenuItem {
					required property string modelData
					text: modelData
					enabled: modelData !== sidebar.menuRowData.project
					onTriggered: {
						const data = sidebar.menuRowData
						const target = modelData
						Qt.callLater(() => sidebar.library.moveCanvas(data.project, data.name, target,
																	 sidebar.library.canvasCount(target)))
					}
				}
				onObjectAdded: (index, object) => moveMenu.insertItem(index, object)
				onObjectRemoved: (index, object) => moveMenu.removeItem(object)
			}
		}
		MenuSeparator {}
		MenuEntry {
			text: qsTr("Delete…")
			destructive: true
			onTriggered: sidebar.askToDeleteCanvas(sidebar.menuRowData.project, sidebar.menuRowData.name,
												   sidebar.menuRowData.canvasCount)
		}
	}
}
