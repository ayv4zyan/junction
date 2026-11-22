#include "Fragment.h"

#include <QHash>
#include <QJsonArray>

Fragment Fragment::copy(const Document &document, const QSet<QString> &nodeIds)
{
    Fragment fragment;
    for (const Node &n : document.nodes()) {
        if (nodeIds.contains(n.id))
            fragment.nodes.append(n);
    }
    for (const Connection &c : document.connections()) {
        if (nodeIds.contains(c.fromNode) && nodeIds.contains(c.toNode))
            fragment.connections.append(c);
    }
    return fragment;
}

Fragment Fragment::fromJson(const QJsonObject &json)
{
    Fragment fragment;
    QSet<QString> ids;
    for (const QJsonValue &value : json["nodes"].toArray()) {
        fragment.nodes.append(Node::fromJson(value.toObject()));
        ids.insert(fragment.nodes.last().id);
    }
    for (const QJsonValue &value : json["edges"].toArray()) {
        const Connection c = Connection::fromJson(value.toObject());
        if (ids.contains(c.fromNode) && ids.contains(c.toNode))   // others can't be inserted
            fragment.connections.append(c);
    }
    return fragment;
}

QJsonObject Fragment::toJson() const
{
    QJsonArray nodesJson, edgesJson;
    for (const Node &n : nodes)
        nodesJson.append(n.toJson());
    for (const Connection &c : connections)
        edgesJson.append(c.toJson(c.fromSide, c.toSide));
    return {{"nodes", nodesJson}, {"edges", edgesJson}};
}

QRectF Fragment::bounds() const
{
    return boundingRect(nodes);
}

Fragment::Insertion Fragment::insertion(QPointF offset) const
{
    Insertion result;
    QHash<QString, QString> newIds;   // old id -> new id
    for (Node n : nodes) {
        n.id = newIds[n.id] = Document::newId();
        n.rect.translate(offset);
        result.transaction.append(Change::added(n));
        result.nodeIds.append(n.id);
    }
    for (Connection c : connections) {
        c.id = Document::newId();
        c.fromNode = newIds.value(c.fromNode);
        c.toNode = newIds.value(c.toNode);
        for (QPointF &p : c.waypoints)
            p += offset;
        result.transaction.append(Change::added(c));
    }
    return result;
}
