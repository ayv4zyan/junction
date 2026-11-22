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
    id: view
    required property Editor editor
    required property Library library   // remembers where each Canvas was viewed
    required property AskDialog ask     // the window's questions

    // screen = world * zoom + pan. Read-only outside; the user changes them with panBy() and
    // zoomAt(), the code with placeView().
    readonly property alias zoom: camera.zoom
    readonly property alias pan: camera.pan
    // What the user is doing. Read-only for the items on the canvas: they change it through the
    // functions below (editText(), startConnection(), ...).
    readonly property alias editingId: interaction.editingId       // Card or Group whose text is being edited
    readonly property alias labelingId: interaction.labelingId     // Connection whose label is being edited
    readonly property alias connectFrom: interaction.connectFrom   // Card a new Connection is being dragged from
    readonly property alias dropTarget: interaction.dropTarget     // Card a dragged Connection end would attach to
    readonly property alias spaceHeld: interaction.spaceHeld       // Space+drag pans
    readonly property real handleRadius: 14 / zoom   // how close to a Handle counts as "on" it
    // The pointer is directly moving something: panning, a selection rectangle, a new Connection.
    readonly property bool manipulating: background.mode === "pan" || rubberBand.visible || connectFrom !== ""

    focus: true
    clip: true

    QtObject {
        id: camera
        property real zoom: 1
        property point pan: Qt.point(0, 0)
    }

    QtObject {
        id: interaction
        property string editingId: ""
        property string labelingId: ""
        property string connectFrom: ""
        property string dropTarget: ""
        property bool spaceHeld: false
        property bool connectCancelled: false   // Esc while dragging out a new Connection
        property point connectPress             // where that drag began, in screen coordinates
    }

    // ---- Editing text (called by CardItem, GroupItem and the menus) ----

    function editText(nodeId) { interaction.editingId = nodeId }
    function endTextEditing(nodeId) {
        if (interaction.editingId === nodeId)
            interaction.editingId = ""
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

    property string keepOnResize: "fit"   // "fit" the content, the "center" point, or "nothing"
    property point keptCenter             // the world point kept in the middle for "center"
    onWidthChanged: Qt.callLater(followResize)
    onHeightChanged: Qt.callLater(followResize)

    function placeView(newZoom, newPan) {
        camera.zoom = newZoom
        camera.pan = newPan
    }

    // The user moved the view.
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
        keepOnResize = "nothing"
        rememberView()
    }
    function rememberView() {
        if (width > 0 && height > 0)
            library.rememberCurrentView(toWorld(Qt.point(width / 2, height / 2)), zoom)
    }

    function followResize() {
        if (keepOnResize === "fit")
            fitToContent()
        else if (keepOnResize === "center")
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
        interaction.connectPress = screenPoint
        interaction.connectCancelled = false
    }

    // The card a dragged Connection would attach to. A point just outside a card's border
    // (on the outer half of a Handle) still counts.
    function connectionTarget(world) {
        const target = editor.cardAt(world, handleRadius)
        return target === connectFrom ? "" : target
    }

    function moveConnection(screenPoint) {
        if (interaction.connectCancelled)
            return
        const world = toWorld(screenPoint)
        interaction.dropTarget = connectionTarget(world)
        editor.setConnectionPreview(connectFrom, world, dropTarget,
                                    dropTarget ? editor.handleAt(dropTarget, world, handleRadius) : "")
    }

    function finishConnection(screenPoint) {
        const from = connectFrom
        editor.clearConnectionPreview()
        interaction.connectFrom = ""
        interaction.dropTarget = ""
        const moved = Math.hypot(screenPoint.x - interaction.connectPress.x, screenPoint.y - interaction.connectPress.y)
        if (interaction.connectCancelled || moved < 10)
            return

        const world = toWorld(screenPoint)
        if (editor.cardAt(world, handleRadius) === from)
            return
        const target = connectionTarget(world)
        if (target)
            editor.connectNodes(from, target, editor.handleAt(target, world, handleRadius))
        else
            editText(editor.connectToNewCard(from, world))
    }

    function cancelConnection() {
        interaction.connectFrom = ""
        interaction.connectCancelled = true
        interaction.dropTarget = ""
        editor.clearConnectionPreview()
    }

    // ---- Connection labels ----

    // Opens the LabelEditor on a Connection.
    function editLabel(connectionId) {
        if (labelingId === connectionId)
            return              // already open: keep what has been typed
        forceActiveFocus()      // first ends any other text editing
        editor.select(connectionId)
        interaction.labelingId = connectionId
    }
    function endLabelEditing() { interaction.labelingId = "" }

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
        target: view.editor
        function onCanvasOpened() {
            view.editText("")
            if (view.labelingId) {   // the Connection belonged to the Canvas shown before
                view.endLabelEditing()
                view.forceActiveFocus()
            }
            const saved = view.library.currentView()
            if (saved) {
                view.keepOnResize = "center"
                view.keptCenter = saved.center
                view.placeView(saved.zoom, view.pan)
                view.centerOn(view.keptCenter)
            } else {
                view.keepOnResize = "fit"
                Qt.callLater(view.fitToContent)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Colors.background
    }

    GridLayer {
        anchors.fill: parent
        zoom: view.zoom
        pan: view.pan
        colors: Colors.forPainter
    }

    // Clicks that hit no Card: pan, rubber-band selection, selecting Connections,
    // double-click to create a Card, wheel to scroll and zoom.
    MouseArea {
        id: background
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        hoverEnabled: true
        cursorShape: mode === "pan" ? Qt.ClosedHandCursor
                   : view.spaceHeld ? Qt.OpenHandCursor
                   : overConnection ? Qt.PointingHandCursor : Qt.ArrowCursor
        property string mode: ""   // "pan", "select" or ""
        property bool overConnection: false
        property point last
        property point rubberStart
        property var rubberBase: []   // what stays selected besides the rectangle's catch (Shift/Ctrl)

        onPressed: (mouse) => {
            view.forceActiveFocus()
            last = Qt.point(mouse.x, mouse.y)
            if (mouse.button === Qt.MiddleButton || view.spaceHeld) {
                mode = "pan"
                return
            }
            const hit = view.editor.connectionAt(view.toWorld(last), 6 / view.zoom)
            if (mouse.button === Qt.RightButton) {
                if (hit) {
                    view.openMenuFor(hit)
                } else {
                    view.editor.clearSelection()
                    menus.openAt(view.toWorld(last))
                }
                return
            }
            const additive = view.isToggle(mouse.modifiers)
            if (hit) {
                view.editor.select(hit, additive)
                mode = ""
                return
            }
            if (!additive)
                view.editor.clearSelection()
            mode = "select"
            rubberStart = last
            rubberBase = view.editor.selection
        }

        onPositionChanged: (mouse) => {
            if (!pressed) {
                overConnection = view.editor.connectionAt(view.toWorld(Qt.point(mouse.x, mouse.y)), 6 / view.zoom) !== ""
            } else if (mode === "pan") {
                view.panBy(mouse.x - last.x, mouse.y - last.y)
                last = Qt.point(mouse.x, mouse.y)
            } else if (mode === "select") {
                rubberBand.x = Math.min(rubberStart.x, mouse.x)
                rubberBand.y = Math.min(rubberStart.y, mouse.y)
                rubberBand.width = Math.abs(mouse.x - rubberStart.x)
                rubberBand.height = Math.abs(mouse.y - rubberStart.y)
                rubberBand.visible = true
                // The selection follows the rectangle as it is dragged, not only on release.
                const a = view.toWorld(Qt.point(rubberBand.x, rubberBand.y))
                view.editor.selectInRect(Qt.rect(a.x, a.y, rubberBand.width / view.zoom,
                                                   rubberBand.height / view.zoom), rubberBase)
            }
        }

        onReleased: {
            rubberBand.visible = false
            mode = ""
        }

        onDoubleClicked: (mouse) => {
            if (mouse.button !== Qt.LeftButton)
                return
            const world = view.toWorld(Qt.point(mouse.x, mouse.y))
            const hit = view.editor.connectionAt(world, 6 / view.zoom)
            if (hit)
                view.editLabel(hit)
            else
                view.editText(view.editor.createCard(world))
        }

        onWheel: (wheel) => {
            if (wheel.modifiers & Qt.ControlModifier) {
                view.zoomAt(Qt.point(wheel.x, wheel.y), Math.pow(1.0015, wheel.angleDelta.y))
                return
            }
            const delta = view.wheelDelta(wheel)
            let dx = delta.x
            let dy = delta.y
            if ((wheel.modifiers & Qt.ShiftModifier) && dx === 0) {
                dx = dy
                dy = 0
            }
            view.panBy(dx, dy)
        }
    }

    Item {
        transform: [
            Scale { xScale: view.zoom; yScale: view.zoom },
            Translate { x: view.pan.x; y: view.pan.y }
        ]
        Repeater {
            id: groupRepeater
            model: view.editor.groups
            delegate: GroupItem { canvas: view }
        }
    }

    ConnectionLayer {
        anchors.fill: parent
        editor: view.editor
        labelEditedOn: view.labelingId
        zoom: view.zoom
        pan: view.pan
        colors: Colors.forPainter
    }

    Item {
        transform: [
            Scale { xScale: view.zoom; yScale: view.zoom },
            Translate { x: view.pan.x; y: view.pan.y }
        ]
        Repeater {
            id: cardRepeater
            model: view.editor.cards
            delegate: CardItem { canvas: view }
        }
        ConnectionOverlay { canvas: view }
        LabelEditor { canvas: view }
    }

    Rectangle {
        id: rubberBand
        visible: false
        color: Qt.alpha(Colors.selection, 0.12)
        border.color: Colors.selection
    }

    CanvasMenus {
        id: menus
        anchors.fill: parent
        view: view
        ask: view.ask
    }

    HoverHandler { id: hover }

    // Touchpad pinch zooms around the fingers.
    PinchHandler {
        target: null
        onScaleChanged: (delta) => view.zoomAt(centroid.position, delta)
    }

    Text {
        anchors.centerIn: parent
        visible: cardRepeater.count === 0 && groupRepeater.count === 0
        text: qsTr("Double-click anywhere to add a card")
        color: Colors.mutedText
        font.pixelSize: Fonts.title
    }
}
