#pragma once

#include "CanvasItems.h"

#include <QObject>
#include <optional>

// One edit to one item. Every modification of a Document is expressed as Changes, which is
// what makes undo trivial (swap before/after) and what phase 2 will send over the network.
//   add:    before empty, after set
//   remove: before set,   after empty
//   update: both set
struct Change
{
    std::optional<Item> before;
    std::optional<Item> after;

    Change inverted() const { return {after, before}; }
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

    // Cards (and Groups) lying completely inside the given Group, including nested ones.
    QStringList nodesInside(const Node &group) const;
    QVector<Connection> connectionsOf(const QString &nodeId) const;

    void apply(const Change &change);
    void apply(const Transaction &transaction);

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &json);
    void clear();

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
