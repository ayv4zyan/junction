#include "Editor.h"

#include "CanvasPainter.h"
#include "Geometry.h"
#include "TransactionCommand.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>

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

QVariantList toVariantList(const QVector<QPointF> &points)
{
    QVariantList list;
    for (const QPointF &p : points)
        list.append(p);
    return list;
}

} // namespace

Editor::Editor(QObject *parent)
    : QObject(parent)
{
    m_snapToGrid = QSettings().value("snapToGrid", true).toBool();

    connect(&m_history, &QUndoGroup::canUndoChanged, this, &Editor::historyChanged);
    connect(&m_history, &QUndoGroup::canRedoChanged, this, &Editor::historyChanged);

    // Before the Library changes files or opens another Canvas, the open one is saved.
    connect(&m_library, &Library::aboutToChange, this, [this] {
        if (m_saveTimer.isActive())
            saveNow();
    });
    connect(&m_library, &Library::currentChanged, this, &Editor::loadCurrent);
    connect(&m_library, &Library::canvasMoved, this, &Editor::canvasMoved);
    connect(&m_library, &Library::canvasRemoved, this, &Editor::canvasRemoved);
    connect(&m_library, &Library::errorOccurred, this, &Editor::errorOccurred);

    // Anything that changes in the Document may move the selected Connection's handles.
    for (auto signal : {&Document::nodeChanged, &Document::nodeRemoved, &Document::nodeAdded})
        connect(&m_document, signal, this, &Editor::pruneSelection);
    connect(&m_document, &Document::connectionsChanged, this, &Editor::pruneSelection);
    connect(&m_document, &Document::reset, this, &Editor::pruneSelection);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, &Editor::saveNow);
    m_viewSaveTimer.setSingleShot(true);
    m_viewSaveTimer.setInterval(1000);
    connect(&m_viewSaveTimer, &QTimer::timeout, &m_library, &Library::saveIndex);
}

Editor::~Editor()
{
    if (m_saveTimer.isActive())
        saveNow();
}

void Editor::commit(const Transaction &transaction, const QString &text)
{
    // Nothing else may change the Document while a drag is in progress: the drag's final
    // Transaction is computed from the state at its start.
    if (isDragging())
        return;
    Transaction real;
    for (const Change &change : transaction) {
        if (change.before != change.after)   // skip changes that change nothing
            real.append(change);
    }
    if (real.isEmpty() || !m_loadError.isEmpty() || !m_history.activeStack())
        return;
    m_history.activeStack()->push(new TransactionCommand(&m_document, real, text));
    scheduleSave();
}

void Editor::undo()
{
    if (isDragging() || !m_history.canUndo())
        return;
    m_history.undo();
    scheduleSave();
}

void Editor::redo()
{
    if (isDragging() || !m_history.canRedo())
        return;
    m_history.redo();
    scheduleSave();
}

void Editor::setSnapToGrid(bool snap)
{
    if (snap == m_snapToGrid)
        return;
    m_snapToGrid = snap;
    QSettings().setValue("snapToGrid", snap);
    emit snapToGridChanged();
}

// ---- Selection ----

void Editor::setSelection(const QStringList &ids)
{
    if (ids == m_selection)
        return;
    m_selection = ids;
    emit selectionChanged();
    emit selectionDetailsChanged();
}

// Drops ids of items that no longer exist (deleted, undone) from the selection.
void Editor::pruneSelection()
{
    QStringList kept;
    for (const QString &id : m_selection) {
        if (m_document.item(id))
            kept.append(id);
    }
    setSelection(kept);
    emit selectionDetailsChanged();
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
    for (const Node &n : m_document.nodes()) {
        const bool hit = n.isCard() ? rect.intersects(n.rect) : rect.contains(n.rect);
        if (hit && !ids.contains(n.id))
            ids.append(n.id);
    }
    setSelection(ids);
}

void Editor::selectAll()
{
    QStringList ids;
    for (const Node &n : m_document.nodes())
        ids.append(n.id);
    for (const Connection &c : m_document.connections())
        ids.append(c.id);
    setSelection(ids);
}

void Editor::clearSelection()
{
    setSelection({});
}

QVector<Connection> Editor::selectedConnections() const
{
    QVector<Connection> result;
    for (const QString &id : m_selection) {
        if (const Connection *c = m_document.connection(id))
            result.append(*c);
    }
    return result;
}

// Everything the overlay and toolbar need to edit the one selected Connection.
QVariantMap Editor::selectedConnection() const
{
    if (m_selection.size() != 1)
        return {};
    const Connection *c = m_document.connection(m_selection.first());
    if (!c)
        return {};
    const Node *from = m_document.node(c->fromNode);
    const Node *to = m_document.node(c->toNode);
    if (!from || !to)
        return {};

    const Geometry::Route route = Geometry::route(from->rect, to->rect, *c);
    return {
        {"id", c->id},
        {"style", pathStyleToString(c->style)},
        {"fromArrow", c->fromArrow},
        {"toArrow", c->toArrow},
        {"label", c->label},
        {"waypoints", toVariantList(c->waypoints)},
        {"midpoints", toVariantList(route.midpoints)},
        {"start", route.start},
        {"end", route.end},
        {"labelPosition", route.labelPosition},
        {"labelRect", CanvasPainter::labelRect(route, c->label)},
    };
}

QString Editor::selectionColor() const
{
    QStringList colors;
    for (const QString &id : m_selection) {
        if (const Node *n = m_document.node(id))
            colors.append(n->color);
        else if (const Connection *c = m_document.connection(id))
            colors.append(c->color);
    }
    colors.removeDuplicates();
    return colors.size() == 1 ? colors.first() : QString();
}

QVariantMap Editor::selectionInfo() const
{
    int cards = 0, editableCards = 0, groups = 0, connections = 0, labeledConnections = 0;
    QStringList labelTargets;   // the selected items that have a Label: Groups and Connections
    QStringList labels;
    for (const QString &id : m_selection) {
        if (const Node *n = m_document.node(id)) {
            if (n->kind == NodeKind::Group) {
                ++groups;
                labelTargets.append(id);
                labels.append(n->label);
            } else {
                ++cards;
                if (n->kind == NodeKind::Text)   // an Unsupported Card can't be edited
                    ++editableCards;
            }
        } else if (const Connection *c = m_document.connection(id)) {
            ++connections;
            labelTargets.append(id);
            labels.append(c->label);
            if (!c->label.isEmpty())
                ++labeledConnections;
        }
    }
    labels.removeDuplicates();
    return {
        {"cards", cards},
        {"editableCards", editableCards},
        {"groups", groups},
        {"connections", connections},
        {"labeledConnections", labeledConnections},
        {"labelTargets", labelTargets},
        {"sharedLabel", labels.size() == 1 ? labels.first() : QString()},
    };
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
        if (const Node *n = m_document.node(id))
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

void Editor::setText(const QString &id, const QString &text)
{
    const Node *existing = m_document.node(id);
    if (!existing)
        return;
    Node changed = *existing;
    (changed.kind == NodeKind::Group ? changed.label : changed.text) = text;
    if (changed != *existing)
        commit({{*existing, changed}}, tr("Edit text"));
}

void Editor::setColor(const QString &color)
{
    Transaction t;
    for (const QString &id : m_selection) {
        if (const Node *n = m_document.node(id)) {
            Node changed = *n;
            changed.color = color;
            t.append({*n, changed});
        } else if (const Connection *c = m_document.connection(id)) {
            Connection changed = *c;
            changed.color = color;
            t.append({*c, changed});
        }
    }
    commit(t, tr("Change color"));
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
        if (const Connection *c = m_document.connection(id))
            removeConnection(*c);
    }
    for (const QString &id : m_selection) {
        if (const Node *n = m_document.node(id)) {
            for (const Connection &c : m_document.connectionsOf(id))
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

void Editor::beginDrag(const QVector<Item> &items)
{
    m_dragOriginal = items;
    m_dragCurrent = items;
}

void Editor::previewDrag(const QVector<Item> &items)
{
    m_dragCurrent = items;
    for (const Item &item : items)
        m_document.apply(Change{std::nullopt, item});
}

void Editor::endDrag(const QString &text)
{
    Transaction t;
    for (int i = 0; i < m_dragOriginal.size(); ++i) {
        if (m_dragOriginal[i] != m_dragCurrent[i])
            t.append({m_dragOriginal[i], m_dragCurrent[i]});
    }
    m_dragOriginal.clear();
    m_dragCurrent.clear();
    commit(t, text);
}

// Selected nodes move, and so does everything inside a selected Group, and the Waypoints of
// Connections between two moving nodes.
QVector<Item> Editor::itemsMovingWithSelection() const
{
    QSet<QString> moving;
    for (const QString &selectedId : m_selection) {
        if (const Node *n = m_document.node(selectedId)) {
            moving.insert(n->id);
            if (n->kind == NodeKind::Group) {
                for (const QString &inside : m_document.nodesInside(*n))
                    moving.insert(inside);
            }
        }
    }

    QVector<Item> items;
    for (const Node &n : m_document.nodes()) {
        if (moving.contains(n.id))
            items.append(n);
    }
    // Waypoints of Connections between two moving nodes move along with them.
    for (const Connection &c : m_document.connections()) {
        if (!c.waypoints.isEmpty() && moving.contains(c.fromNode) && moving.contains(c.toNode))
            items.append(c);
    }
    return items;
}

void Editor::beginMove(const QString &id)
{
    const Node *grabbed = m_document.node(id);
    if (!grabbed)
        return;
    if (!m_selection.contains(id))
        setSelection({id});
    m_dragAnchor = grabbed->rect.topLeft();
    beginDrag(itemsMovingWithSelection());
}

void Editor::nudge(QPointF delta)
{
    Transaction t;
    for (const Item &item : itemsMovingWithSelection())
        t.append({item, translated(item, delta)});
    commit(t, tr("Move"));
}

void Editor::updateMove(QPointF delta, bool invertSnap)
{
    QPointF target = m_dragAnchor + delta;
    if (snapping(invertSnap))
        target = Geometry::snap(target);
    const QPointF snappedDelta = target - m_dragAnchor;

    QVector<Item> items;
    for (const Item &item : m_dragOriginal)
        items.append(translated(item, snappedDelta));
    previewDrag(items);
}

void Editor::endMove()
{
    endDrag(tr("Move"));
}

// `horizontal` and `vertical` say which sides follow the mouse: -1 = left/top, 1 = right/bottom,
// 0 = neither. Dragging a corner moves two sides.
void Editor::beginResize(const QString &id, int horizontal, int vertical)
{
    const Node *n = m_document.node(id);
    if (!n)
        return;
    setSelection({id});
    m_resizeSides = QPoint(horizontal, vertical);
    beginDrag({*n});
}

void Editor::updateResize(QPointF delta, bool invertSnap)
{
    if (m_dragOriginal.isEmpty())
        return;
    Node n = std::get<Node>(m_dragOriginal.first());
    QRectF &r = n.rect;
    const bool snap = snapping(invertSnap);
    auto place = [snap](qreal side) { return snap ? Geometry::snap(side) : side; };

    if (m_resizeSides.x() < 0)
        r.setLeft(std::min(place(r.left() + delta.x()), r.right() - MinimumSize.width()));
    else if (m_resizeSides.x() > 0)
        r.setRight(std::max(place(r.right() + delta.x()), r.left() + MinimumSize.width()));
    if (m_resizeSides.y() < 0)
        r.setTop(std::min(place(r.top() + delta.y()), r.bottom() - MinimumSize.height()));
    else if (m_resizeSides.y() > 0)
        r.setBottom(std::max(place(r.bottom() + delta.y()), r.top() + MinimumSize.height()));
    previewDrag({n});
}

void Editor::endResize()
{
    endDrag(tr("Resize"));
}

// With `insert`, a new Waypoint is created at the midpoint `index` (between control point
// `index` and `index + 1`) and then dragged.
void Editor::beginWaypointDrag(const QString &connectionId, int index, bool insert)
{
    const Connection *c = m_document.connection(connectionId);
    if (!c || !m_document.node(c->fromNode) || !m_document.node(c->toNode))
        return;
    const Connection original = *c;
    Connection current = original;
    if (insert) {
        const auto route = Geometry::route(m_document.node(c->fromNode)->rect, m_document.node(c->toNode)->rect, *c);
        if (index < 0 || index >= route.midpoints.size())
            return;
        current.waypoints.insert(index, route.midpoints[index]);
    }
    if (index < 0 || index >= current.waypoints.size())
        return;

    beginDrag({original});
    m_dragWaypoint = index;
    m_dragAnchor = current.waypoints[index];
    previewDrag({current});
}

void Editor::updateWaypointDrag(QPointF delta, bool invertSnap)
{
    if (m_dragCurrent.isEmpty() || m_dragWaypoint < 0)
        return;
    Connection c = std::get<Connection>(m_dragCurrent.first());
    QPointF point = m_dragAnchor + delta;
    c.waypoints[m_dragWaypoint] = snapping(invertSnap) ? Geometry::snap(point) : point;
    previewDrag({c});
}

void Editor::endWaypointDrag()
{
    m_dragWaypoint = -1;
    endDrag(tr("Move waypoint"));
}

// ---- Connections ----

// The topmost Card under the point. `margin` widens each card so that a point on the outer
// half of a Handle still counts. Groups are never targets: dropping inside a Group means
// "new card here".
QString Editor::cardAt(QPointF point, qreal margin) const
{
    const QVector<Node> &nodes = m_document.nodes();
    for (auto it = nodes.crbegin(); it != nodes.crend(); ++it) {
        if (it->isCard() && it->rect.adjusted(-margin, -margin, margin, margin).contains(point))
            return it->id;
    }
    return {};
}

QString Editor::handleAt(const QString &nodeId, QPointF point, qreal radius) const
{
    const Node *n = m_document.node(nodeId);
    return n ? sideToString(Geometry::handleAt(n->rect, point, radius)) : QString();
}

QString Editor::connectionAt(QPointF point, qreal tolerance) const
{
    const QVector<Connection> &connections = m_document.connections();
    for (auto it = connections.crbegin(); it != connections.crend(); ++it) {
        const Node *from = m_document.node(it->fromNode);
        const Node *to = m_document.node(it->toNode);
        if (!from || !to)
            continue;
        // The label counts too: it can stick out far from the line.
        const Geometry::Route route = Geometry::route(from->rect, to->rect, *it);
        if (Geometry::hits(route, point, tolerance) || CanvasPainter::labelRect(route, it->label).contains(point))
            return it->id;
    }
    return {};
}

void Editor::setPreview(const QString &from, QPointF to, const QString &target, const QString &side)
{
    m_preview = {true, from, to, target, sideFromString(side)};
    emit previewChanged();
}

void Editor::clearPreview()
{
    m_preview = {};
    emit previewChanged();
}

void Editor::connectNodes(const QString &from, const QString &to, const QString &toSide)
{
    if (from == to || !m_document.node(from) || !m_document.node(to))
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
    if (!m_document.node(from))
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
    const Connection *existing = m_document.connection(connectionId);
    if (!existing || !m_document.node(nodeId))
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
    const Connection *existing = m_document.connection(connectionId);
    if (!existing || index < 0 || index >= existing->waypoints.size())
        return;
    Connection c = *existing;
    c.waypoints.removeAt(index);
    commit({{*existing, c}}, tr("Remove waypoint"));
}

void Editor::setConnectionStyle(const QString &style)
{
    Transaction t;
    for (const Connection &c : selectedConnections()) {
        Connection changed = c;
        changed.style = pathStyleFromString(style, c.style);
        t.append({c, changed});
    }
    commit(t, tr("Change path style"));
}

void Editor::setArrows(bool fromArrow, bool toArrow)
{
    Transaction t;
    for (const Connection &c : selectedConnections()) {
        Connection changed = c;
        changed.fromArrow = fromArrow;
        changed.toArrow = toArrow;
        t.append({c, changed});
    }
    commit(t, tr("Change arrows"));
}

void Editor::setConnectionLabel(const QString &connectionId, const QString &label)
{
    const Connection *existing = m_document.connection(connectionId);
    const QString trimmed = label.trimmed();   // a label of only spaces would be an empty box
    if (!existing || existing->label == trimmed)
        return;
    Connection c = *existing;
    c.label = trimmed;
    commit({{*existing, c}}, tr("Edit label"));
}

void Editor::setLabels(const QString &label)
{
    Transaction t;
    for (const QString &id : m_selection) {
        if (const Node *n = m_document.node(id); n && n->kind == NodeKind::Group) {
            Node changed = *n;
            changed.label = label;
            t.append({*n, changed});
        } else if (const Connection *c = m_document.connection(id)) {
            Connection changed = *c;
            changed.label = label;
            t.append({*c, changed});
        }
    }
    commit(t, tr("Edit label"));
}

void Editor::removeConnectionLabels()
{
    Transaction t;
    for (const Connection &c : selectedConnections()) {
        Connection changed = c;
        changed.label.clear();
        t.append({c, changed});
    }
    commit(t, tr("Remove label"));
}

// ---- Clipboard ----
// The clipboard holds a small JSON Canvas: the selected nodes plus the Connections between them.

// The nodes copy() takes: the selected ones, and a selected Group brings the cards inside it along.
QSet<QString> Editor::copiedNodeIds() const
{
    QSet<QString> ids;
    for (const QString &id : m_selection) {
        if (const Node *n = m_document.node(id)) {
            ids.insert(id);
            if (n->kind == NodeKind::Group) {
                for (const QString &inside : m_document.nodesInside(*n))
                    ids.insert(inside);
            }
        }
    }
    return ids;
}

QJsonObject Editor::selectionToJson() const
{
    const QSet<QString> ids = copiedNodeIds();
    QJsonArray nodes, edges;
    for (const Node &n : m_document.nodes()) {   // document order keeps Groups behind their cards
        if (ids.contains(n.id))
            nodes.append(n.toJson());
    }
    for (const Connection &c : m_document.connections()) {
        if (ids.contains(c.fromNode) && ids.contains(c.toNode))
            edges.append(c.toJson(c.fromSide, c.toSide));
    }
    return {{"nodes", nodes}, {"edges", edges}};
}

// Adds the nodes and Connections in `json` with fresh ids, moved by `offset`.
void Editor::insertJson(const QJsonObject &json, QPointF offset)
{
    QHash<QString, QString> newIds;
    Transaction t;
    QStringList inserted;

    for (const QJsonValue &value : json["nodes"].toArray()) {
        Node n = Node::fromJson(value.toObject());
        newIds[n.id] = Document::newId();
        n.id = newIds[n.id];
        n.rect.translate(offset);
        t.append({std::nullopt, n});
        inserted.append(n.id);
    }
    for (const QJsonValue &value : json["edges"].toArray()) {
        Connection c = Connection::fromJson(value.toObject());
        if (!newIds.contains(c.fromNode) || !newIds.contains(c.toNode))
            continue;
        c.id = Document::newId();
        c.fromNode = newIds[c.fromNode];
        c.toNode = newIds[c.toNode];
        for (QPointF &p : c.waypoints)
            p += offset;
        t.append({std::nullopt, c});
    }
    commit(t, tr("Paste"));
    setSelection(inserted);
}

// Removes exactly what copy() puts on the clipboard, so a cut Group takes its cards along
// (Delete removes only the Group).
void Editor::cut()
{
    const QSet<QString> ids = copiedNodeIds();
    if (ids.isEmpty())
        return;
    copy();
    Transaction t;
    for (const Connection &c : m_document.connections()) {
        if (ids.contains(c.fromNode) || ids.contains(c.toNode))
            t.append({c, std::nullopt});
    }
    for (const Node &n : m_document.nodes()) {
        if (ids.contains(n.id))
            t.append({n, std::nullopt});
    }
    commit(t, tr("Cut"));
}

void Editor::copy()
{
    const QJsonObject json = selectionToJson();
    if (!json["nodes"].toArray().isEmpty())
        QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact)));
}

void Editor::paste(QPointF center)
{
    const QString text = QGuiApplication::clipboard()->text();
    const QJsonObject json = QJsonDocument::fromJson(text.toUtf8()).object();
    QRectF bounds;
    for (const QJsonValue &value : json["nodes"].toArray())
        bounds = bounds.united(Node::fromJson(value.toObject()).rect);
    if (!bounds.isEmpty()) {
        const QPointF offset = center - bounds.center();
        insertJson(json, snapping(false) ? Geometry::snap(offset) : offset);
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
    insertJson(selectionToJson(), QPointF(offset, offset));
}

// ---- Files ----

QString Editor::libraryFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/Library");
}

void Editor::start()
{
    m_library.load();
    loadCurrent();   // in case load() found nothing to open
}

// Shows the Library's current Canvas. A Canvas that can't be read is shown as an error and
// is never saved, so the file stays as it was.
void Editor::loadCurrent()
{
    const QString path = m_library.currentPath();
    if (path == m_filePath && !path.isEmpty())
        return;   // the open file was only renamed or moved; canvasMoved() has followed it
    if (m_saveTimer.isActive())
        saveNow();

    setSelection({});
    clearPreview();
    QString error;
    if (path.isEmpty())
        error = tr("The library folder %1 can't be written to.").arg(m_library.root());
    else if (!m_document.load(path, &error))
        error = tr("%1 can't be read: %2").arg(QFileInfo(path).fileName(), error);
    if (!error.isEmpty())
        m_document.clear();
    m_loadError = error;
    setFilePath(path);

    // Its undo history from earlier in this session, unless the file changed since then.
    QUndoStack *&stack = m_stacks[path];
    const QDateTime modified = QFileInfo(path).lastModified();
    if (!stack) {
        stack = new QUndoStack(&m_history);
        m_history.addStack(stack);
    } else if (m_lastWritten.value(path) != modified) {
        stack->clear();
    }
    m_lastWritten[path] = modified;
    m_history.setActiveStack(stack);
    emit documentLoaded();
}

void Editor::canvasMoved(const QString &oldPath, const QString &newPath)
{
    if (QUndoStack *stack = m_stacks.take(oldPath))
        m_stacks[newPath] = stack;
    if (m_lastWritten.contains(oldPath))
        m_lastWritten[newPath] = m_lastWritten.take(oldPath);
    if (m_filePath == oldPath)
        setFilePath(newPath);
}

void Editor::canvasRemoved(const QString &path)
{
    delete m_stacks.take(path);   // also leaves m_history
    m_lastWritten.remove(path);
    if (m_filePath == path) {
        // Nothing may be written there any more. A new Canvas may later get the same path
        // (delete the only Project, and a new "Untitled" appears): it must be loaded afresh.
        m_saveTimer.stop();
        setFilePath({});
    }
}

void Editor::save()
{
    saveNow();
}

void Editor::exportCanvas(const QUrl &url)
{
    QString path = url.toLocalFile();
    if (!path.endsWith(".canvas"))
        path += ".canvas";
    QString error;
    if (!m_document.save(path, &error))
        emit errorOccurred(tr("Could not save %1: %2").arg(path, error));
}

QVariantMap Editor::savedView() const
{
    const auto view = m_library.view(m_library.currentProject(), m_library.currentCanvas());
    if (!view)
        return {};
    return {{"x", view->center.x()}, {"y", view->center.y()}, {"zoom", view->zoom}};
}

void Editor::rememberView(QPointF center, qreal zoom)
{
    m_library.setView(m_library.currentProject(), m_library.currentCanvas(), {center, zoom});
    m_viewSaveTimer.start();
}

void Editor::exportImage(const QUrl &url, const QVariantMap &palette)
{
    // Exports the selection, or the whole Canvas when nothing is selected.
    QStringList ids;
    for (const QString &id : m_selection) {
        if (const Node *n = m_document.node(id)) {
            ids.append(id);
            if (n->kind == NodeKind::Group)
                ids += m_document.nodesInside(*n);
        }
    }
    QString error;
    if (!CanvasPainter::exportCanvas(m_document, ids, url.toLocalFile(), Palette::fromVariantMap(palette), &error))
        emit errorOccurred(error);
}

QRectF Editor::contentBounds() const
{
    QRectF bounds;
    for (const Node &n : m_document.nodes())
        bounds = bounds.united(n.rect);
    return bounds;
}

void Editor::setFilePath(const QString &path)
{
    if (path == m_filePath)
        return;
    m_filePath = path;
    emit filePathChanged();
}

void Editor::scheduleSave()
{
    m_saveTimer.start();
}

void Editor::saveNow()
{
    m_saveTimer.stop();
    if (m_filePath.isEmpty() || !m_loadError.isEmpty())
        return;
    QString error;
    if (!m_document.save(m_filePath, &error)) {
        emit errorOccurred(tr("Could not save %1: %2").arg(m_filePath, error));
        return;
    }
    m_lastWritten[m_filePath] = QFileInfo(m_filePath).lastModified();
}
