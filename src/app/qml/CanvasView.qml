pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Junction

// The unbounded canvas: pan and zoom, the layers, and background mouse input.
//
// Coordinates: "world" coordinates are canvas units stored in the file; "screen" coordinates
// are pixels in this item. screen = world * zoom + pan.
//
// Layers, bottom to top: grid, Groups, Connections, Cards, selected Connection handles.
Item {
    id: view
    required property Editor editor

    property real zoom: 1
    property point pan: Qt.point(0, 0)
    property string editingId: ""        // Card or Group whose text is being edited
    property string connectFrom: ""      // Card a new Connection is being dragged from
    property string dropTarget: ""       // node under the pointer while dragging a Connection
    property bool connectCancelled: false
    property point connectPress
    property bool spaceHeld: false
    readonly property real handleRadius: 14 / zoom   // how close to a Handle counts as "on" it

    focus: true
    clip: true

    // Shift or Ctrl while clicking adds to / removes from the selection.
    function isToggle(modifiers) { return (modifiers & (Qt.ShiftModifier | Qt.ControlModifier)) !== 0 }

    function toWorld(p) { return Qt.point((p.x - pan.x) / zoom, (p.y - pan.y) / zoom) }
    function toScreen(p) { return Qt.point(p.x * zoom + pan.x, p.y * zoom + pan.y) }
    function pointerWorld() { return toWorld(hover.point.position) }

    function zoomAt(screenPoint, factor) {
        const world = toWorld(screenPoint)
        zoom = Math.max(0.1, Math.min(4, zoom * factor))
        pan = Qt.point(screenPoint.x - world.x * zoom, screenPoint.y - world.y * zoom)
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
        fitting = true
        const r = editor.contentBounds()
        if (r.width <= 0) {
            zoom = 1
            pan = Qt.point(width / 2, height / 2)
        } else {
            zoom = Math.min(width / (r.width + 160), height / (r.height + 160), 1)
            pan = Qt.point(width / 2 - (r.x + r.width / 2) * zoom, height / 2 - (r.y + r.height / 2) * zoom)
        }
        fitting = false
    }

    // ---- Dragging a new Connection out of a Handle (called by CardItem) ----

    function startConnection(nodeId, screenPoint) {
        forceActiveFocus()
        connectFrom = nodeId
        connectPress = screenPoint
        connectCancelled = false
    }

    // The card a dragged Connection would attach to. A point just outside a card's border
    // (on the outer half of a Handle) still counts.
    function connectionTarget(world) {
        const target = editor.cardAt(world, handleRadius)
        return target === connectFrom ? "" : target
    }

    function moveConnection(screenPoint) {
        if (connectCancelled)
            return
        const world = toWorld(screenPoint)
        dropTarget = connectionTarget(world)
        editor.setPreview(connectFrom, world, dropTarget,
                          dropTarget ? editor.handleAt(dropTarget, world, handleRadius) : "")
    }

    function finishConnection(screenPoint) {
        const from = connectFrom
        editor.clearPreview()
        connectFrom = ""
        dropTarget = ""
        const moved = Math.hypot(screenPoint.x - connectPress.x, screenPoint.y - connectPress.y)
        if (connectCancelled || moved < 10)
            return

        const world = toWorld(screenPoint)
        if (editor.cardAt(world, handleRadius) === from)
            return
        const target = connectionTarget(world)
        if (target)
            editor.connectNodes(from, target, editor.handleAt(target, world, handleRadius))
        else
            editingId = editor.connectToNewCard(from, world)
    }

    function cancelConnection() {
        connectFrom = ""
        connectCancelled = true
        dropTarget = ""
        editor.clearPreview()
    }

    // ---- Connection labels ----

    function editLabel(connectionId) {
        editor.select(connectionId)
        const info = editor.selectedConnection
        labelEditor.connectionId = connectionId
        labelEditor.worldPosition = info.labelPosition
        labelEditor.text = info.label
        labelEditor.visible = true
        labelEditor.forceActiveFocus()
        labelEditor.selectAll()
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Space && !event.isAutoRepeat) {
            spaceHeld = true
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
            spaceHeld = false
    }

    // After opening a file the view fits the content, and keeps fitting while the window is
    // being resized (tiling window managers resize right after showing), until the user
    // pans or zooms.
    property bool autoFit: true
    onPanChanged: if (!fitting) autoFit = false
    property bool fitting: false
    onWidthChanged: if (autoFit) Qt.callLater(fitToContent)
    onHeightChanged: if (autoFit) Qt.callLater(fitToContent)

    Connections {
        target: view.editor
        function onDocumentLoaded() {
            view.autoFit = true
            Qt.callLater(view.fitToContent)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    GridLayer {
        anchors.fill: parent
        zoom: view.zoom
        pan: view.pan
        colors: Theme.palette
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

        onPressed: (mouse) => {
            view.forceActiveFocus()
            last = Qt.point(mouse.x, mouse.y)
            if (mouse.button === Qt.MiddleButton || view.spaceHeld) {
                mode = "pan"
                return
            }
            if (mouse.button === Qt.RightButton) {
                contextMenu.at = view.toWorld(last)
                contextMenu.popup()
                return
            }
            const additive = view.isToggle(mouse.modifiers)
            const hit = view.editor.connectionAt(view.toWorld(last), 6 / view.zoom)
            if (hit) {
                view.editor.select(hit, additive)
                mode = ""
                return
            }
            if (!additive)
                view.editor.clearSelection()
            mode = "select"
            rubberStart = last
        }

        onPositionChanged: (mouse) => {
            if (!pressed) {
                overConnection = view.editor.connectionAt(view.toWorld(Qt.point(mouse.x, mouse.y)), 6 / view.zoom) !== ""
            } else if (mode === "pan") {
                view.pan = Qt.point(view.pan.x + mouse.x - last.x, view.pan.y + mouse.y - last.y)
                last = Qt.point(mouse.x, mouse.y)
            } else if (mode === "select") {
                rubberBand.x = Math.min(rubberStart.x, mouse.x)
                rubberBand.y = Math.min(rubberStart.y, mouse.y)
                rubberBand.width = Math.abs(mouse.x - rubberStart.x)
                rubberBand.height = Math.abs(mouse.y - rubberStart.y)
                rubberBand.visible = true
            }
        }

        onReleased: (mouse) => {
            if (mode === "select" && rubberBand.visible) {
                const a = view.toWorld(Qt.point(rubberBand.x, rubberBand.y))
                view.editor.selectInRect(Qt.rect(a.x, a.y, rubberBand.width / view.zoom,
                                                   rubberBand.height / view.zoom),
                                           view.isToggle(mouse.modifiers))
            }
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
                view.editingId = view.editor.createCard(world)
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
            view.pan = Qt.point(view.pan.x + dx, view.pan.y + dy)
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
        zoom: view.zoom
        pan: view.pan
        colors: Theme.palette
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
    }

    Rectangle {
        id: rubberBand
        visible: false
        color: Qt.alpha(Theme.selection, 0.12)
        border.color: Theme.selection
    }

    TextField {
        id: labelEditor
        property string connectionId
        property point worldPosition   // follows pan and zoom while open
        visible: false
        x: view.toScreen(worldPosition).x - width / 2
        y: view.toScreen(worldPosition).y - height / 2
        width: 200
        placeholderText: qsTr("Label")
        horizontalAlignment: TextInput.AlignHCenter
        onAccepted: view.forceActiveFocus()
        onActiveFocusChanged: {
            if (!activeFocus && visible) {
                view.editor.setConnectionLabel(connectionId, text)
                visible = false
            }
        }
        Keys.onEscapePressed: view.forceActiveFocus()
    }

    Menu {
        id: contextMenu
        property point at
        MenuItem { text: qsTr("New card"); onTriggered: view.editingId = view.editor.createCard(contextMenu.at) }
        // With something selected this groups it; otherwise it adds an empty Group here.
        MenuItem {
            text: view.editor.selection.length > 0 ? qsTr("Group selection") : qsTr("New group")
            onTriggered: view.editingId = view.editor.createGroup(contextMenu.at)
        }
        MenuSeparator {}
        MenuItem { text: qsTr("Paste"); onTriggered: view.editor.paste(contextMenu.at) }
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
        color: Theme.mutedText
        font.pixelSize: 16
    }
}
