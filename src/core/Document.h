#pragma once

#include "CanvasItems.h"
#include "Geometry.h"

#include <QObject>
#include <QSet>
#include <optional>

// One edit to one item: it is added, removed, or updated. Every modification of a Document is
// expressed as Changes, which is what makes undo trivial (swap before and after) and what
// phase 2 will send over the network. A Change always has at least one side.
class Change
{
public:
    static Change added(const Item &item) { return {std::nullopt, item}; }
    static Change removed(const Item &item) { return {item, std::nullopt}; }
    static Change updated(const Item &before, const Item &after) { return {before, after}; }

    const std::optional<Item> &before() const { return m_before; }   // empty when added
    const std::optional<Item> &after() const { return m_after; }     // empty when removed
    bool changesNothing() const { return m_before == m_after; }
    Change inverted() const { return {m_after, m_before}; }

private:
    Change(std::optional<Item> before, std::optional<Item> after)
        : m_before(std::move(before)), m_after(std::move(after))
    {
    }

    std::optional<Item> m_before;
    std::optional<Item> m_after;
};

// A user action: all its Changes are done and undone together.
using Transaction = QVector<Change>;

Transaction inverted(const Transaction &transaction);

// The Canvas in memory. Order in the vectors is drawing order (last = on top).
class Document : public QObject
{
    Q_OBJECT

public:
    explicit Document(QObject *parent = nullptr);

    const QVector<Node> &nodes() const { return m_nodes; }
    const QVector<Connection> &connections() const { return m_connections; }
    const Node *node(const QString &id) const;
    const Connection *connection(const QString &id) const;
    std::optional<Item> item(const QString &id) const;

    // The given nodes plus every node lying completely inside the Groups among them (nested
    // ones too). Other ids are left out. This is what moves, copies and exports with a selection.
    QSet<QString> nodesWithContents(const QStringList &ids) const;
    // The path a Connection takes between its nodes, or nothing if one of them is missing.
    std::optional<Geometry::Route> route(const Connection &connection) const;

    void apply(const Change &change);
    void apply(const Transaction &transaction);

    QJsonObject toJson() const;
    // Items without an id, or with one already taken, get a fresh id: everything finds items by id.
    void fromJson(const QJsonObject &json);
    void clear();

    // Fails, leaving the Document as it was, if the file can't be read or isn't a JSON object.
    bool load(const QString &path, QString *error);
    bool save(const QString &path, QString *error) const;

    static QString newId();

signals:
    void nodeAdded(const QString &id);
    void nodeRemoved(const QString &id);
    void nodeChanged(const QString &id);
    void connectionsChanged();
    void reset();

private:
    void put(const Item &item);
    void remove(const Item &item);

    QVector<Node> m_nodes;
    QVector<Connection> m_connections;
};
