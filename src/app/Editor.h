#pragma once

// The bridge between QML and the core: owns the Library, the open Canvas and the selection,
// and turns what the user does into Transactions. QML calls these methods in canvas (world)
// coordinates; it never edits the Document directly, so every change goes through commit() and
// can be undone.
//
// The Editor always shows the Library's current Canvas: when another one is opened (or the
// open one is renamed, moved or deleted) the Library says so and the Editor follows.

#include "Document.h"
#include "Geometry.h"
#include "Library.h"
#include "LibraryModel.h"
#include "NodeModel.h"
#include "OpenCanvas.h"

#include <QObject>
#include <QQmlEngine>
#include <QRectF>
#include <QTimer>
#include <QUrl>
#include <optional>
#include <variant>

class Editor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main.cpp, which decides where the Library is")

    Q_PROPERTY(LibraryModel *library READ library CONSTANT)
    Q_PROPERTY(NodeModel *cards READ cards CONSTANT)
    Q_PROPERTY(NodeModel *groups READ groups CONSTANT)
    Q_PROPERTY(QStringList selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedConnection READ selectedConnection NOTIFY selectionDetailsChanged)
    Q_PROPERTY(QString selectionColor READ selectionColor NOTIFY selectionDetailsChanged)
    Q_PROPERTY(QVariantMap selectionInfo READ selectionInfo NOTIFY selectionDetailsChanged)
    Q_PROPERTY(QRectF selectionBounds READ selectionBounds NOTIFY selectionDetailsChanged)
    Q_PROPERTY(bool dragging READ isDragging NOTIFY draggingChanged)   // moving, resizing, or a Waypoint
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(QString loadError READ loadError NOTIFY documentLoaded)   // why the Canvas can't be shown
    Q_PROPERTY(bool snapToGrid READ snapToGrid WRITE setSnapToGrid NOTIFY snapToGridChanged)

public:
    // The Connection being dragged out of a Handle, drawn by ConnectionLayer. It ends on
    // `targetNode` when the pointer is over one, otherwise at the pointer.
    struct ConnectionPreview
    {
        QString fromNode;
        QPointF pointer;
        QString targetNode;
        Side targetSide = Side::None;
    };

    explicit Editor(const QString &libraryFolder, QObject *parent = nullptr);

    const Document &document() const { return m_canvas.document(); }
    const std::optional<ConnectionPreview> &connectionPreview() const { return m_preview; }
    LibraryModel *library() { return &m_libraryModel; }
    NodeModel *cards() { return &m_cards; }
    NodeModel *groups() { return &m_groups; }
    QStringList selection() const { return m_selection; }
    QVariantMap selectedConnection() const;
    QString selectionColor() const;   // color of the selected items, "" if mixed or default
    // What the right-click menus need to know about the selection: how many Cards, Groups and
    // Connections it holds, which of them have a Label, and the Label they share (if any).
    QVariantMap selectionInfo() const;
    // The world rectangle around everything selected (empty when nothing is); the floating
    // selection toolbar sits against it.
    QRectF selectionBounds() const;
    bool isDragging() const { return m_drag.has_value(); }
    bool canUndo() const { return m_canvas.canUndo(); }
    bool canRedo() const { return m_canvas.canRedo(); }
    QString filePath() const { return m_canvas.filePath(); }
    QString loadError() const { return m_canvas.loadError(); }
    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool snap);

    // Selection
    Q_INVOKABLE void select(const QString &id, bool toggle = false);
    // The selection becomes `base` plus what the rectangle catches; called while it is dragged.
    Q_INVOKABLE void selectInRect(const QRectF &rect, const QStringList &base);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();

    // Creating and editing
    Q_INVOKABLE QString createCard(QPointF center);
    Q_INVOKABLE QString createGroup(QPointF center);   // around the selection, if any
    Q_INVOKABLE void setText(const QString &id, const QString &text);   // Card text or Group label
    Q_INVOKABLE void setColor(const QString &color);
    Q_INVOKABLE void deleteSelection();
    Q_INVOKABLE void nudge(QPointF steps);   // arrow keys: moves the selection by whole grid steps

    // Dragging: begin, update with the total distance moved so far, end (= one undo step).
    // `invertSnap` is true while Alt is held.
    Q_INVOKABLE void beginMove(const QString &id);
    Q_INVOKABLE void updateMove(QPointF delta, bool invertSnap);
    Q_INVOKABLE void endMove();
    Q_INVOKABLE void beginResize(const QString &id, int horizontal, int vertical);
    Q_INVOKABLE void updateResize(QPointF delta, bool invertSnap);
    Q_INVOKABLE void endResize();
    Q_INVOKABLE void beginWaypointDrag(const QString &connectionId, int index, bool insert);
    Q_INVOKABLE void updateWaypointDrag(QPointF delta, bool invertSnap);
    Q_INVOKABLE void endWaypointDrag();

    // Connections
    Q_INVOKABLE QString cardAt(QPointF point, qreal margin = 0) const;
    Q_INVOKABLE QString handleAt(const QString &nodeId, QPointF point, qreal radius) const;
    Q_INVOKABLE QString connectionAt(QPointF point, qreal tolerance) const;
    // Where the floating selection toolbar goes (see Geometry::floatingPosition), in view coordinates.
    Q_INVOKABLE QPointF floatingPosition(const QRectF &selection, const QSizeF &size, const QRectF &view, qreal gap) const
    {
        return Geometry::floatingPosition(selection, size, view, gap);
    }
    Q_INVOKABLE void setConnectionPreview(const QString &fromNode, QPointF pointer, const QString &targetNode,
                                          const QString &targetSide);
    Q_INVOKABLE void clearConnectionPreview();
    Q_INVOKABLE void connectNodes(const QString &from, const QString &to, const QString &toSide);
    Q_INVOKABLE QString connectToNewCard(const QString &from, QPointF center);
    Q_INVOKABLE void reattach(const QString &connectionId, bool fromEnd, const QString &nodeId, const QString &side);
    Q_INVOKABLE void removeWaypoint(const QString &connectionId, int index);
    Q_INVOKABLE void setConnectionStyle(const QString &style);
    Q_INVOKABLE void setArrows(bool fromArrow, bool toArrow);
    Q_INVOKABLE void setConnectionLabel(const QString &connectionId, const QString &label);
    Q_INVOKABLE void setLabels(const QString &label);   // of every selected Group and Connection
    Q_INVOKABLE void removeConnectionLabels();          // of every selected Connection

    // Clipboard and history
    Q_INVOKABLE void cut();
    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste(QPointF center);
    Q_INVOKABLE void duplicate();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Files. Every Canvas is saved automatically, shortly after each change.
    Q_INVOKABLE void start();   // loads the Library and opens what was open last time
    Q_INVOKABLE void save();
    // Both return the file written, or an empty URL when that failed (errorOccurred says why).
    Q_INVOKABLE QUrl exportCanvas(const QUrl &url);   // a copy of the .canvas file
    Q_INVOKABLE QUrl exportImage(const QUrl &url, const QVariantMap &palette);
    Q_INVOKABLE QRectF contentBounds() const;

    // The part of the Canvas on screen, kept per Canvas so switching back returns to it.
    // savedView() is {x, y, zoom} with (x, y) the world point at the center, or empty.
    Q_INVOKABLE QVariantMap savedView() const;
    Q_INVOKABLE void rememberView(QPointF center, qreal zoom);

signals:
    void selectionChanged();
    void selectionDetailsChanged();   // the selection, or something about the selected items
    void historyChanged();
    void filePathChanged();
    void snapToGridChanged();
    void previewChanged();
    void documentLoaded();
    void draggingChanged();
    void errorOccurred(const QString &message);

private:
    // A drag in progress. While it lasts, the Document shows `current` without an undo step;
    // on release, one Transaction from `original` to `current` goes on the undo stack.
    struct MoveDrag
    {
        QPointF anchor;   // the grabbed node's top-left corner, which snaps to the grid
    };
    struct ResizeDrag
    {
        int horizontal;   // which sides follow the mouse, see beginResize()
        int vertical;
    };
    struct WaypointDrag
    {
        int index;        // of the Waypoint in the Connection
        QPointF anchor;   // where it was, which snaps to the grid
    };
    struct Drag
    {
        QVector<Item> original;
        QVector<Item> current;
        std::variant<MoveDrag, ResizeDrag, WaypointDrag> kind;
    };

    void commit(const Transaction &transaction, const QString &text);
    void setSelection(const QStringList &ids);
    void pruneSelection();
    bool snapping(bool invert) const { return m_snapToGrid != invert; }

    Node newCard(QPointF center, const QString &text = {}) const;
    QVector<Item> itemsMovingWithSelection() const;
    template <typename Kind>
    const Kind *activeDrag() const;   // the drag in progress, if it is of that kind
    void beginDrag(const QVector<Item> &items, const decltype(Drag::kind) &kind);
    void showDrag(const QVector<Item> &items);
    void endDrag(const QString &text);

    QVector<Connection> selectedConnections() const;
    void insertFragment(const QJsonObject &fragment, QPointF offset);

    void loadCurrent();

    Library m_library;
    LibraryModel m_libraryModel{&m_library};
    OpenCanvas m_canvas;
    NodeModel m_cards{&m_canvas.document(), NodeModel::Cards};
    NodeModel m_groups{&m_canvas.document(), NodeModel::Groups};
    QStringList m_selection;
    std::optional<ConnectionPreview> m_preview;
    std::optional<Drag> m_drag;
    bool m_snapToGrid = true;
    QTimer m_viewSaveTimer;   // writes the index a moment after the view stops moving
};
