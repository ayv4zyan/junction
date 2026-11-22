#pragma once

// The bridge between QML and the open Canvas: owns it and the Selection, and turns what the
// user does on the Canvas into Transactions. QML calls these methods in canvas (world)
// coordinates; it never edits the Document directly, so every change goes through commit() and
// can be undone.
//
// The Editor always shows the Library's current Canvas: when another one is opened (or the
// open one is renamed, moved or deleted) the Library says so and the Editor follows.

#include "Document.h"
#include "Fragment.h"
#include "Geometry.h"
#include "Library.h"
#include "NodeModel.h"
#include "Selection.h"
#include "CanvasSession.h"

#include <QClipboard>
#include <QObject>
#include <QQmlEngine>
#include <QRectF>
#include <optional>
#include <variant>

class Editor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main.cpp")

    Q_PROPERTY(NodeModel *cards READ cards CONSTANT)
    Q_PROPERTY(NodeModel *groups READ groups CONSTANT)
    Q_PROPERTY(Selection *selection READ selection CONSTANT)
    Q_PROPERTY(bool dragging READ isDragging NOTIFY draggingChanged)   // moving, resizing, or a Waypoint
    Q_PROPERTY(bool snapToGrid READ snapToGrid WRITE setSnapToGrid NOTIFY snapToGridChanged)

public:
    // Shows the Library's current Canvas, and follows it.
    // `clipboard` is the system clipboard, for cut, copy and paste.
    Editor(Library *library, QClipboard *clipboard, QObject *parent = nullptr);

    const Document &document() const { return m_session.document(); }
    CanvasSession *canvasSession() { return &m_session; }   // its file, history and whether it can be edited
    NodeModel *cards() { return &m_cards; }
    NodeModel *groups() { return &m_groups; }
    Selection *selection() { return &m_selection; }   // what the commands below act on
    bool isDragging() const { return m_drag.has_value(); }
    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool snap);

    // Creating and editing
    Q_INVOKABLE QString createCard(QPointF center);
    Q_INVOKABLE QString createGroup(QPointF center);   // around the selection, if any
    Q_INVOKABLE void setCardText(const QString &id, const QString &text);
    Q_INVOKABLE void setColor(const QString &color);
    Q_INVOKABLE void deleteSelection();
    Q_INVOKABLE void nudge(QPointF steps);   // arrow keys: moves the selection by whole grid steps

    // Dragging: begin, update with the total distance moved so far, end (= one undo step).
    // `invertSnap` is true while Alt is held.
    Q_INVOKABLE void beginMove(const QString &id);
    Q_INVOKABLE void updateMove(QPointF delta, bool invertSnap);
    Q_INVOKABLE void endMove();
    Q_INVOKABLE void beginResize(const QString &id, Qt::Edges edges);   // the sides that follow the mouse
    Q_INVOKABLE void updateResize(QPointF delta, bool invertSnap);
    Q_INVOKABLE void endResize();
    Q_INVOKABLE void beginWaypointDrag(const QString &connectionId, int index, bool insert);
    Q_INVOKABLE void updateWaypointDrag(QPointF delta, bool invertSnap);
    Q_INVOKABLE void endWaypointDrag();

    // Connections
    Q_INVOKABLE void connectNodes(const QString &from, const QString &to, CanvasEnums::Side toSide);
    Q_INVOKABLE QString connectToNewCard(const QString &from, QPointF center);
    Q_INVOKABLE void reattach(const QString &connectionId, bool fromEnd, const QString &nodeId, CanvasEnums::Side side);
    Q_INVOKABLE void removeWaypoint(const QString &connectionId, int index);
    Q_INVOKABLE void setConnectionStyle(CanvasEnums::PathStyle style);
    Q_INVOKABLE void setArrows(bool fromArrow, bool toArrow);
    // Of the Groups and Connections among `ids` (the others have no Label), in one undo step.
    Q_INVOKABLE void setLabels(const QStringList &ids, const QString &label);
    Q_INVOKABLE void removeConnectionLabels();          // of every selected Connection

    // Clipboard and history
    Q_INVOKABLE void cut();
    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste(QPointF center);
    Q_INVOKABLE void duplicate();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    Q_INVOKABLE QRectF contentBounds() const;

signals:
    void snapToGridChanged();
    void canvasOpened();   // another Canvas is shown; the Document was replaced
    void draggingChanged();

private:
    // A drag in progress. While it lasts, the Document shows where the items are now without
    // an undo step; on release, its Changes (from where they were) go on the undo stack.
    struct MoveDrag
    {
        QPointF anchor;   // the grabbed node's top-left corner, which snaps to the grid
    };
    struct ResizeDrag
    {
        Qt::Edges edges;   // the sides that follow the mouse; two at a corner
    };
    struct WaypointDrag
    {
        int index;        // of the Waypoint in the Connection
        QPointF anchor;   // where it starts, which snaps to the grid
        bool inserted;    // a new Waypoint at `index`, rather than an existing one
    };
    struct Drag
    {
        Transaction changes;   // before: as the drag began; after: where the item is now
        std::variant<MoveDrag, ResizeDrag, WaypointDrag> kind;
    };

    bool commit(const Transaction &transaction, const QString &text);
    QString addAndSelect(const Transaction &transaction, const QString &text, const QString &selected);
    // `point` on the grid when snapping (inverted while Alt is held), otherwise as it is.
    QPointF snapped(QPointF point, bool invert = false) const { return m_snapToGrid != invert ? Geometry::snap(point) : point; }
    qreal snapped(qreal value, bool invert) const { return m_snapToGrid != invert ? Geometry::snap(value) : value; }

    QRectF placed(QPointF center, QSizeF size) const;
    Node newCard(QPointF center, const QString &text = {}) const;
    Transaction removal(const QSet<QString> &nodeIds, const QStringList &connectionIds) const;
    QVector<CanvasItem> itemsMovingWithSelection() const;
    template <typename Kind>
    const Kind *activeDrag() const;   // the drag in progress, if it is of that kind
    bool beginDrag(const QVector<CanvasItem> &items, const decltype(Drag::kind) &kind);
    template <typename Moved>
    void moveDraggedItems(Moved moved);   // every dragged item becomes moved(item as the drag began)
    void endDrag(const QString &text);
    void cancelDrag();
    void showDraggedWaypointAt(QPointF point);

    template <typename EditNode, typename EditConnection>
    Transaction editedItems(const QStringList &ids, EditNode editNode, EditConnection editConnection) const;
    void insertFragment(const Fragment &fragment, QPointF offset);

    void loadCurrent();

    Library *m_library;
    QClipboard *m_clipboard;
    CanvasSession m_session;
    NodeModel m_cards{&m_session.document(), NodeModel::Cards};
    NodeModel m_groups{&m_session.document(), NodeModel::Groups};
    Selection m_selection{&m_session.document()};
    std::optional<Drag> m_drag;
    bool m_snapToGrid = true;
};
