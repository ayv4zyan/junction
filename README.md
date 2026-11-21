# Junction

A desktop canvas for boxes-and-arrows thinking (software architecture, plans), combining
Obsidian Canvas's quick card-and-connection workflow with tldraw-style control over how
connections run. Files are [JSON Canvas](https://jsoncanvas.org), so Obsidian canvases open
here and the other way round. Terms are defined in [GLOSSARY.md](GLOSSARY.md); decisions
in [docs/adr](docs/adr).

## Build

Requires Qt 6.8+ (Core, Gui, Svg, Quick, QuickControls2, Test), CMake and a C++20 compiler.

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build          # core unit tests
./build/src/app/junction [file.canvas]
```

## Using it

| Do | Gesture |
|---|---|
| New card | double-click empty space (or paste text from anywhere) |
| Select several | Shift/Ctrl+click, or drag a rectangle over them |
| Edit card / group label | double-click it (Esc or click away to finish) |
| Scroll long card text | mouse wheel over the card (cards whose text fits let the canvas scroll) |
| Connect | hover a card, drag from a Handle onto another card |
| Connect to a new card | drag from a Handle into empty space |
| Pin a side | drop the connection on a Handle (otherwise ends float) |
| Add a Waypoint | select a connection, drag a hollow midpoint dot |
| Remove a Waypoint | double-click it |
| Re-attach an end | select a connection, drag its end dot onto a card or Handle |
| Label a connection | double-click it (or its middle dot when selected) |
| Pan | scroll, middle-drag or Space+drag |
| Zoom | Ctrl+scroll; Ctrl+0 resets, Ctrl+1 fits |
| Resize | drag any side or corner of a card or group |
| Group | Ctrl+G or right-click → Group selection (with nothing selected: an empty group) |
| Nudge | arrow keys move the selection one grid step |
| Snap | toolbar toggle; hold Alt while dragging for the opposite |
| Rename the canvas | click its name in the toolbar (Enter to save, Esc to cancel) |

Undo/redo, copy/paste, Ctrl+D duplicate (a Group comes with its cards), Delete. Canvases save
automatically; new ones live in `~/Documents/Junction` until "Save as" moves them. Export PNG/SVG from the ☰ menu (exports the
selection, or everything when nothing is selected).

## Code layout

- `src/core` — no QML: the model (`CanvasItems`, `Document`), every edit as a `Change`
  inside a `Transaction` (undo, and later network sync), connection `Geometry`, and
  `CanvasPainter` for drawing and export.
- `src/app` — `Editor` (the object QML talks to), `NodeModel`, the painted grid/connection
  layers, and the QML UI.
- `tests` — Qt Test unit tests for the core.
