#include "Editor.h"

#include "CanvasPainter.h"
#include "DesignValues.h"
#include "Fragment.h"
#include "Geometry.h"

#include <QClipboard>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

constexpr QSizeF CardSize(240, 80);
constexpr QSizeF GroupSize(480, 320);
constexpr QSizeF MinimumSize(80, 40);

Item translated(Item item, QPointF delta)
{
    if (Node *n = std::get_if<Node>(&item)) {
        n->rect.translate(delta);
    } else {
        for (QPointF &p : std::get<Connection>(item).waypoints)
            p += delta;
    }
    return item;
}

Connection newConnection(const QString &from, const QString &to, Side toSide)
{
    Connection c;
    c.id = Document::newId();
    c.fromNode = from;
    c.toNode = to;
    c.toSide = toSide;
    return c;
}

// The Change that applies `edit` to a copy of `item`.
template <typename T, typename Edit>
Change edited(const T &item, Edit edit)
{
    T changed = item;
    edit(changed);
    return Change::updated(item, changed);
}

} // namespace

Editor::Editor(Library *library, QClipboard *clipboard, QObject *parent)
    : QObject(parent), m_library(library), m_clipboard(clipboard)
{
    // The open Canvas follows the Library. Before the Library changes files or opens another
    // Canvas, the open one is saved, without a drag still in progress.
    connect(m_library, &Library::aboutToChangeFiles, this, [this] {
        cancelDrag();
        m_canvas.savePending();
    });
    connect(m_library, &Library::currentChanged, this, &Editor::loadCurrent);
    connect(m_library, &Library::canvasMoved, &m_canvas, &OpenCanvas::fileMoved);
    connect(m_library, &Library::canvasRemoved, &m_canvas, &OpenCanvas::fileRemoved);

    connect(&m_canvas, &OpenCanvas::opened, this, &Editor::canvasOpened);

    // Anything that changes in the Document may move the selected Connection's handles.
    const Document *document = &m_canvas.document();
    for (auto signal : {&Document::nodeChanged, &Document::nodeRemoved, &Document::nodeAdded})
        connect(document, signal, this, &Editor::syncSelectionWithDocument);
    connect(document, &Document::connectionsChanged, this, &Editor::syncSelectionWithDocument);
    connect(document, &Document::reset, this, &Editor::syncSelectionWithDocument);
    connect(m_library, &Library::loaded, this, &Editor::loadCurrent);   // also when nothing could be opened
}

// Returns whether it went on the undo stack. Nothing else may change the Document while a drag
// is in progress: the drag's final Transaction is computed from the state at its start.
bool Editor::commit(const Transaction &transaction, const QString &text)
{
    return !isDragging() && m_canvas.push(transaction, text);
}

// Commits a Transaction that adds the item `selected`, and makes that the selection. Returns its
// id, or "" when it wasn't added (during a drag, or when the Canvas can't be edited).
QString Editor::addAndSelect(const Transaction &transaction, const QString &text, const QString &selected)
{
    if (!commit(transaction, text))
        return {};
    setSelection({selected});
    return selected;
}

void Editor::undo()
{
    if (!isDragging())
        m_canvas.undo();
}

void Editor::redo()
{
    if (!isDragging())
        m_canvas.redo();
}

void Editor::setSnapToGrid(bool snap)
{
    if (snap == m_snapToGrid)
        return;
    m_snapToGrid = snap;
    emit snapToGridChanged();
}

// ---- Selection ----

void Editor::setSelection(const QStringList &ids)
{
    if (ids == m_selection)
        return;
    m_selection = ids;
    emit selectionChanged();
    emit selectedItemsChanged();
}

// The Document changed: the selection drops the ids of items that are gone (deleted, undone),
// and says the selected items themselves may have changed.
void Editor::syncSelectionWithDocument()
{
    QStringList kept;
    for (const QString &id : m_selection) {
        if (document().item(id))
            kept.append(id);
    }
    if (kept != m_selection)
        setSelection(kept);
    else
        emit selectedItemsChanged();   // the same items, but something about them changed
}

void Editor::select(const QString &id, bool toggle)
{
    if (!toggle) {
        setSelection({id});
        return;
    }
    QStringList ids = m_selection;
    if (!ids.removeOne(id))
        ids.append(id);
    setSelection(ids);
}

void Editor::selectInRect(const QRectF &rect, const QStringList &base)
{
    // Cards are selected when the rectangle touches them. Groups only when it covers them
    // completely, so a rectangle drawn inside a Group selects its cards, not the Group.
    QStringList ids = base;
    for (const Node &n : document().nodes()) {
        const bool hit = n.isCard() ? rect.intersects(n.rect) : rect.contains(n.rect);
        if (hit && !ids.contains(n.id))
            ids.append(n.id);
    }
    setSelection(ids);
}

void Editor::selectAll()
{
    QStringList ids;
    for (const Node &n : document().nodes())
        ids.append(n.id);
    for (const Connection &c : document().connections())
        ids.append(c.id);
    setSelection(ids);
}

void Editor::clearSelection()
{
    setSelection({});
}

ConnectionDetails Editor::selectedConnection() const
{
    const Connection *c = m_selection.size() == 1 ? document().connection(m_selection.first()) : nullptr;
    const auto route = c ? document().route(*c) : std::nullopt;
    if (!route)
        return {};
    return {
        .valid = true,
        .id = c->id,
        .style = c->style,
        .fromArrow = c->fromArrow,
        .toArrow = c->toArrow,
        .label = c->label,
        .waypoints = c->waypoints,
        .midpoints = route->midpoints,
        .start = route->start,
        .end = route->end,
        .labelPosition = route->labelPosition,
        .labelRect = CanvasPainter::labelRect(*route, c->label),
    };
}

QString Editor::selectionColor() const
{
    QStringList colors;
    for (const QString &id : m_selection) {
        if (const Node *n = document().node(id))
            colors.append(n->color);
        else if (const Connection *c = document().connection(id))
            colors.append(c->color);
    }
    colors.removeDuplicates();
    return colors.size() == 1 ? colors.first() : QString();
}

SelectionSummary Editor::selectionSummary() const
{
    SelectionSummary summary;
    QStringList labels;
    for (const QString &id : m_selection) {
        if (const Node *n = document().node(id)) {
            if (n->kind() == NodeKind::Group) {
                ++summary.groups;
                summary.labelTargets.append(id);
                labels.append(n->label());
            } else {
                ++summary.cards;
                if (n->kind() == NodeKind::Text)
                    ++summary.editableCards;
            }
        } else if (const Connection *c = document().connection(id)) {
            ++summary.connections;
            summary.labelTargets.append(id);
            labels.append(c->label);
            if (!c->label.isEmpty())
                ++summary.labeledConnections;
        }
    }
    labels.removeDuplicates();
    summary.sharedLabel = labels.size() == 1 ? labels.first() : QString();
    return summary;
}

QRectF Editor::selectionBounds() const
{
    QRectF bounds;
    for (const QString &id : m_selection) {
        if (const Node *n = document().node(id)) {
            const qreal labelSpace = n->kind() == NodeKind::Group ? DesignValues::GroupLabelSpace : 0;
            bounds = bounds.united(n->rect.adjusted(0, -labelSpace, 0, 0));
        } else if (const Connection *c = document().connection(id)) {
            if (const auto route = document().route(*c)) {
                bounds = bounds.united(route->path.boundingRect())
                               .united(CanvasPainter::labelRect(*route, c->label));   // empty without a label
            }
        }
    }
    return bounds;
}

// ---- Creating and editing ----

// A rectangle of `size` centered on `center`, with its corner on the grid when snapping.
QRectF Editor::placed(QPointF center, QSizeF size) const
{
    const QPointF topLeft = center - QPointF(size.width(), size.height()) / 2;
    return QRectF(snapped(topLeft), size);
}

Node Editor::newCard(QPointF center, const QString &text) const
{
    Node card;
    card.id = Document::newId();
    card.content = TextContent{text};
    card.rect = placed(center, CardSize);
    return card;
}

QString Editor::createCard(QPointF center)
{
    const Node card = newCard(center);
    return addAndSelect({Change::added(card)}, tr("Add card"), card.id);
}

// With Cards or Groups selected, the new Group goes around them; otherwise it is an empty
// Group centered on `center`.
QString Editor::createGroup(QPointF center)
{
    QRectF bounds;
    for (const QString &id : m_selection) {
        if (const Node *n = document().node(id))
            bounds = bounds.united(n->rect);
    }

    Node group;
    group.id = Document::newId();
    group.content = GroupContent{tr("Group")};
    const qreal margin = Geometry::GridSize * 2;
    group.rect = bounds.isEmpty() ? placed(center, GroupSize) : bounds.adjusted(-margin, -margin, margin, margin);
    return addAndSelect({Change::added(group)}, tr("Group"), group.id);
}

void Editor::setCardText(const QString &id, const QString &text)
{
    const Node *existing = document().node(id);
    if (existing && existing->kind() == NodeKind::Text)
        commit({edited(*existing, [&](Node &n) { n.content = TextContent{text}; })}, tr("Edit text"));
}

// The Changes that apply `editNode` to the nodes among `ids` and `editConnection` to the
// Connections. Items they leave as they were are dropped when committed.
template <typename EditNode, typename EditConnection>
Transaction Editor::editedItems(const QStringList &ids, EditNode editNode, EditConnection editConnection) const
{
    Transaction t;
    for (const QString &id : ids) {
        if (const Node *n = document().node(id))
            t.append(edited(*n, editNode));
        else if (const Connection *c = document().connection(id))
            t.append(edited(*c, editConnection));
    }
    return t;
}

template <typename EditConnection>
Transaction Editor::editedSelectedConnections(EditConnection editConnection) const
{
    return editedItems(m_selection, [](Node &) {}, editConnection);
}

void Editor::setColor(const QString &color)
{
    commit(editedItems(m_selection, [&](Node &n) { n.color = color; }, [&](Connection &c) { c.color = color; }),
           tr("Change color"));
}

// The Changes that remove the nodes in `nodeIds`, every Connection touching them, and the
// Connections among `connectionIds`.
Transaction Editor::removal(const QSet<QString> &nodeIds, const QStringList &connectionIds) const
{
    Transaction t;
    for (const Connection &c : document().connections()) {
        if (nodeIds.contains(c.fromNode) || nodeIds.contains(c.toNode) || connectionIds.contains(c.id))
            t.append(Change::removed(c));
    }
    for (const Node &n : document().nodes()) {
        if (nodeIds.contains(n.id))
            t.append(Change::removed(n));
    }
    return t;
}

// Removes what is selected; a Group's Cards stay (cut takes them along).
void Editor::deleteSelection()
{
    QSet<QString> nodes;
    for (const QString &id : m_selection) {
        if (document().node(id))
            nodes.insert(id);
    }
    commit(removal(nodes, m_selection), tr("Delete"));
}

// ---- Dragging ----
// While dragging, changes are applied to the Document directly so the view follows the
// mouse. On release, a single Transaction from the original to the final state goes on the
// undo stack.

template <typename Kind>
const Kind *Editor::activeDrag() const
{
    return m_drag ? std::get_if<Kind>(&m_drag->kind) : nullptr;
}

// Not on a Canvas that can't be edited: the drag would show changes that can't be kept. A drag
// still in progress (its release never came) is put back first.
bool Editor::beginDrag(const QVector<Item> &items, const decltype(Drag::kind) &kind)
{
    cancelDrag();
    if (!m_canvas.isEditable())
        return false;
    Transaction changes;
    for (const Item &item : items)
        changes.append(Change::updated(item, item));
    m_drag = Drag{changes, kind};
    emit draggingChanged();
    return true;
}

template <typename Moved>
void Editor::showDrag(Moved moved)
{
    QVector<Item> items;
    for (Change &change : m_drag->changes) {
        change = Change::updated(*change.before(), moved(*change.before()));
        items.append(*change.after());
    }
    m_canvas.showUncommitted(items);
}

// Puts the dragged items back where the drag began, without an undo step.
void Editor::cancelDrag()
{
    if (!m_drag)
        return;
    QVector<Item> originals;
    for (const Change &change : std::as_const(m_drag->changes))
        originals.append(*change.before());
    m_canvas.showUncommitted(originals);
    m_drag.reset();
    emit draggingChanged();
}

void Editor::endDrag(const QString &text)
{
    if (!m_drag)
        return;
    const Drag drag = *std::exchange(m_drag, std::nullopt);
    emit draggingChanged();
    commit(drag.changes, text);
}

// Selected nodes move, and so does everything inside a selected Group, and the Waypoints of
// Connections between two moving nodes.
QVector<Item> Editor::itemsMovingWithSelection() const
{
    const QSet<QString> moving = document().nodesWithContents(m_selection);
    QVector<Item> items;
    for (const Node &n : document().nodes()) {
        if (moving.contains(n.id))
            items.append(n);
    }
    // Waypoints of Connections between two moving nodes move along with them.
    for (const Connection &c : document().connections()) {
        if (!c.waypoints.isEmpty() && moving.contains(c.fromNode) && moving.contains(c.toNode))
            items.append(c);
    }
    return items;
}

void Editor::beginMove(const QString &id)
{
    const Node *grabbed = document().node(id);
    if (!grabbed)
        return;
    if (!m_selection.contains(id))
        setSelection({id});
    beginDrag(itemsMovingWithSelection(), MoveDrag{grabbed->rect.topLeft()});
}

void Editor::nudge(QPointF steps)
{
    const QPointF delta = steps * Geometry::GridSize;
    Transaction t;
    for (const Item &item : itemsMovingWithSelection())
        t.append(Change::updated(item, translated(item, delta)));
    commit(t, tr("Move"));
}

void Editor::updateMove(QPointF delta, bool invertSnap)
{
    const MoveDrag *move = activeDrag<MoveDrag>();
    if (!move)
        return;
    const QPointF snappedDelta = snapped(move->anchor + delta, invertSnap) - move->anchor;
    showDrag([&](const Item &item) { return translated(item, snappedDelta); });
}

void Editor::endMove()
{
    endDrag(tr("Move"));
}

void Editor::beginResize(const QString &id, Qt::Edges edges)
{
    const Node *n = document().node(id);
    if (!n)
        return;
    setSelection({id});
    beginDrag({*n}, ResizeDrag{edges});
}

void Editor::updateResize(QPointF delta, bool invertSnap)
{
    const ResizeDrag *resize = activeDrag<ResizeDrag>();
    if (!resize)
        return;
    auto place = [&](qreal side) { return snapped(side, invertSnap); };
    showDrag([&](const Item &item) {
        Node n = std::get<Node>(item);
        QRectF &r = n.rect;
        if (resize->edges & Qt::LeftEdge)
            r.setLeft(std::min(place(r.left() + delta.x()), r.right() - MinimumSize.width()));
        if (resize->edges & Qt::RightEdge)
            r.setRight(std::max(place(r.right() + delta.x()), r.left() + MinimumSize.width()));
        if (resize->edges & Qt::TopEdge)
            r.setTop(std::min(place(r.top() + delta.y()), r.bottom() - MinimumSize.height()));
        if (resize->edges & Qt::BottomEdge)
            r.setBottom(std::max(place(r.bottom() + delta.y()), r.top() + MinimumSize.height()));
        return Item(n);
    });
}

void Editor::endResize()
{
    endDrag(tr("Resize"));
}

// With `insert`, a new Waypoint is created at the midpoint `index` (between control point
// `index` and `index + 1`) and then dragged.
void Editor::beginWaypointDrag(const QString &connectionId, int index, bool insert)
{
    const Connection *c = document().connection(connectionId);
    const auto route = c ? document().route(*c) : std::nullopt;
    if (!route)
        return;
    const QVector<QPointF> &points = insert ? route->midpoints : c->waypoints;
    if (index < 0 || index >= points.size())
        return;
    if (!beginDrag({*c}, WaypointDrag{index, points[index], insert}))
        return;
    showDraggedWaypointAt(points[index]);   // a new Waypoint shows up right away
}

void Editor::updateWaypointDrag(QPointF delta, bool invertSnap)
{
    const WaypointDrag *drag = activeDrag<WaypointDrag>();
    if (!drag)
        return;
    const QPointF point = drag->anchor + delta;
    showDraggedWaypointAt(snapped(point, invertSnap));
}

void Editor::showDraggedWaypointAt(QPointF point)
{
    const WaypointDrag *drag = activeDrag<WaypointDrag>();
    showDrag([&](const Item &item) {
        Connection c = std::get<Connection>(item);
        if (drag->inserted)
            c.waypoints.insert(drag->index, point);
        else
            c.waypoints[drag->index] = point;
        return Item(c);
    });
}

void Editor::endWaypointDrag()
{
    endDrag(tr("Move waypoint"));
}

// ---- Connections ----

void Editor::connectNodes(const QString &from, const QString &to, Side toSide)
{
    if (from == to || !document().node(from) || !document().node(to))
        return;
    const Connection c = newConnection(from, to, toSide);
    addAndSelect({Change::added(c)}, tr("Connect"), c.id);
}

QString Editor::connectToNewCard(const QString &from, QPointF center)
{
    if (!document().node(from))
        return {};
    const Node card = newCard(center);
    const Transaction t{Change::added(card), Change::added(newConnection(from, card.id, Side::Floating))};
    return addAndSelect(t, tr("Add connected card"), card.id);
}

void Editor::reattach(const QString &connectionId, bool fromEnd, const QString &nodeId, Side side)
{
    const Connection *existing = document().connection(connectionId);
    if (!existing || !document().node(nodeId))
        return;
    Connection c = *existing;
    if (fromEnd) {
        c.fromNode = nodeId;
        c.fromSide = side;
    } else {
        c.toNode = nodeId;
        c.toSide = side;
    }
    if (c.fromNode != c.toNode && c != *existing)
        commit({Change::updated(*existing, c)}, tr("Reattach connection"));
}

void Editor::removeWaypoint(const QString &connectionId, int index)
{
    const Connection *existing = document().connection(connectionId);
    if (!existing || index < 0 || index >= existing->waypoints.size())
        return;
    commit({edited(*existing, [&](Connection &c) { c.waypoints.removeAt(index); })}, tr("Remove waypoint"));
}

void Editor::setConnectionStyle(PathStyle style)
{
    commit(editedSelectedConnections([&](Connection &c) { c.style = style; }),
           tr("Change path style"));
}

void Editor::setArrows(bool fromArrow, bool toArrow)
{
    commit(editedSelectedConnections([&](Connection &c) {
               c.fromArrow = fromArrow;
               c.toArrow = toArrow;
           }),
           tr("Change arrows"));
}

// Only Groups and Connections have a Label. A Label of only spaces would be an empty box, so
// spaces around it are dropped.
void Editor::setLabels(const QStringList &ids, const QString &label)
{
    const QString trimmed = label.trimmed();
    auto setGroupLabel = [&](Node &n) {
        if (n.kind() == NodeKind::Group)
            n.content = GroupContent{trimmed};
    };
    commit(editedItems(ids, setGroupLabel, [&](Connection &c) { c.label = trimmed; }), tr("Edit label"));
}

void Editor::removeConnectionLabels()
{
    commit(editedSelectedConnections([](Connection &c) { c.label.clear(); }), tr("Remove label"));
}

// ---- Clipboard ----
// The clipboard holds a Fragment: the selected nodes, the Cards inside selected Groups, and the
// Connections between them.

void Editor::insertFragment(const QJsonObject &fragment, QPointF offset)
{
    const Fragment::Insertion insertion = Fragment::insertion(fragment, offset);
    if (commit(insertion.transaction, tr("Paste")))
        setSelection(insertion.nodeIds);
}

// Copies, then removes what was copied and every Connection touching it, so a cut Group takes
// its Cards along (Delete removes only the Group). Selected Connections are removed too: one
// can't be pasted on its own, but cutting it still takes it away.
void Editor::cut()
{
    const QSet<QString> ids = document().nodesWithContents(m_selection);
    if (ids.isEmpty() && selectionSummary().connections == 0)
        return;
    copy();
    commit(removal(ids, m_selection), tr("Cut"));
}

void Editor::copy()
{
    const QJsonObject fragment = Fragment::copy(document(), document().nodesWithContents(m_selection));
    if (!fragment["nodes"].toArray().isEmpty())
        m_clipboard->setText(QString::fromUtf8(QJsonDocument(fragment).toJson(QJsonDocument::Compact)));
}

void Editor::paste(QPointF center)
{
    const QString text = m_clipboard->text();
    const QJsonObject fragment = QJsonDocument::fromJson(text.toUtf8()).object();
    const QRectF bounds = Fragment::bounds(fragment);
    if (!bounds.isEmpty()) {
        const QPointF offset = center - bounds.center();
        insertFragment(fragment, snapped(offset));
    } else if (!text.trimmed().isEmpty()) {
        // Text copied from somewhere else becomes a new card.
        const Node card = newCard(center, text.trimmed());
        addAndSelect({Change::added(card)}, tr("Paste"), card.id);
    }
}

void Editor::duplicate()
{
    const qreal offset = Geometry::GridSize * 2;
    insertFragment(Fragment::copy(document(), document().nodesWithContents(m_selection)), QPointF(offset, offset));
}

// ---- Files ----

// Shows the Library's current Canvas.
void Editor::loadCurrent()
{
    const QString path = m_library->currentPath();
    if (path == m_canvas.filePath() && !path.isEmpty())
        return;   // the open file was only renamed or moved; OpenCanvas::fileMoved() has followed it
    cancelDrag();
    setSelection({});
    if (path.isEmpty())
        m_canvas.openNothing(tr("The library folder %1 can't be written to.").arg(m_library->root()));
    else
        m_canvas.open(path);
}

void Editor::save()
{
    m_canvas.save();
}

QRectF Editor::contentBounds() const
{
    QRectF bounds;
    for (const Node &n : document().nodes())
        bounds = bounds.united(n.rect);
    return bounds;
}
