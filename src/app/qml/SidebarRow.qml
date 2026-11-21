pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import Junction

// One row of the sidebar: a Project (chevron, name, number of Canvases) or one of its Canvases.
//   click: a Canvas opens; a Project opens the Canvas last open in it (and expands)
//   chevron: expand or collapse        double-click: rename
//   drag: reorder, or move a Canvas to another Project        right-click or "...": the menu
Item {
    id: row

    required property int index
    required property string kind
    required property string name
    required property string project
    required property int projectIndex
    required property int canvasIndex
    required property int canvasCount
    required property bool expanded
    required property bool current
    required property Sidebar sidebar

    readonly property bool isProject: kind === "project"
    readonly property bool hovered: mouse.containsMouse || plusButton.hovered || moreButton.hovered
    readonly property bool dragged: sidebar.dragItem !== null && sidebar.dragItem.kind === kind
                                    && sidebar.dragItem.project === project && sidebar.dragItem.name === name

    function rename() { nameLabel.edit() }
    function rowData() {
        return { kind: kind, name: name, project: project, projectIndex: projectIndex,
                 canvasIndex: canvasIndex, canvasCount: canvasCount }
    }

    width: ListView.view.width
    height: sidebar.rowHeight
    opacity: dragged ? 0.4 : 1

    Rectangle {
        id: background
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        radius: 5
        color: !row.isProject && row.current ? Theme.rowCurrent
             // a collapsed Project holding the open Canvas
             : row.isProject && row.current && !row.expanded ? Qt.alpha(Theme.selection, 0.10)
             : (row.hovered && !row.sidebar.dragging) || row.sidebar.menuRow === row.index ? Theme.rowHover
             : "transparent"
        // a collapsed Project a Canvas is being dropped into
        border.width: row.sidebar.drop && row.sidebar.drop.intoRow === row.index ? 1.5 : 0
        border.color: Theme.selection
    }

    MouseArea {
        id: mouse
        property point pressPoint
        property bool moved: false

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        enabled: !nameLabel.editing
        cursorShape: row.sidebar.dragging ? Qt.ClosedHandCursor : Qt.ArrowCursor

        onPressed: (event) => {
            row.sidebar.focusCanvas()
            pressPoint = Qt.point(event.x, event.y)
            moved = false
        }
        onPositionChanged: (event) => {
            if (!pressed || !(event.buttons & Qt.LeftButton))
                return
            if (!moved && Math.hypot(event.x - pressPoint.x, event.y - pressPoint.y) > 6
                    && row.sidebar.library.filter === "") {
                moved = true
                row.sidebar.startDrag(row.rowData())
            }
            if (row.sidebar.dragging)
                row.sidebar.moveDrag(mapToItem(row.sidebar.list, event.x, event.y))
        }
        onReleased: {
            if (row.sidebar.dragging)
                row.sidebar.endDrag()
        }
        onCanceled: row.sidebar.cancelDrag()
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
        anchors.fill: background
        anchors.leftMargin: row.isProject ? 2 : 22
        anchors.rightMargin: 4
        spacing: 4

        // Expands or collapses without opening anything.
        AbstractButton {
            id: chevronButton
            visible: row.isProject
            implicitWidth: 20
            implicitHeight: 20
            focusPolicy: Qt.NoFocus
            enabled: row.sidebar.library.filter === ""
            onClicked: row.sidebar.library.setExpanded(row.project, !row.expanded)
            contentItem: IconImage {
                source: "../icons/chevron.svg"
                sourceSize: Qt.size(14, 14)
                color: Theme.mutedText
                rotation: row.expanded ? 90 : 0
                Behavior on rotation { NumberAnimation { duration: 120 } }
            }
            background: Rectangle {
                radius: 4
                color: chevronButton.hovered ? Theme.rowHover : "transparent"
            }
        }

        IconImage {
            visible: !row.isProject
            source: "../icons/canvas.svg"
            sourceSize: Qt.size(15, 15)
            color: row.current ? Theme.selection : Theme.mutedText
            Layout.rightMargin: 3
        }

        InlineName {
            id: nameLabel
            Layout.fillWidth: true
            text: row.name
            richText: row.sidebar.highlight(row.name)
            font.weight: row.isProject ? Font.DemiBold : Font.Normal
            color: Theme.text
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
            color: Theme.mutedText
            font.pixelSize: 11
            Layout.rightMargin: 6
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

    component RowButton: ToolButton {
        property string iconName
        property string tip
        implicitWidth: 22
        implicitHeight: 22
        padding: 0
        focusPolicy: Qt.NoFocus
        display: AbstractButton.IconOnly
        icon.source: "../icons/" + iconName + ".svg"
        icon.width: 15
        icon.height: 15
        icon.color: Theme.mutedText
        ToolTip.visible: hovered
        ToolTip.delay: 500
        ToolTip.text: tip
    }
}
