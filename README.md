# Junction

A desktop canvas for boxes-and-arrows thinking (software architecture, plans), combining
Obsidian Canvas's quick card-and-connection workflow with tldraw-style control over how
connections run. Canvases are organised in Projects, the way tldraw organises pages in files.
Each Canvas is a [JSON Canvas](https://jsoncanvas.org) file, so Obsidian canvases can be
imported and exported. Terms are defined in [GLOSSARY.md](GLOSSARY.md); decisions
in [docs/adr](docs/adr).

## Build

Requires Qt 6.8+ (Core, Gui, Svg, Network, Quick, QuickControls2, Test), CMake and a C++20 compiler.

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build          # core unit tests
./build/src/app/junction
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
| Add a Waypoint | select a connection, drag a hollow midpoint dot (or its label, which covers the middle dot) |
| Remove a Waypoint | double-click it |
| Re-attach an end | select a connection, drag its end dot onto a card or Handle |
| Label a connection | double-click it or its label (Enter, Esc or click away to finish; clear the text to remove the label) |
| Pan | scroll, middle-drag or Space+drag |
| Zoom | Ctrl+scroll; Ctrl+0 resets, Ctrl+1 fits |
| Resize | drag any side or corner of a card or group |
| Group | Ctrl+G or right-click → Group selection (with nothing selected: an empty group) |
| Nudge | arrow keys move the selection one grid step |
| Snap | toolbar toggle; hold Alt while dragging for the opposite |

Undo/redo (each Canvas keeps its own history while the app runs), copy/paste, Ctrl+D duplicate
(a Group comes with its cards), Delete. Export PNG/SVG from the ☰ menu (exports the selection,
or everything when nothing is selected).

### Projects and Canvases

The sidebar lists every Project with its Canvases. Everything saves automatically, and each
Canvas reopens where you left it.

| Do | Gesture |
|---|---|
| Open a Canvas | click it; Alt+↑ / Alt+↓ for the previous / next one in the Project |
| Open a Project | click it (opens the Canvas last open there); the chevron only expands/collapses |
| New Canvas / Project | the + on a Project row / at the top (Ctrl+N / Ctrl+Shift+N) |
| Rename | double-click a row, or click a name in the toolbar (F2: the open Canvas) |
| Reorder, move to another Project | drag the row (or right-click → Move to) |
| Duplicate, delete | right-click a row, or its ⋯ button (deleting moves files to the trash) |
| Search | Ctrl+F (or Ctrl+P), type, Enter opens the first match |
| Hide / resize the sidebar | Ctrl+\ / drag its edge |
| Import an Obsidian canvas | ☰ → Import canvas…, or drop `.canvas` files on the window |
| Export a canvas file | ☰ → Export as canvas file… |

The Library lives in the app-data folder (`~/.local/share/Junction/Junction/Library` on Linux;
☰ → Open library folder): one folder per Project, one `.canvas` file per Canvas, and a
`.junction` index with the order, views and what was open. Only one Junction runs at a time;
starting it again brings the window forward.

## Code layout

- `src/core` — no QML: the model (`CanvasItems`, `Document`), every edit as a `Change`
  inside a `Transaction` (undo, and later network sync), connection `Geometry`,
  `CanvasPainter` for drawing and export, and the `Library` of Projects and Canvases on disk.
- `src/app` — `Editor` (the object QML talks to), `NodeModel`, `LibraryModel` (the sidebar),
  the painted grid/connection layers, and the QML UI.
- `tests` — Qt Test unit tests for the core.
