pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import Junction
import JunctionDesign

// Dragging sidebar rows to reorder them: a Project among Projects, a Canvas within its Project
// or into another. SidebarRow calls start(), move() and end(). Meanwhile this shows where the
// row would land and the row itself following the pointer, and scrolls the list near its edges.
//
// `drop` is where the dragged row would land, or null, always with the same fields:
//   lineY    the line drawn between rows (list content coordinates), or -1 for none
//   intoRow  the collapsed Project row it would go into (shown outlined), or -1
//   noOp     it would put the row back where it is
//   move()   moves it there
Item {
    id: rowDrag
    required property Library library
    required property LibraryRows libraryRows
    required property ListView list
    required property real canvasIndent   // where a Canvas row's name starts (see Sidebar)
    required property real projectIndent  // ...and a Project row's

    property var draggedRow: null   // the dragged row's roles (LibraryRows.rowAt())
    readonly property bool active: draggedRow !== null
    property var drop: null
    property point point      // the pointer, in list coordinates

    function start(rowData) { draggedRow = rowData }

    // For drawing the rows: the one being dragged, and a collapsed Project it would go into.
    function isDragged(isProject, project, name) {
        return draggedRow !== null && draggedRow.isProject === isProject
            && draggedRow.project === project && draggedRow.name === name
    }
    function dropsInto(rowIndex) { return drop !== null && drop.intoRow === rowIndex }

    function move(pointInList) {
        point = pointInList
        drop = dropAt(point.y + list.contentY)
    }

    function end() {
        const target = drop
        cancel()
        if (target && !target.noOp)
            Qt.callLater(target.move)
    }

    function cancel() {
        draggedRow = null
        drop = null
    }

    // The drop for a point `y` in list content coordinates, or null.
    function dropAt(y) {
        const count = list.count
        if (count === 0)
            return null
        let i = list.indexAt(list.width / 2, y)
        if (i < 0) {   // above the first row or below the last one
            const last = list.itemAtIndex(count - 1)
            i = last && y >= last.y ? count - 1 : 0
        }
        const rowItem = list.itemAtIndex(i)
        if (!rowItem)
            return null
        const row = libraryRows.rowAt(i)
        return draggedRow.isProject ? projectDrop(row, y) : canvasDrop(row, i, rowItem, y)
    }

    // A Canvas lands before or after the Canvas row under the pointer, whichever half it is in;
    // on a Project row, at the top of an open Project or the end of a collapsed one.
    function canvasDrop(row, rowIndex, rowItem, y) {
        const dragged = draggedRow
        const drop = (index, lineY, intoRow) => ({
            lineY: lineY,
            intoRow: intoRow,
            noOp: row.project === dragged.project && (index === dragged.canvasIndex || index === dragged.canvasIndex + 1),
            move: () => library.moveCanvas(dragged.project, dragged.name, row.project, index)
        })
        if (!row.isProject) {
            const after = y >= rowItem.y + rowItem.height / 2
            return drop(row.canvasIndex + (after ? 1 : 0), rowItem.y + (after ? rowItem.height : 0), -1)
        }
        if (row.expanded)
            return drop(0, rowItem.y + rowItem.height, -1)
        return drop(row.canvasCount, -1, rowIndex)
    }

    // A Project lands before or after the whole Project under the pointer, whichever half it is in.
    function projectDrop(row, y) {
        const top = list.itemAtIndex(libraryRows.rowOf(row.project))
        const bottom = list.itemAtIndex(libraryRows.lastRowOf(row.project))
        if (!top || !bottom)
            return null
        const before = y < (top.y + bottom.y + bottom.height) / 2
        const index = row.projectIndex + (before ? 0 : 1)
        const dragged = draggedRow
        return {
            lineY: before ? top.y : bottom.y + bottom.height,
            intoRow: -1,
            noOp: index === dragged.projectIndex || index === dragged.projectIndex + 1,
            move: () => library.moveProject(dragged.project, index)
        }
    }

    // Dragging near the top or bottom edge scrolls.
    Timer {
        interval: 16
        repeat: true
        running: rowDrag.active && rowDrag.list.contentHeight > rowDrag.list.height
                 && (rowDrag.point.y < 32 || rowDrag.point.y > rowDrag.list.height - 32)
        onTriggered: {
            const list = rowDrag.list
            const y = rowDrag.point.y
            const speed = Math.max(-16, Math.min(16, y < 32 ? -(32 - y) / 2 : (y - list.height + 32) / 2))
            list.contentY = Math.max(list.originY, Math.min(list.contentY + speed,
                                                            list.originY + list.contentHeight - list.height))
            rowDrag.move(rowDrag.point)
        }
    }

    // Where the dragged row would land.
    Rectangle {
        visible: rowDrag.active && rowDrag.drop !== null && !rowDrag.drop.noOp && rowDrag.drop.lineY >= 0
        x: rowDrag.draggedRow && !rowDrag.draggedRow.isProject ? rowDrag.canvasIndent : rowDrag.projectIndent
        y: rowDrag.list.y + (rowDrag.drop ? rowDrag.drop.lineY : 0) - rowDrag.list.contentY - 1
        width: rowDrag.width - x - Space.md
        height: 2
        color: Colors.selection
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: -width + 1
            width: Space.sm
            height: Space.sm
            color: Colors.sidebar
            border.width: 2
            border.color: Colors.selection
        }
    }

    // The dragged row, following the pointer (floating: it is lifted off the list).
    Floating {
        visible: rowDrag.active
        x: Math.min(rowDrag.list.x + rowDrag.point.x + Space.md, rowDrag.width - width - Space.xs)
        y: rowDrag.list.y + rowDrag.point.y - height / 2
        width: Math.min(ghostRow.implicitWidth + 2 * Space.sm, rowDrag.width - Space.xl)
        height: Sizes.control
        border.color: Colors.selection
        RowLayout {
            id: ghostRow
            anchors.fill: parent
            anchors.leftMargin: Space.sm
            anchors.rightMargin: Space.sm
            spacing: Space.sm
            IconImage {
                source: rowDrag.draggedRow && !rowDrag.draggedRow.isProject ? "../icons/canvas.svg" : "../icons/project.svg"
                sourceSize: Qt.size(Sizes.icon, Sizes.icon)
                color: Colors.mutedText
            }
            Label {
                Layout.fillWidth: true
                text: rowDrag.draggedRow ? rowDrag.draggedRow.name : ""
                elide: Text.ElideRight
            }
        }
    }
}
