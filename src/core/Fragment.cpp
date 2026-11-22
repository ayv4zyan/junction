#include "Fragment.h"

#include <QJsonArray>

namespace Fragment {

QJsonObject copy(const Document &document, const QSet<QString> &nodeIds)
{
    QJsonArray nodes, edges;
    for (const Node &n : document.nodes()) {
        if (nodeIds.contains(n.id))
            nodes.append(n.toJson());
    }
    for (const Connection &c : document.connections()) {
        if (nodeIds.contains(c.fromNode) && nodeIds.contains(c.toNode))
            edges.append(c.toJson(c.fromSide, c.toSide));
    }
    return {{"nodes", nodes}, {"edges", edges}};
}

QRectF bounds(const QJsonObject &fragment)
{
    QRectF result;
    for (const QJsonValue &value : fragment["nodes"].toArray())
        result = result.united(Node::fromJson(value.toObject()).rect);
    return result;
}

Insertion insertion(const QJsonObject &fragment, QPointF offset)
{
    Insertion result;
    QHash<QString, QString> newIds;   // old id -> new id

    for (const QJsonValue &value : fragment["nodes"].toArray()) {
        Node n = Node::fromJson(value.toObject());
        n.id = newIds[n.id] = Document::newId();
        n.rect.translate(offset);
        result.transaction.append({std::nullopt, n});
        result.nodeIds.append(n.id);
    }
    for (const QJsonValue &value : fragment["edges"].toArray()) {
        Connection c = Connection::fromJson(value.toObject());
        if (!newIds.contains(c.fromNode) || !newIds.contains(c.toNode))
            continue;
        c.id = Document::newId();
        c.fromNode = newIds[c.fromNode];
        c.toNode = newIds[c.toNode];
        for (QPointF &p : c.waypoints)
            p += offset;
        result.transaction.append({std::nullopt, c});
    }
    return result;
}

} // namespace Fragment
