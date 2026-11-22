pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import Junction
import JunctionDesign

// One row of the sidebar: a Project (chevron, name, number of Canvases) or one of its Canvases.
//   click: a Canvas opens; a Project opens the Canvas last open in it (and expands)
//   chevron: expand or collapse        double-click: rename
//   drag: reorder, or move a Canvas to another Project        right-click or "...": the menu
Item {
    id: row

    required property int index
    required property bool isProject   // otherwise a Canvas row
    required property string name
    required property string project
    required property int projectIndex
    required property int canvasIndex
    required property int canvasCount
    required property bool expanded
    required property bool current
    required property Sidebar sidebar

    readonly property bool hovered: mouse.containsMouse || plusButton.hovered || moreButton.hovered
    readonly property bool dragged: sidebar.rowDrag.draggedRow !== null && sidebar.rowDrag.draggedRow.isProject === isProject
                                    && sidebar.rowDrag.draggedRow.project === project && sidebar.rowDrag.draggedRow.name === name

    function rename() { nameLabel.edit() }
    function rowData() {
        return { isProject: isProject, name: name, project: project, projectIndex: projectIndex,
                 canvasIndex: canvasIndex, canvasCount: canvasCount }
    }

    width: ListView.view.width
    height: sidebar.rowHeight
    opacity: dragged ? 0.4 : 1

    Rectangle {
        id: background
        anchors.fill: parent
        anchors.leftMargin: row.sidebar.rowInset
        anchors.rightMargin: row.sidebar.rowInset
        color: {
            if (row.current && !row.isProject)
                return Colors.current
            if (row.current && !row.expanded)   // a collapsed Project holding the open Canvas
                return Qt.alpha(Colors.selection, 0.10)
            if ((row.hovered && !row.sidebar.rowDrag.active) || row.sidebar.menuRow === row.index)
                return Colors.hover
            return "transparent"
        }
        Behavior on color { ColorAnimation { duration: Motion.fast } }
        // a collapsed Project a Canvas is being dropped into
        border.width: row.sidebar.rowDrag.drop && row.sidebar.rowDrag.drop.intoRow === row.index ? 1.5 : 0
        border.color: Colors.selection

        // The open Canvas: an accent edge on the left, so "where am I" reads at a glance.
        Rectangle {
            visible: !row.isProject && row.current
            width: 2
            height: parent.height
            color: Colors.selection
        }
    }

    MouseArea {
        id: mouse
        property point pressPoint
        property bool moved: false

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        enabled: !nameLabel.editing
        cursorShape: row.sidebar.rowDrag.active ? Qt.ClosedHandCursor : Qt.ArrowCursor

        onPressed: (event) => {
            row.sidebar.focusCanvas()
            pressPoint = Qt.point(event.x, event.y)
            moved = false
        }
        onPositionChanged: (event) => {
            if (!pressed || !(event.buttons & Qt.LeftButton))
                return
            if (!moved && Math.hypot(event.x - pressPoint.x, event.y - pressPoint.y) > 6
                    && row.sidebar.libraryRows.filter === "") {
                moved = true
                row.sidebar.rowDrag.start(row.rowData())
            }
            if (row.sidebar.rowDrag.active)
                row.sidebar.rowDrag.move(mapToItem(row.sidebar.list, event.x, event.y))
        }
        onReleased: {
            if (row.sidebar.rowDrag.active)
                row.sidebar.rowDrag.end()
        }
        onCanceled: row.sidebar.rowDrag.cancel()
        onClicked: (event) => {
            if (moved)
                return
            if (event.button === Qt.RightButton)
                row.sidebar.openMenu(row, row, event.x, event.y)
            else
                row.sidebar.library.open(row.project, row.isProject ? "" : row.name)
        }
        onDoubleClicked: (event) => {
            if (event.button === Qt.LeftButton)
                row.rename()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: row.isProject ? row.sidebar.rowInset + Space.xs : row.sidebar.canvasIndent
        anchors.rightMargin: row.sidebar.rowInset + Space.xs / 2
        spacing: Space.xs

        // Expands or collapses without opening anything.
        AbstractButton {
            id: chevronButton
            visible: row.isProject
            implicitWidth: row.sidebar.chevronSize
            implicitHeight: row.sidebar.chevronSize
            focusPolicy: Qt.NoFocus
            enabled: row.sidebar.libraryRows.filter === ""
            onClicked: row.sidebar.library.setExpanded(row.project, !row.expanded)
            contentItem: IconImage {
                source: "../icons/chevron.svg"
                sourceSize: Qt.size(Sizes.iconSmall, Sizes.iconSmall)
                fillMode: Image.Pad
                color: Colors.mutedText
                rotation: row.expanded ? 90 : 0
                Behavior on rotation { NumberAnimation { duration: Motion.normal; easing.type: Motion.enter } }
            }
            background: Rectangle {
                color: chevronButton.hovered ? Colors.hover : "transparent"
            }
        }

        IconImage {
            visible: !row.isProject
            source: "../icons/canvas.svg"
            sourceSize: Qt.size(Sizes.icon, Sizes.icon)
            color: row.current ? Colors.accentText : Colors.mutedText
            Layout.rightMargin: Space.xs
        }

        InlineName {
            id: nameLabel
            Layout.fillWidth: true
            text: row.name
            richText: row.sidebar.highlight(row.name)
            font.weight: row.isProject ? Font.DemiBold : Font.Normal
            color: Colors.text
            problemFor: (name) => row.isProject
                ? row.sidebar.library.projectNameProblem(name, row.name)
                : row.sidebar.library.canvasNameProblem(row.project, name, row.name)
            onRenamed: (name) => {
                if (row.isProject)
                    row.sidebar.library.renameProject(row.project, name)
                else
                    row.sidebar.library.renameCanvas(row.project, row.name, name)
            }
            onFinished: row.sidebar.focusCanvas()
        }

        Label {
            visible: row.isProject && !row.hovered && row.sidebar.menuRow !== row.index && !nameLabel.editing
            text: row.canvasCount
            color: Colors.mutedText
            font.pixelSize: Fonts.small
            font.features: { "tnum": 1 }   // digits of equal width, so counts line up
            Layout.rightMargin: Space.sm
        }

        RowButton {
            id: plusButton
            visible: row.isProject && (row.hovered || row.sidebar.menuRow === row.index) && !nameLabel.editing
            iconName: "plus"
            tip: qsTr("New canvas in %1").arg(row.project)
            onClicked: {
                row.sidebar.focusCanvas()
                row.sidebar.library.createCanvas(row.project)
            }
        }

        RowButton {
            id: moreButton
            visible: (row.hovered || row.sidebar.menuRow === row.index) && !nameLabel.editing
            iconName: "more"
            tip: qsTr("More")
            onClicked: row.sidebar.openMenu(row, moreButton, 0, moreButton.height)
        }
    }

    component RowButton: IconButton {
        implicitWidth: Sizes.control - Space.xs
        implicitHeight: Sizes.control - Space.xs
        padding: (implicitWidth - Sizes.icon) / 2
        icon.color: Colors.mutedText
    }
}
