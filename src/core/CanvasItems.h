#pragma once

// The things that live on a Canvas (see GLOSSARY.md), and their JSON Canvas form.

#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>
#include <variant>

// In a namespace of their own only so that QML can use them too (registered by the app).
namespace CanvasEnums {
Q_NAMESPACE
enum class Side { Floating, Top, Right, Bottom, Left };   // Floating: no Pinned side, see GLOSSARY.md
Q_ENUM_NS(Side)
enum class PathStyle { Straight, Curved, Elbow };
Q_ENUM_NS(PathStyle)
enum class NodeKind { Text, Group, Unsupported };
Q_ENUM_NS(NodeKind)
} // namespace CanvasEnums
using CanvasEnums::Side;
using CanvasEnums::PathStyle;
using CanvasEnums::NodeKind;

QString sideToString(Side side);
Side sideFromString(const QString &text);
QString pathStyleToString(PathStyle style);
PathStyle pathStyleFromString(const QString &text, PathStyle fallback);

// What a node holds depends on its kind.
struct TextContent         // a Card
{
    QString text;          // Markdown
    bool operator==(const TextContent &) const = default;
};
struct GroupContent        // a Group
{
    QString label;         // its title; shown as defaultLabel() while empty
    static QString defaultLabel();
    bool operator==(const GroupContent &) const = default;
};
struct UnsupportedContent  // an Unsupported Card
{
    QJsonObject raw;       // the JSON it was loaded from, written back as-is
    bool operator==(const UnsupportedContent &) const = default;
};

// A Card, a Group or an Unsupported Card. JSON Canvas calls all of these "nodes".
struct Node
{
    static constexpr QSizeF MinimumSize{80, 40};   // smaller can't be resized or read

    QString id;
    QRectF rect;
    QString color;   // "" (default), "1".."6" (preset) or "#rrggbb"
    std::variant<TextContent, GroupContent, UnsupportedContent> content;

    NodeKind kind() const;
    bool isCard() const { return kind() != NodeKind::Group; }
    QString text() const;    // a Card's Markdown; "" for the other kinds
    QString label() const;   // a Group's title; "" for the other kinds
    // What it shows: a Card's text as Markdown (every newline a line break, as Obsidian shows
    // it), a Group's label (or its default), or an Unsupported Card's placeholder text.
    QString displayText() const;

    QJsonObject toJson() const;
    static Node fromJson(const QJsonObject &json);
    bool operator==(const Node &) const = default;
};

// JSON Canvas calls these "edges".
struct Connection
{
    QString id;
    QString fromNode;
    QString toNode;
    Side fromSide = Side::Floating;   // the Pinned side, or Floating
    Side toSide = Side::Floating;
    bool fromArrow = false;
    bool toArrow = true;
    QString color;
    QString label;
    PathStyle style = PathStyle::Elbow;
    QVector<QPointF> waypoints;

    // Floating ends are written with the side they currently use, so Obsidian draws them right.
    QJsonObject toJson(Side resolvedFrom, Side resolvedTo) const;
    static Connection fromJson(const QJsonObject &json);
    bool operator==(const Connection &) const = default;
};

using CanvasItem = std::variant<Node, Connection>;

QRectF boundingRect(const QVector<Node> &nodes);   // around all of them; empty for none

QString itemId(const CanvasItem &item);
