pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import Junction

// The Library: a search box, then every Project with its Canvases (see SidebarRow).
// Rows can be dragged: a Project among Projects, a Canvas within its Project or into another.
Rectangle {
    id: sidebar

    required property LibraryModel library
    required property ConfirmDialog confirm
    property real headerHeight: 40
    property alias list: list

    readonly property int rowHeight: 30
    property int menuRow: -1               // the row whose menu is open
    property var menuData: ({})

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
            const i = library.rowOf(project, canvas)
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
        const query = library.filter.trim()
        const at = name.toLowerCase().indexOf(query.toLowerCase())
        if (!query || at < 0)
            return ""
        const escape = (s) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        return escape(name.slice(0, at)) + "<b><font color=\"" + Theme.selection + "\">"
             + escape(name.slice(at, at + query.length)) + "</font></b>" + escape(name.slice(at + query.length))
    }

    // ---- Menus and deleting ----

    function openMenu(row, item, x, y) {
        menuRow = row.index
        menuData = row.rowData()
        if (row.isProject)
            projectMenu.popup(item, x, y)
        else
            canvasMenu.popup(item, x, y)
    }

    function askToDeleteProject(project, count) {
        confirm.ask(qsTr("Delete “%1”?").arg(project),
                    count === 1 ? qsTr("“%1” and its canvas will be moved to the trash.").arg(project)
                                : qsTr("“%1” and its %2 canvases will be moved to the trash.").arg(project).arg(count),
                    () => library.deleteProject(project))
    }

    function askToDeleteCanvas(project, canvas) {
        confirm.ask(qsTr("Delete “%1”?").arg(canvas),
                    qsTr("“%1” will be moved to the trash.").arg(canvas),
                    () => library.deleteCanvas(project, canvas))
    }

    // ---- Dragging rows ----
    //
    // `drop` describes where the dragged row would land:
    //   a Canvas: {project, index} plus lineY (a line between rows) or intoRow (a collapsed
    //   Project, shown outlined); a Project: {projectIndex, lineY}. lineY is in list content
    //   coordinates. `noOp` drops would put the row back where it is.

    property var dragItem: null
    readonly property bool dragging: dragItem !== null
    property var drop: null
    property point dragPoint   // in list coordinates

    function startDrag(data) {
        dragItem = data
    }

    function moveDrag(point) {
        dragPoint = point
        drop = dropAt(point.y + list.contentY)
    }

    function endDrag() {
        const item = dragItem
        const target = drop
        cancelDrag()
        if (!item || !target || target.noOp)
            return
        Qt.callLater(() => {
            if (item.kind === "canvas")
                library.moveCanvas(item.project, item.name, target.project, target.index)
            else
                library.moveProject(item.project, target.projectIndex)
        })
    }

    function cancelDrag() {
        dragItem = null
        drop = null
    }

    function dropAt(y) {
        const count = list.count
        if (count === 0)
            return null
        let i = list.indexAt(list.width / 2, y)
        let fraction = 0
        if (i < 0) {   // above the first row or below the last one
            const last = list.itemAtIndex(count - 1)
            i = last && y >= last.y ? count - 1 : 0
            fraction = i === 0 ? 0 : 1
        } else {
            const item = list.itemAtIndex(i)
            fraction = (y - item.y) / item.height
        }
        const row = library.rowAt(i)
        const item = list.itemAtIndex(i)
        if (!item)
            return null

        let target
        if (dragItem.kind === "canvas") {
            if (row.kind === "canvas") {
                const after = fraction >= 0.5
                target = { project: row.project, index: row.canvasIndex + (after ? 1 : 0),
                           lineY: item.y + (after ? item.height : 0) }
            } else if (row.expanded) {   // a Project row: to the top of its list
                target = { project: row.project, index: 0, lineY: item.y + item.height }
            } else {                     // a collapsed Project: to the end of its list
                target = { project: row.project, index: row.canvasCount, intoRow: i }
            }
            target.noOp = target.project === dragItem.project
                && (target.index === dragItem.canvasIndex || target.index === dragItem.canvasIndex + 1)
        } else {
            // Before or after the whole Project under the pointer, whichever half it is in.
            const top = list.itemAtIndex(library.rowOf(row.project))
            const bottom = list.itemAtIndex(library.lastRowOf(row.project))
            if (!top || !bottom)
                return null
            const before = y < (top.y + bottom.y + bottom.height) / 2
            target = { projectIndex: row.projectIndex + (before ? 0 : 1),
                       lineY: before ? top.y : bottom.y + bottom.height }
            target.noOp = target.projectIndex === dragItem.projectIndex
                || target.projectIndex === dragItem.projectIndex + 1
        }
        return target
    }

    color: Theme.sidebar

    // ---- Search and new Project ----

    Item {
        id: top
        width: parent.width
        height: sidebar.headerHeight

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 4
            spacing: 2

            TextField {
                id: search
                Layout.fillWidth: true
                Layout.rightMargin: 2
                placeholderText: qsTr("Search")
                leftPadding: 28
                rightPadding: clearButton.visible ? 26 : 8
                topPadding: 5
                bottomPadding: 5
                selectByMouse: true
                onTextChanged: sidebar.library.filter = text
                // Enter opens the first Canvas found; Esc gives up.
                function openFirst() {
                    if (sidebar.library.openFirstMatch()) {
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
                background: Rectangle {
                    radius: 6
                    color: Theme.cardFill
                    border.width: search.activeFocus ? 1.5 : 1
                    border.color: search.activeFocus ? Theme.selection : Theme.divider
                }
                ToolTip.visible: hovered && !activeFocus && text === ""
                ToolTip.delay: 600
                ToolTip.text: qsTr("Search canvases and projects (Ctrl+F)")

                IconImage {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    source: "../icons/search.svg"
                    sourceSize: Qt.size(14, 14)
                    color: Theme.mutedText
                }
                ToolButton {
                    id: clearButton
                    visible: search.text !== ""
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    implicitWidth: 22
                    implicitHeight: 22
                    padding: 0
                    focusPolicy: Qt.NoFocus
                    display: AbstractButton.IconOnly
                    icon.source: "../icons/close.svg"
                    icon.width: 14
                    icon.height: 14
                    icon.color: Theme.mutedText
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
        color: Theme.divider
    }

    // ---- The rows ----

    ListView {
        id: list

        // Kept across model resets, which would otherwise jump back to the top.
        property real keptY: 0
        function clampY(y) { return Math.max(originY, Math.min(y, originY + contentHeight - height)) }

        anchors.top: top.bottom
        anchors.topMargin: 5
        anchors.bottom: parent.bottom
        width: parent.width
        model: sidebar.library
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        highlightFollowsCurrentItem: false
        currentIndex: -1
        // While dragging every row stays created: the drop position is computed from them,
        // and the dragged row must survive being scrolled out of view.
        cacheBuffer: sidebar.dragging ? 100000 : 600
        ScrollBar.vertical: ScrollBar {}
        footer: Item { width: 1; height: 8 }

        delegate: SidebarRow {
            sidebar: sidebar
        }

        add: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 } }
        displaced: Transition { NumberAnimation { property: "y"; duration: 140; easing.type: Easing.OutCubic } }

        Connections {
            target: sidebar.library
            function onModelAboutToBeReset() { list.keptY = list.contentY }
            function onModelReset() {
                list.forceLayout()
                list.contentY = list.clampY(list.keptY)
            }
        }

        Label {
            anchors.centerIn: parent
            width: parent.width - 32
            visible: list.count === 0 && sidebar.library.filter !== ""
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.mutedText
            text: qsTr("Nothing matches “%1”").arg(sidebar.library.filter.trim())
        }

        // Dragging near the top or bottom edge scrolls.
        Timer {
            interval: 16
            repeat: true
            running: sidebar.dragging && list.contentHeight > list.height
                     && (sidebar.dragPoint.y < 32 || sidebar.dragPoint.y > list.height - 32)
            onTriggered: {
                const y = sidebar.dragPoint.y
                const speed = y < 32 ? -(32 - y) / 2 : (y - list.height + 32) / 2
                list.contentY = list.clampY(list.contentY + Math.max(-16, Math.min(16, speed)))
                sidebar.moveDrag(sidebar.dragPoint)
            }
        }
    }

    // Where the dragged row would land.
    Rectangle {
        readonly property bool indent: sidebar.dragItem !== null && sidebar.dragItem.kind === "canvas"
        visible: sidebar.dragging && sidebar.drop !== null && !sidebar.drop.noOp && sidebar.drop.lineY !== undefined
        x: indent ? 30 : 10
        y: list.y + (sidebar.drop ? sidebar.drop.lineY : 0) - list.contentY - 1
        width: sidebar.width - x - 12
        height: 2
        radius: 1
        color: Theme.selection
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: -6
            width: 7
            height: 7
            radius: 3.5
            color: Theme.sidebar
            border.width: 2
            border.color: Theme.selection
        }
    }

    // The dragged row, following the pointer.
    Rectangle {
        visible: sidebar.dragging
        x: Math.min(list.x + sidebar.dragPoint.x + 14, sidebar.width - width - 4)
        y: list.y + sidebar.dragPoint.y - height / 2
        width: Math.min(ghostRow.implicitWidth + 20, sidebar.width - 24)
        height: 26
        radius: 6
        color: Theme.panel
        border.color: Theme.selection
        opacity: 0.95
        RowLayout {
            id: ghostRow
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 10
            spacing: 6
            IconImage {
                source: sidebar.dragItem && sidebar.dragItem.kind === "canvas" ? "../icons/canvas.svg" : "../icons/project.svg"
                sourceSize: Qt.size(14, 14)
                color: Theme.mutedText
            }
            Label {
                Layout.fillWidth: true
                text: sidebar.dragItem ? sidebar.dragItem.name : ""
                elide: Text.ElideRight
                color: Theme.text
            }
        }
    }

    // ---- Menus ----

    Menu {
        id: projectMenu
        onClosed: sidebar.menuRow = -1
        MenuItem {
            text: qsTr("New canvas")
            onTriggered: Qt.callLater(sidebar.library.createCanvas, sidebar.menuData.project)
        }
        MenuItem {
            text: qsTr("Rename")
            onTriggered: sidebar.renameRow(sidebar.menuData.project, "")
        }
        MenuItem {
            text: qsTr("Duplicate")
            onTriggered: Qt.callLater(sidebar.library.duplicateProject, sidebar.menuData.project)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Delete…")
            onTriggered: sidebar.askToDeleteProject(sidebar.menuData.project, sidebar.menuData.canvasCount)
        }
    }

    Menu {
        id: canvasMenu
        onClosed: sidebar.menuRow = -1
        MenuItem {
            text: qsTr("Rename")
            onTriggered: sidebar.renameRow(sidebar.menuData.project, sidebar.menuData.name)
        }
        MenuItem {
            text: qsTr("Duplicate")
            onTriggered: Qt.callLater(sidebar.library.duplicateCanvas, sidebar.menuData.project, sidebar.menuData.name)
        }
        Menu {
            id: moveMenu
            title: qsTr("Move to")
            enabled: sidebar.library.projectNames.length > 1 && sidebar.menuData.canvasCount > 1
            Instantiator {
                model: sidebar.library.projectNames
                delegate: MenuItem {
                    required property string modelData
                    text: modelData
                    enabled: modelData !== sidebar.menuData.project
                    onTriggered: {
                        const data = sidebar.menuData
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
        MenuItem {
            text: qsTr("Delete…")
            enabled: sidebar.menuData.canvasCount > 1   // a Project keeps at least one Canvas
            onTriggered: sidebar.askToDeleteCanvas(sidebar.menuData.project, sidebar.menuData.name)
        }
    }
}
