#include "Document.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSaveFile>
#include <algorithm>

Transaction inverted(const Transaction &transaction)
{
    Transaction result;
    for (auto it = transaction.crbegin(); it != transaction.crend(); ++it)
        result.append(it->inverted());
    return result;
}

Document::Document(QObject *parent)
    : QObject(parent)
{
}

const Node *Document::node(const QString &id) const
{
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const Node &n) { return n.id == id; });
    return it == m_nodes.end() ? nullptr : &*it;
}

const Connection *Document::connection(const QString &id) const
{
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
                           [&](const Connection &c) { return c.id == id; });
    return it == m_connections.end() ? nullptr : &*it;
}

std::optional<Item> Document::item(const QString &id) const
{
    if (const Node *n = node(id))
        return *n;
    if (const Connection *c = connection(id))
        return *c;
    return std::nullopt;
}

QSet<QString> Document::nodesWithContents(const QStringList &ids) const
{
    QSet<QString> result;
    for (const QString &id : ids) {
        const Node *selected = node(id);
        if (!selected)
            continue;
        result.insert(id);
        if (selected->kind != NodeKind::Group)
            continue;
        for (const Node &n : m_nodes) {
            if (selected->rect.contains(n.rect))
                result.insert(n.id);
        }
    }
    return result;
}

std::optional<Geometry::Route> Document::route(const Connection &connection) const
{
    const Node *from = node(connection.fromNode);
    const Node *to = node(connection.toNode);
    if (!from || !to)
        return std::nullopt;
    return Geometry::route(from->rect, to->rect, connection);
}

void Document::apply(const Change &change)
{
    if (change.after())
        put(*change.after());
    else
        remove(*change.before());
}

void Document::apply(const Transaction &transaction)
{
    for (const Change &change : transaction)
        apply(change);
}

// Inserts the item, or replaces the one with the same id.
void Document::put(const Item &item)
{
    if (const Node *n = std::get_if<Node>(&item)) {
        for (Node &existing : m_nodes) {
            if (existing.id == n->id) {
                existing = *n;
                emit nodeChanged(n->id);
                return;
            }
        }
        m_nodes.append(*n);
        emit nodeAdded(n->id);
    } else {
        const Connection &c = std::get<Connection>(item);
        for (Connection &existing : m_connections) {
            if (existing.id == c.id) {
                existing = c;
                emit connectionsChanged();
                return;
            }
        }
        m_connections.append(c);
        emit connectionsChanged();
    }
}

void Document::remove(const Item &item)
{
    const QString id = itemId(item);
    if (std::holds_alternative<Node>(item)) {
        if (m_nodes.removeIf([&](const Node &n) { return n.id == id; }) > 0)
            emit nodeRemoved(id);
    } else {
        if (m_connections.removeIf([&](const Connection &c) { return c.id == id; }) > 0)
            emit connectionsChanged();
    }
}

QJsonObject Document::toJson() const
{
    QJsonArray nodes;
    for (const Node &n : m_nodes)
        nodes.append(n.toJson());

    QJsonArray edges;
    for (const Connection &c : m_connections) {
        const Node *from = node(c.fromNode);
        const Node *to = node(c.toNode);
        if (!from || !to) {   // points at a node the file doesn't have: written back unchanged
            edges.append(c.toJson(c.fromSide, c.toSide));
            continue;
        }
        const auto [fromSide, toSide] = Geometry::resolveSides(from->rect, to->rect, c);
        edges.append(c.toJson(fromSide, toSide));
    }
    return QJsonObject{{"nodes", nodes}, {"edges", edges}};
}

void Document::fromJson(const QJsonObject &json)
{
    QSet<QString> taken;
    auto unique = [&](const QString &id) {
        const QString result = id.isEmpty() || taken.contains(id) ? newId() : id;
        taken.insert(result);
        return result;
    };

    m_nodes.clear();
    m_connections.clear();
    for (const QJsonValue &value : json["nodes"].toArray()) {
        Node n = Node::fromJson(value.toObject());
        n.id = unique(n.id);
        m_nodes.append(n);
    }
    for (const QJsonValue &value : json["edges"].toArray()) {
        Connection c = Connection::fromJson(value.toObject());
        c.id = unique(c.id);
        m_connections.append(c);
    }
    emit reset();
}

void Document::clear()
{
    fromJson({});
}

bool Document::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        *error = parseError.errorString();
        return false;
    }
    if (!json.isObject()) {   // valid JSON, but not a canvas: saving over it would lose it
        *error = tr("it is not a canvas file");
        return false;
    }
    fromJson(json.object());
    return true;
}

bool Document::save(const QString &path, QString *error) const
{
    QSaveFile file(path);   // writes to a temp file first, so a crash never leaves half a canvas
    if (!file.open(QIODevice::WriteOnly)) {
        *error = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

QString Document::newId()
{
    // Same shape as Obsidian's ids: 16 hex characters.
    return QString::number(QRandomGenerator::global()->generate64(), 16).rightJustified(16, '0');
}
