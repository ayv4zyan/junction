#include "Editor.h"

#include "CanvasPainter.h"
#include "DesignValues.h"
#include "Fragment.h"
#include "Geometry.h"

#include <QClipboard>
#include <QGuiApplication>
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

// For editedSelection(): leaves the selected nodes as they are.
void keepNode(Node &) {}

// The Change that applies `edit` to a copy of `item`.
template <typename T, typename Edit>
Change edited(const T &item, Edit edit)
{
    T changed = item;
    edit(changed);
    return {item, changed};
}

} // namespace

Editor::Editor(Library *library, QObject *parent)
    : QObject(parent), m_library(library)
{
    // The open Canvas follows the Library. Before the Library changes files or opens another
    // Canvas, the open one is saved.
    connect(m_library, &Library::aboutToChange, &m_canvas, &OpenCanvas::savePending);
    connect(m_library, &Library::currentChanged, this, &Editor::loadCurrent);
    connect(m_library, &Library::canvasMoved, &m_canvas, &OpenCanvas::fileMoved);
    connect(m_library, &Library::canvasRemoved, &m_canvas, &OpenCanvas::fileRemoved);

    connect(&m_canvas, &OpenCanvas::opened, this, &Editor::canvasOpened);
    connect(&m_canvas, &OpenCanvas::filePathChanged, this, &Editor::filePathChanged);
    connect(&m_canvas, &OpenCanvas::historyChanged, this, &Editor::historyChanged);
    connect(&m_canvas, &OpenCanvas::errorOccurred, this, &Editor::errorOccurred);

    // Anything that changes in the Document may move the selected Connection's handles.
    const Document *document = &m_canvas.document();
    for (auto signal : {&Document::nodeChanged, &Document::nodeRemoved, &Document::nodeAdded})
        connect(document, signal, this, &Editor::pruneSelection);
    connect(document, &Document::connectionsChanged, this, &Editor::pruneSelection);
    connect(document, &Document::reset, this, &Editor::pruneSelection);

    m_viewSaveTimer.setSingleShot(true);
    m_viewSaveTimer.setInterval(1000);
    connect(&m_viewSaveTimer, &QTimer::timeout, m_library, &Library::saveIndex);
}

// Nothing else may change the Document while a drag is in progress: the drag's final
// Transaction is computed from the state at its start.
void Editor::commit(const Transaction &transaction, const QString &text)
{
    if (!isDragging())
        m_canvas.push(transaction, text);
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

// Drops ids of items that no longer exist (deleted, undone) from the selection.
void Editor::pruneSelection()
{
    QStringList kept;
    for (const QString &id : m_selection) {
        if (document().item(id))
            kept.append(id);
    }
    setSelection(kept);
    emit selectedItemsChanged();
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
        .style = pathStyleToString(c->style),
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
            if (n->kind == NodeKind::Group) {
                ++summary.groups;
                summary.labelTargets.append(id);
                labels.append(n->label);
            } else {
                ++summary.cards;
                if (n->kind == NodeKind::Text)
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
            const qreal labelSpace = n->kind == NodeKind::Group ? DesignValues::GroupLabelSpace : 0;
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

Node Editor::newCard(QPointF center, const QString &text) const
{
    Node card;
    card.id = Document::newId();
    card.text = text;
    const QPointF topLeft = center - QPointF(CardSize.width(), CardSize.height()) / 2;
    card.rect = QRectF(snapping(false) ? Geometry::snap(topLeft) : topLeft, CardSize);
    return card;
}

QString Editor::createCard(QPointF center)
{
    const Node card = newCard(center);
    commit({{std::nullopt, card}}, tr("Add card"));
    setSelection({card.id});
    return card.id;
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
    group.kind = NodeKind::Group;
    group.label = tr("Group");
    if (!bounds.isEmpty()) {
        const qreal margin = Geometry::GridSize * 2;
        group.rect = bounds.adjusted(-margin, -margin, margin, margin);
    } else {
        const QPointF topLeft = center - QPointF(GroupSize.width(), GroupSize.height()) / 2;
        group.rect = QRectF(snapping(false) ? Geometry::snap(topLeft) : topLeft, GroupSize);
    }
    commit({{std::nullopt, group}}, tr("Group"));
    setSelection({group.id});
    return group.id;
}

void Editor::setCardText(const QString &id, const QString &text)
{
    const Node *existing = document().node(id);
    if (existing && existing->kind == NodeKind::Text)
        commit({edited(*existing, [&](Node &n) { n.text = text; })}, tr("Edit text"));
}

// The Changes that apply `editNode` to every selected node and `editConnection` to every
// selected Connection. Items they leave as they were are dropped when committed.
template <typename EditNode, typename EditConnection>
Transaction Editor::editedSelection(EditNode editNode, EditConnection editConnection) const
{
    Transaction t;
    for (const QString &id : m_selection) {
        if (const Node *n = document().node(id))
            t.append(edited(*n, editNode));
        else if (const Connection *c = document().connection(id))
            t.append(edited(*c, editConnection));
    }
    return t;
}

void Editor::setColor(const QString &color)
{
    commit(editedSelection([&](Node &n) { n.color = color; }, [&](Connection &c) { c.color = color; }),
           tr("Change color"));
}

void Editor::deleteSelection()
{
    Transaction t;
    QSet<QString> removedConnections;
    auto removeConnection = [&](const Connection &c) {
        if (!removedConnections.contains(c.id)) {
            removedConnections.insert(c.id);
            t.append({c, std::nullopt});
        }
    };

    for (const QString &id : m_selection) {
        if (const Connection *c = document().connection(id))
            removeConnection(*c);
    }
    for (const QString &id : m_selection) {
        if (const Node *n = document().node(id)) {
            for (const Connection &c : document().connectionsOf(id))
                removeConnection(c);
            t.append({*n, std::nullopt});
        }
    }
    commit(t, tr("Delete"));
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

void Editor::beginDrag(const QVector<Item> &items, const decltype(Drag::kind) &kind)
{
    Transaction changes;
    for (const Item &item : items)
        changes.append({item, item});
    m_drag = Drag{changes, kind};
    emit draggingChanged();
}

template <typename Moved>
void Editor::showDrag(Moved moved)
{
    QVector<Item> items;
    for (Change &change : m_drag->changes) {
        change.after = moved(*change.before);
        items.append(*change.after);
    }
    m_canvas.showUncommitted(items);
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
        t.append({item, translated(item, delta)});
    commit(t, tr("Move"));
}

void Editor::updateMove(QPointF delta, bool invertSnap)
{
    const MoveDrag *move = activeDrag<MoveDrag>();
    if (!move)
        return;
    QPointF target = move->anchor + delta;
    if (snapping(invertSnap))
        target = Geometry::snap(target);
    const QPointF snappedDelta = target - move->anchor;
    showDrag([&](const Item &item) { return translated(item, snappedDelta); });
}

void Editor::endMove()
{
    endDrag(tr("Move"));
}

// `horizontal` and `vertical` say which sides follow the mouse: -1 = left/top, 1 = right/bottom,
// 0 = neither. Dragging a corner moves two sides.
void Editor::beginResize(const QString &id, int horizontal, int vertical)
{
    const Node *n = document().node(id);
    if (!n)
        return;
    setSelection({id});
    beginDrag({*n}, ResizeDrag{horizontal, vertical});
}

void Editor::updateResize(QPointF delta, bool invertSnap)
{
    const ResizeDrag *resize = activeDrag<ResizeDrag>();
    if (!resize)
        return;
    const bool snap = snapping(invertSnap);
    auto place = [snap](qreal side) { return snap ? Geometry::snap(side) : side; };
    showDrag([&](const Item &item) {
        Node n = std::get<Node>(item);
        QRectF &r = n.rect;
        if (resize->horizontal < 0)
            r.setLeft(std::min(place(r.left() + delta.x()), r.right() - MinimumSize.width()));
        else if (resize->horizontal > 0)
            r.setRight(std::max(place(r.right() + delta.x()), r.left() + MinimumSize.width()));
        if (resize->vertical < 0)
            r.setTop(std::min(place(r.top() + delta.y()), r.bottom() - MinimumSize.height()));
        else if (resize->vertical > 0)
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
    beginDrag({*c}, WaypointDrag{index, points[index], insert});
    showDraggedWaypointAt(points[index]);   // a new Waypoint shows up right away
}

void Editor::updateWaypointDrag(QPointF delta, bool invertSnap)
{
    const WaypointDrag *drag = activeDrag<WaypointDrag>();
    if (!drag)
        return;
    const QPointF point = drag->anchor + delta;
    showDraggedWaypointAt(snapping(invertSnap) ? Geometry::snap(point) : point);
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

// The topmost Card under the point. `margin` widens each card so that a point on the outer
// half of a Handle still counts. Groups are never targets: dropping inside a Group means
// "new card here".
QString Editor::cardAt(QPointF point, qreal margin) const
{
    const QVector<Node> &nodes = document().nodes();
    for (auto it = nodes.crbegin(); it != nodes.crend(); ++it) {
        if (it->isCard() && it->rect.adjusted(-margin, -margin, margin, margin).contains(point))
            return it->id;
    }
    return {};
}

QString Editor::handleAt(const QString &nodeId, QPointF point, qreal radius) const
{
    const Node *n = document().node(nodeId);
    return n ? sideToString(Geometry::handleAt(n->rect, point, radius)) : QString();
}

QString Editor::connectionAt(QPointF point, qreal tolerance) const
{
    const QVector<Connection> &connections = document().connections();
    for (auto it = connections.crbegin(); it != connections.crend(); ++it) {
        const auto route = document().route(*it);
        // The label counts too: it can stick out far from the line.
        if (route && (Geometry::hits(*route, point, tolerance) || CanvasPainter::labelRect(*route, it->label).contains(point)))
            return it->id;
    }
    return {};
}

void Editor::setConnectionPreview(const QString &fromNode, QPointF pointer, const QString &targetNode,
                                  const QString &targetSide)
{
    m_preview = ConnectionPreview{fromNode, pointer, targetNode, sideFromString(targetSide)};
    emit previewChanged();
}

void Editor::clearConnectionPreview()
{
    m_preview.reset();
    emit previewChanged();
}

void Editor::connectNodes(const QString &from, const QString &to, const QString &toSide)
{
    if (from == to || !document().node(from) || !document().node(to))
        return;
    Connection c;
    c.id = Document::newId();
    c.fromNode = from;
    c.toNode = to;
    c.toSide = sideFromString(toSide);
    commit({{std::nullopt, c}}, tr("Connect"));
    setSelection({c.id});
}

QString Editor::connectToNewCard(const QString &from, QPointF center)
{
    if (!document().node(from))
        return {};
    const Node card = newCard(center);
    Connection c;
    c.id = Document::newId();
    c.fromNode = from;
    c.toNode = card.id;
    commit({{std::nullopt, card}, {std::nullopt, c}}, tr("Add connected card"));
    setSelection({card.id});
    return card.id;
}

void Editor::reattach(const QString &connectionId, bool fromEnd, const QString &nodeId, const QString &side)
{
    const Connection *existing = document().connection(connectionId);
    if (!existing || !document().node(nodeId))
        return;
    Connection c = *existing;
    if (fromEnd) {
        c.fromNode = nodeId;
        c.fromSide = sideFromString(side);
    } else {
        c.toNode = nodeId;
        c.toSide = sideFromString(side);
    }
    if (c.fromNode != c.toNode && c != *existing)
        commit({{*existing, c}}, tr("Reattach connection"));
}

void Editor::removeWaypoint(const QString &connectionId, int index)
{
    const Connection *existing = document().connection(connectionId);
    if (!existing || index < 0 || index >= existing->waypoints.size())
        return;
    commit({edited(*existing, [&](Connection &c) { c.waypoints.removeAt(index); })}, tr("Remove waypoint"));
}

void Editor::setConnectionStyle(const QString &style)
{
    commit(editedSelection(keepNode, [&](Connection &c) { c.style = pathStyleFromString(style, c.style); }),
           tr("Change path style"));
}

void Editor::setArrows(bool fromArrow, bool toArrow)
{
    commit(editedSelection(keepNode, [&](Connection &c) {
               c.fromArrow = fromArrow;
               c.toArrow = toArrow;
           }),
           tr("Change arrows"));
}

// Only Groups and Connections have a Label. A Label of only spaces would be an empty box, so
// spaces around it are dropped.
void Editor::setLabel(const QString &id, const QString &label)
{
    const QString trimmed = label.trimmed();
    if (const Node *n = document().node(id); n && n->kind == NodeKind::Group)
        commit({edited(*n, [&](Node &group) { group.label = trimmed; })}, tr("Edit label"));
    else if (const Connection *c = document().connection(id))
        commit({edited(*c, [&](Connection &changed) { changed.label = trimmed; })}, tr("Edit label"));
}

void Editor::setLabels(const QString &label)
{
    const QString trimmed = label.trimmed();   // as in setLabel()
    auto setGroupLabel = [&](Node &n) {
        if (n.kind == NodeKind::Group)
            n.label = trimmed;
    };
    commit(editedSelection(setGroupLabel, [&](Connection &c) { c.label = trimmed; }), tr("Edit label"));
}

void Editor::removeConnectionLabels()
{
    commit(editedSelection(keepNode, [](Connection &c) { c.label.clear(); }), tr("Remove label"));
}

// ---- Clipboard ----
// The clipboard holds a Fragment: the selected nodes, the Cards inside selected Groups, and the
// Connections between them.

void Editor::insertFragment(const QJsonObject &fragment, QPointF offset)
{
    const Fragment::Insertion insertion = Fragment::insertion(fragment, offset);
    commit(insertion.transaction, tr("Paste"));
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
    Transaction t;
    for (const Connection &c : document().connections()) {
        if (ids.contains(c.fromNode) || ids.contains(c.toNode) || m_selection.contains(c.id))
            t.append({c, std::nullopt});
    }
    for (const Node &n : document().nodes()) {
        if (ids.contains(n.id))
            t.append({n, std::nullopt});
    }
    commit(t, tr("Cut"));
}

void Editor::copy()
{
    const QJsonObject fragment = Fragment::copy(document(), document().nodesWithContents(m_selection));
    if (!fragment["nodes"].toArray().isEmpty())
        QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(fragment).toJson(QJsonDocument::Compact)));
}

void Editor::paste(QPointF center)
{
    const QString text = QGuiApplication::clipboard()->text();
    const QJsonObject fragment = QJsonDocument::fromJson(text.toUtf8()).object();
    const QRectF bounds = Fragment::bounds(fragment);
    if (!bounds.isEmpty()) {
        const QPointF offset = center - bounds.center();
        insertFragment(fragment, snapping(false) ? Geometry::snap(offset) : offset);
    } else if (!text.trimmed().isEmpty()) {
        // Text copied from somewhere else becomes a new card.
        const Node card = newCard(center, text.trimmed());
        commit({{std::nullopt, card}}, tr("Paste"));
        setSelection({card.id});
    }
}

void Editor::duplicate()
{
    const qreal offset = Geometry::GridSize * 2;
    insertFragment(Fragment::copy(document(), document().nodesWithContents(m_selection)), QPointF(offset, offset));
}

// ---- Files ----

void Editor::start()
{
    m_library->load();
    loadCurrent();   // in case load() found nothing to open
}

// Shows the Library's current Canvas.
void Editor::loadCurrent()
{
    const QString path = m_library->currentPath();
    if (path == m_canvas.filePath() && !path.isEmpty())
        return;   // the open file was only renamed or moved; OpenCanvas::fileMoved() has followed it
    setSelection({});
    clearConnectionPreview();
    if (path.isEmpty())
        m_canvas.openNothing(tr("The library folder %1 can't be written to.").arg(m_library->root()));
    else
        m_canvas.open(path);
}

void Editor::save()
{
    m_canvas.save();
}

QUrl Editor::exportCanvas(const QUrl &url)
{
    QString path = url.toLocalFile();
    if (!path.endsWith(".canvas"))
        path += ".canvas";
    QString error;
    if (!document().save(path, &error)) {
        emit errorOccurred(tr("Could not save %1: %2").arg(path, error));
        return {};
    }
    return QUrl::fromLocalFile(path);
}

QVariantMap Editor::savedView() const
{
    const auto view = m_library->view(m_library->currentProject(), m_library->currentCanvas());
    if (!view)
        return {};
    return {{"x", view->center.x()}, {"y", view->center.y()}, {"zoom", view->zoom}};
}

void Editor::rememberView(QPointF center, qreal zoom)
{
    m_library->setView(m_library->currentProject(), m_library->currentCanvas(), {center, zoom});
    m_viewSaveTimer.start();
}

QUrl Editor::exportImage(const QUrl &url, const QVariantMap &palette)
{
    // Exports the selection, or the whole Canvas when nothing is selected.
    const std::optional<Palette> colors = Palette::fromVariantMap(palette);
    QString error = tr("The colors to export with are incomplete.");
    if (colors && CanvasPainter::exportCanvas(document(), document().nodesWithContents(m_selection),
                                              url.toLocalFile(), *colors, &error))
        return url;
    emit errorOccurred(error);
    return {};
}

QRectF Editor::contentBounds() const
{
    QRectF bounds;
    for (const Node &n : document().nodes())
        bounds = bounds.united(n.rect);
    return bounds;
}
