#include "CanvasItems.h"

#include <QCoreApplication>
#include <QJsonArray>

QString sideToString(Side side)
{
    switch (side) {
    case Side::Top: return QStringLiteral("top");
    case Side::Right: return QStringLiteral("right");
    case Side::Bottom: return QStringLiteral("bottom");
    case Side::Left: return QStringLiteral("left");
    case Side::Floating: break;
    }
    return {};
}

Side sideFromString(const QString &text)
{
    if (text == u"top") return Side::Top;
    if (text == u"right") return Side::Right;
    if (text == u"bottom") return Side::Bottom;
    if (text == u"left") return Side::Left;
    return Side::Floating;
}

QString pathStyleToString(PathStyle style)
{
    switch (style) {
    case PathStyle::Straight: return QStringLiteral("straight");
    case PathStyle::Curved: return QStringLiteral("curved");
    case PathStyle::Elbow: return QStringLiteral("elbow");
    }
    return {};
}

PathStyle pathStyleFromString(const QString &text, PathStyle fallback)
{
    if (text == u"straight") return PathStyle::Straight;
    if (text == u"curved") return PathStyle::Curved;
    if (text == u"elbow") return PathStyle::Elbow;
    return fallback;
}

NodeKind Node::kind() const
{
    if (std::holds_alternative<GroupContent>(content))
        return NodeKind::Group;
    if (std::holds_alternative<UnsupportedContent>(content))
        return NodeKind::Unsupported;
    return NodeKind::Text;
}

QString Node::text() const
{
    const auto *card = std::get_if<TextContent>(&content);
    return card ? card->text : QString();
}

QString Node::label() const
{
    const auto *group = std::get_if<GroupContent>(&content);
    return group ? group->label : QString();
}

QString GroupContent::defaultLabel()
{
    return QCoreApplication::translate("Group", "Group");
}

QString Node::displayText() const
{
    if (const auto *card = std::get_if<TextContent>(&content)) {
        // In plain Markdown a newline is a space; two spaces before it make it a hard break.
        return QString(card->text).replace('\n', QStringLiteral("  \n"));
    }
    if (const auto *group = std::get_if<GroupContent>(&content))
        return group->label.isEmpty() ? GroupContent::defaultLabel() : group->label;
    const auto &unsupported = std::get<UnsupportedContent>(content);
    return QCoreApplication::translate("Node", "Unsupported: %1").arg(unsupported.raw["type"].toString());
}

QJsonObject Node::toJson() const
{
    const auto *unsupported = std::get_if<UnsupportedContent>(&content);
    QJsonObject json = unsupported ? unsupported->raw : QJsonObject();
    json["id"] = id;
    json["x"] = qRound(rect.x());
    json["y"] = qRound(rect.y());
    json["width"] = qRound(rect.width());
    json["height"] = qRound(rect.height());
    json.remove("color");
    if (!color.isEmpty())
        json["color"] = color;

    if (const auto *card = std::get_if<TextContent>(&content)) {
        json["type"] = "text";
        json["text"] = card->text;
    } else if (const auto *group = std::get_if<GroupContent>(&content)) {
        json["type"] = "group";
        if (!group->label.isEmpty())
            json["label"] = group->label;
    }
    return json;
}

Node Node::fromJson(const QJsonObject &json)
{
    Node node;
    node.id = json["id"].toString();
    // A file can say anything; a size that is missing or not positive becomes the smallest one.
    const auto size = [&](const char *key, qreal minimum) {
        const qreal value = json[key].toDouble();
        return value > 0 ? value : minimum;
    };
    node.rect = QRectF(json["x"].toDouble(), json["y"].toDouble(),
                       size("width", MinimumSize.width()), size("height", MinimumSize.height()));
    node.color = json["color"].toString();

    const QString type = json["type"].toString();
    if (type == u"text")
        node.content = TextContent{json["text"].toString()};
    else if (type == u"group")
        node.content = GroupContent{json["label"].toString()};
    else
        node.content = UnsupportedContent{json};
    return node;
}

QJsonObject Connection::toJson(Side resolvedFrom, Side resolvedTo) const
{
    QJsonObject json;
    json["id"] = id;
    json["fromNode"] = fromNode;
    json["fromSide"] = sideToString(resolvedFrom);
    json["toNode"] = toNode;
    json["toSide"] = sideToString(resolvedTo);
    json["fromEnd"] = fromArrow ? "arrow" : "none";
    json["toEnd"] = toArrow ? "arrow" : "none";
    if (!color.isEmpty())
        json["color"] = color;
    if (!label.isEmpty())
        json["label"] = label;

    // Junction's own fields; other apps ignore them.
    json["pathStyle"] = pathStyleToString(style);
    json["fromPinned"] = fromSide != Side::Floating;
    json["toPinned"] = toSide != Side::Floating;
    if (!waypoints.isEmpty()) {
        QJsonArray points;
        for (const QPointF &p : waypoints)
            points.append(QJsonObject{{"x", qRound(p.x())}, {"y", qRound(p.y())}});
        json["waypoints"] = points;
    }
    return json;
}

Connection Connection::fromJson(const QJsonObject &json)
{
    Connection c;
    c.id = json["id"].toString();
    c.fromNode = json["fromNode"].toString();
    c.toNode = json["toNode"].toString();
    c.fromArrow = json["fromEnd"].toString() == u"arrow";
    c.toArrow = json["toEnd"].toString() != u"none";   // JSON Canvas default is an arrow
    c.color = json["color"].toString();
    c.label = json["label"].toString();

    // A file without "pathStyle" was written by another app (Obsidian). Its sides were
    // chosen by that app, so we let both ends float instead of keeping them.
    const bool writtenByJunction = json.contains("pathStyle");
    c.style = pathStyleFromString(json["pathStyle"].toString(), PathStyle::Curved);
    if (writtenByJunction) {
        if (json["fromPinned"].toBool())
            c.fromSide = sideFromString(json["fromSide"].toString());
        if (json["toPinned"].toBool())
            c.toSide = sideFromString(json["toSide"].toString());
    }
    for (const QJsonValue &value : json["waypoints"].toArray()) {
        const QJsonObject p = value.toObject();
        c.waypoints.append(QPointF(p["x"].toDouble(), p["y"].toDouble()));
    }
    return c;
}

QRectF boundingRect(const QVector<Node> &nodes)
{
    QRectF bounds;
    for (const Node &n : nodes)
        bounds = bounds.united(n.rect);
    return bounds;
}

QString itemId(const CanvasItem &item)
{
    return std::visit([](const auto &i) { return i.id; }, item);
}
