# Junction

A desktop tool for sketching boxes-and-arrows boards (such as software architecture), combining Obsidian Canvas's quick card-and-connection workflow with tldraw-style control over connection shape.

## Language

### Board

**Canvas**:
An unbounded board holding Cards, Groups and Connections, saved as one document.
_Avoid_: board, diagram, whiteboard, page

**Project**:
A named collection of Canvases kept together, such as all the Canvases for one web app. Only organises them: each Canvas is still shared on its own.
_Avoid_: file, workspace, notebook, folder

**Library**:
Every Project the user has. The app keeps exactly one and owns where it is stored.
_Avoid_: vault, workspace, home

**Card**:
A fixed-size rectangle on the Canvas holding Markdown text. Created by double-clicking empty space.
_Avoid_: node, element, box, rectangle

**Unsupported Card**:
A card kind from another app's canvas (e.g. an Obsidian note or web link) that is shown as a labeled placeholder and saved back unchanged.
_Avoid_: unknown node, file card, link card

**Group**:
A labeled rectangle; every Card lying inside its bounds belongs to it and moves with it.
_Avoid_: frame, container, cluster

**Selection**:
The Cards, Groups and Connections currently picked out on a Canvas; editing commands (delete, duplicate, copy, group, color) act on all of it at once.
_Avoid_: highlighted items, active items

### Connections

**Handle**:
One of the dots shown on a Card's side when hovering it; dragging from it starts a Connection.
_Avoid_: anchor, port, dot

**Connection**:
A line between two Cards, optionally with arrowheads and a label.
_Avoid_: edge, arrow, link, line

**Floating End**:
A Connection end not fixed to a side: it follows the other end as Cards move, always facing it.
_Avoid_: auto side, dynamic anchor

**Pinned End**:
A Connection end fixed to one specific side of its Card, made by dropping the end on a Handle.
_Avoid_: fixed anchor, locked side

**Path Style**:
How a Connection's path is drawn between its ends: straight, curved or elbow.
_Avoid_: line type, routing mode

**Label**:
The short name shown on a Group or a Connection. A Group always shows one (a default when left empty); a Connection's is optional. Cards hold text, not a label.
_Avoid_: name, title, caption

**Waypoint**:
A user-placed point on a Connection that its path must pass through.
_Avoid_: anchor point, control point, bend
