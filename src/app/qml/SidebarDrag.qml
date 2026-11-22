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
// `drop` describes where the dragged row would land:
//   a Canvas: {project, index} plus lineY (a line between rows) or intoRow (a collapsed
//   Project, shown outlined); a Project: {projectIndex, lineY}. lineY is in list content
//   coordinates. `noOp` drops would put the row back where it is.
Item {
    id: rowDrag
    required property LibraryModel library
    required property ListView list
    required property real canvasIndent   // where a Canvas row's name starts (see Sidebar)
    required property real projectIndent  // ...and a Project row's

    property var item: null   // the dragged row's data (SidebarRow.rowData())
    readonly property bool active: item !== null
    property var drop: null
    property point point      // the pointer, in list coordinates

    function start(data) { item = data }

    function move(pointInList) {
        point = pointInList
        drop = dropAt(point.y + list.contentY)
    }

    function end() {
        const dragged = item
        const target = drop
        cancel()
        if (!dragged || !target || target.noOp)
            return
        Qt.callLater(() => {
            if (dragged.kind === "canvas")
                library.moveCanvas(dragged.project, dragged.name, target.project, target.index)
            else
                library.moveProject(dragged.project, target.projectIndex)
        })
    }

    function cancel() {
        item = null
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
        const row = library.rowAt(i)
        return item.kind === "canvas" ? canvasDrop(row, i, rowItem, y) : projectDrop(row, y)
    }

    // A Canvas lands before or after the Canvas row under the pointer, whichever half it is in;
    // on a Project row, at the top of an open Project or the end of a collapsed one.
    function canvasDrop(row, rowIndex, rowItem, y) {
        let target
        if (row.kind === "canvas") {
            const after = y >= rowItem.y + rowItem.height / 2
            target = { project: row.project, index: row.canvasIndex + (after ? 1 : 0),
                       lineY: rowItem.y + (after ? rowItem.height : 0) }
        } else if (row.expanded) {
            target = { project: row.project, index: 0, lineY: rowItem.y + rowItem.height }
        } else {
            target = { project: row.project, index: row.canvasCount, intoRow: rowIndex }
        }
        target.noOp = target.project === item.project
            && (target.index === item.canvasIndex || target.index === item.canvasIndex + 1)
        return target
    }

    // A Project lands before or after the whole Project under the pointer, whichever half it is in.
    function projectDrop(row, y) {
        const top = list.itemAtIndex(library.rowOf(row.project))
        const bottom = list.itemAtIndex(library.lastRowOf(row.project))
        if (!top || !bottom)
            return null
        const before = y < (top.y + bottom.y + bottom.height) / 2
        const target = { projectIndex: row.projectIndex + (before ? 0 : 1),
                         lineY: before ? top.y : bottom.y + bottom.height }
        target.noOp = target.projectIndex === item.projectIndex || target.projectIndex === item.projectIndex + 1
        return target
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
        visible: rowDrag.active && rowDrag.drop !== null && !rowDrag.drop.noOp && rowDrag.drop.lineY !== undefined
        x: rowDrag.item && rowDrag.item.kind === "canvas" ? rowDrag.canvasIndent : rowDrag.projectIndent
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
                source: rowDrag.item && rowDrag.item.kind === "canvas" ? "../icons/canvas.svg" : "../icons/project.svg"
                sourceSize: Qt.size(Sizes.icon, Sizes.icon)
                color: Colors.mutedText
            }
            Label {
                Layout.fillWidth: true
                text: rowDrag.item ? rowDrag.item.name : ""
                elide: Text.ElideRight
            }
        }
    }
}
