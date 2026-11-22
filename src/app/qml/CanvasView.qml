pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction
import JunctionDesign

// The unbounded canvas: pan and zoom, the layers, and background mouse input.
//
// Coordinates: "world" coordinates are canvas units stored in the file; "screen" coordinates
// are pixels in this item. screen = world * zoom + pan.
//
// Layers, bottom to top: grid, Groups, Connections, Cards, selected Connection handles,
// the label editor.
Item {
    id: canvasView
    required property Editor editor
    required property HitTest hitTest
    required property Library library   // remembers where each Canvas was viewed
    required property AskDialog ask     // the window's questions

    // screen = world * zoom + pan. Read-only outside; the user changes them with panBy() and
    // zoomAt(), the code with placeView().
    readonly property alias zoom: camera.zoom
    readonly property alias pan: camera.pan
    // What the user is doing. Read-only for the items on the canvas: they change it through the
    // functions below (editText(), startConnection(), ...).
    readonly property alias editingNodeId: interaction.editingNodeId       // Card or Group whose text is being edited
    readonly property alias editingLabelId: interaction.editingLabelId     // Connection whose label is being edited
    readonly property alias connectFrom: interaction.connectFrom   // Card a new Connection is being dragged from
    readonly property alias dropTarget: interaction.dropTarget     // Card a dragged Connection end would attach to
    readonly property alias spaceHeld: interaction.spaceHeld       // Space+drag pans
    readonly property real handleRadius: 14 / zoom   // how close to a Handle counts as "on" it
    readonly property real markerMargin: 4 / zoom       // how far around a PointMarker still grabs it
    readonly property real connectionTolerance: 6 / zoom   // how far from a Connection's line still hits it
    // The pointer is directly moving something: panning, a selection rectangle, a new Connection.
    readonly property bool manipulating: background.mode !== BackgroundInput.Idle || connectFrom !== ""

    enum ResizeKeeps { Fit, Center, Nothing }       // what a resize keeps, see keepOnResize

    focus: true
    clip: true

    QtObject {
        id: camera
        property real zoom: 1
        property point pan: Qt.point(0, 0)
    }

    QtObject {
        id: interaction
        property string editingNodeId: ""
        property string editingLabelId: ""
        property string connectFrom: ""
        property string dropTarget: ""
        property int dropSide: CanvasEnums.Side.Floating   // the dropTarget's Handle under the pointer, if any
        property point connectPointer                   // where the new Connection's end is, in world coordinates
        property bool spaceHeld: false
        property point connectPressPoint   // where the new Connection's drag began, in screen coordinates
    }

    // ---- Editing text (called by CardItem, GroupItem and the menus) ----

    function editText(nodeId) { interaction.editingNodeId = nodeId }
    // A Card or Group just made: its text is typed straight away. "" when it couldn't be made.
    function editNew(nodeId) {
        if (nodeId)
            editText(nodeId)
    }
    function stopTextEditing() { interaction.editingNodeId = "" }
    function endTextEditing(nodeId) {
        if (interaction.editingNodeId === nodeId)
            interaction.editingNodeId = ""
    }

    // Shift or Ctrl while clicking adds to / removes from the selection.
    function isToggle(modifiers) { return (modifiers & (Qt.ShiftModifier | Qt.ControlModifier)) !== 0 }

    function toWorld(p) { return Qt.point((p.x - pan.x) / zoom, (p.y - pan.y) / zoom) }
    function toScreen(p) { return Qt.point(p.x * zoom + pan.x, p.y * zoom + pan.y) }
    function pointerWorld() { return toWorld(hover.point.position) }

    // ---- Moving the view ----
    // After opening a Canvas the view goes back to where the user left it, or fits the content
    // if it was never moved. Until the user pans or zooms, a resize (tiling window managers
    // resize right after showing; the sidebar opens and closes) keeps that: it fits again, or
    // keeps the same point in the middle. Every pan and zoom the user makes is remembered.

    property int keepOnResize: CanvasView.Fit   // fit the content, keep keptCenter in the middle, or nothing
    property point keptCenter
    onWidthChanged: Qt.callLater(followResize)
    onHeightChanged: Qt.callLater(followResize)

    function placeView(newZoom, newPan) {
        camera.zoom = newZoom
        camera.pan = newPan
    }

    // The user moved the canvas.
    function panBy(dx, dy) {
        placeView(zoom, Qt.point(pan.x + dx, pan.y + dy))
        userMoved()
    }
    function zoomAt(screenPoint, factor) {
        const world = toWorld(screenPoint)
        const newZoom = Math.max(0.1, Math.min(4, zoom * factor))
        placeView(newZoom, Qt.point(screenPoint.x - world.x * newZoom, screenPoint.y - world.y * newZoom))
        userMoved()
    }
    function userMoved() {
        keepOnResize = CanvasView.Nothing
        rememberView()
    }
    function rememberView() {
        if (width > 0 && height > 0)
            library.rememberCurrentView(toWorld(Qt.point(width / 2, height / 2)), zoom)
    }

    function followResize() {
        if (keepOnResize === CanvasView.Fit)
            fitToContent()
        else if (keepOnResize === CanvasView.Center)
            centerOn(keptCenter)
    }
    function centerOn(point) {
        placeView(zoom, Qt.point(width / 2 - point.x * zoom, height / 2 - point.y * zoom))
    }
    function userFit() {   // the Fit button and Ctrl+1
        fitToContent()
        rememberView()
    }

    // How far a wheel event scrolls, in screen pixels. A mouse wheel has no scroll phase and
    // moves 60 px per notch (its pixelDelta on Wayland is only 15); a touchpad reports exact pixels.
    function wheelDelta(event) {
        if (event.phase === Qt.NoScrollPhase || (event.pixelDelta.x === 0 && event.pixelDelta.y === 0))
            return Qt.point(event.angleDelta.x / 2, event.angleDelta.y / 2)
        return Qt.point(event.pixelDelta.x, event.pixelDelta.y)
    }

    function resetZoom() { zoomAt(Qt.point(width / 2, height / 2), 1 / zoom) }

    function fitToContent() {
        const r = editor.contentBounds()
        if (r.width <= 0) {
            placeView(1, Qt.point(width / 2, height / 2))
            return
        }
        const newZoom = Math.min(width / (r.width + 160), height / (r.height + 160), 1)
        placeView(newZoom, Qt.point(width / 2 - (r.x + r.width / 2) * newZoom, height / 2 - (r.y + r.height / 2) * newZoom))
    }

    // ---- Dragging a new Connection out of a Handle (called by CardItem) ----

    function startConnection(nodeId, screenPoint) {
        forceActiveFocus()
        interaction.connectFrom = nodeId
        interaction.connectPressPoint = screenPoint
    }

    // Where a dragged Connection end would attach at `world`: the topmost Card there (a point
    // just outside its border, on the outer half of a Handle, still counts) other than
    // `except`, and the Handle under the point. {node: ""} where there is none.
    function attachmentAt(world, except) {
        const node = hitTest.cardAt(world, handleRadius)
        if (!node || node === except)
            return { node: "", side: CanvasEnums.Side.Floating }
        return { node: node, side: hitTest.handleAt(node, world, handleRadius) }
    }

    // After Esc (cancelConnection) the rest of the drag does nothing: connectFrom is "".
    function moveConnection(screenPoint) {
        if (!connectFrom)
            return
        const world = toWorld(screenPoint)
        const at = attachmentAt(world, connectFrom)
        interaction.dropTarget = at.node
        interaction.dropSide = at.side
        interaction.connectPointer = world
    }

    function finishConnection(screenPoint) {
        const from = connectFrom
        cancelConnection()
        const moved = Math.hypot(screenPoint.x - interaction.connectPressPoint.x, screenPoint.y - interaction.connectPressPoint.y)
        if (!from || moved < 10)
            return

        const world = toWorld(screenPoint)
        if (hitTest.cardAt(world, handleRadius) === from)
            return   // dropped back on its own Card
        const at = attachmentAt(world, from)
        if (at.node)
            editor.connectNodes(from, at.node, at.side)
        else
            editNew(editor.connectToNewCard(from, world))
    }

    function cancelConnection() {
        interaction.connectFrom = ""
        interaction.dropTarget = ""
    }

    // ---- Connection labels ----

    // Opens the LabelEditor on a Connection.
    function editLabel(connectionId) {
        if (editingLabelId === connectionId)
            return              // already open: keep what has been typed
        forceActiveFocus()      // first ends any other text editing
        editor.select(connectionId)
        interaction.editingLabelId = connectionId
    }
    function endLabelEditing() { interaction.editingLabelId = "" }

    // While a Connection's end is dragged to another Card (ConnectionOverlay), that Card shows
    // its Handles.
    function showDropTarget(nodeId) { interaction.dropTarget = nodeId }

    // Right-clicking a Card, Group, Connection or Waypoint (see CanvasMenus).
    function openMenuFor(id, waypointIndex) { menus.openFor(id, waypointIndex) }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Space && !event.isAutoRepeat) {
            interaction.spaceHeld = true
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            if (connectFrom)
                cancelConnection()
            else
                editor.clearSelection()
            event.accepted = true
        }
    }
    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Space && !event.isAutoRepeat)
            interaction.spaceHeld = false
    }

    Connections {
        target: canvasView.editor
        function onCanvasOpened() {
            canvasView.cancelConnection()
            canvasView.stopTextEditing()
            if (canvasView.editingLabelId) {   // the Connection belonged to the Canvas shown before
                canvasView.endLabelEditing()
                canvasView.forceActiveFocus()
            }
            const saved = canvasView.library.currentView()
            if (saved) {
                canvasView.keepOnResize = CanvasView.Center
                canvasView.keptCenter = saved.center
                canvasView.placeView(saved.zoom, canvasView.pan)
                canvasView.centerOn(canvasView.keptCenter)
            } else {
                canvasView.keepOnResize = CanvasView.Fit
                Qt.callLater(canvasView.fitToContent)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Colors.background
    }

    GridLayer {
        anchors.fill: parent
        zoom: canvasView.zoom
        pan: canvasView.pan
        colors: Colors.forPainter
    }

    BackgroundInput {
        id: background
        anchors.fill: parent
        canvasView: canvasView
        menus: menus
    }

    // Holds items placed in world coordinates: they move and scale with the view.
    component WorldLayer: Item {
        id: world
        required property real zoom
        required property point pan
        transform: [
            Scale { xScale: world.zoom; yScale: world.zoom },
            Translate { x: world.pan.x; y: world.pan.y }
        ]
    }

    WorldLayer {
        zoom: canvasView.zoom
        pan: canvasView.pan
        Repeater {
            id: groupRepeater
            model: canvasView.editor.groups
            delegate: GroupItem { canvasView: canvasView }
        }
    }

    ConnectionLayer {
        anchors.fill: parent
        editor: canvasView.editor
        editedLabelConnection: canvasView.editingLabelId
        connectFrom: canvasView.connectFrom
        connectPointer: interaction.connectPointer
        dropTarget: canvasView.dropTarget
        dropSide: interaction.dropSide
        zoom: canvasView.zoom
        pan: canvasView.pan
        colors: Colors.forPainter
    }

    WorldLayer {
        zoom: canvasView.zoom
        pan: canvasView.pan
        Repeater {
            id: cardRepeater
            model: canvasView.editor.cards
            delegate: CardItem { canvasView: canvasView }
        }
        ConnectionOverlay { canvasView: canvasView }
        LabelEditor { canvasView: canvasView }
    }

    Rectangle {
        visible: background.mode === BackgroundInput.Selecting
        x: background.selectionRect.x
        y: background.selectionRect.y
        width: background.selectionRect.width
        height: background.selectionRect.height
        color: Qt.alpha(Colors.selection, 0.12)
        border.color: Colors.selection
    }

    CanvasMenus {
        id: menus
        anchors.fill: parent
        canvasView: canvasView
        ask: canvasView.ask
    }

    HoverHandler { id: hover }

    // Touchpad pinch zooms around the fingers.
    PinchHandler {
        target: null
        onScaleChanged: (delta) => canvasView.zoomAt(centroid.position, delta)
    }

    Text {
        anchors.centerIn: parent
        visible: cardRepeater.count === 0 && groupRepeater.count === 0
        text: qsTr("Double-click anywhere to add a card")
        color: Colors.mutedText
        font.pixelSize: Fonts.title
    }
}
