#pragma once

// The things that live on a Canvas (see GLOSSARY.md), and their JSON Canvas form.

#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <variant>

// In a namespace of their own only so that QML can use them too (registered by the app).
namespace CanvasEnums {
Q_NAMESPACE
enum class Side { None, Top, Right, Bottom, Left };   // None = Floating End
Q_ENUM_NS(Side)
enum class PathStyle { Straight, Curved, Elbow };
Q_ENUM_NS(PathStyle)
} // namespace CanvasEnums
using CanvasEnums::Side;
using CanvasEnums::PathStyle;

enum class NodeKind { Text, Group, Unsupported };

QString sideToString(Side side);
Side sideFromString(const QString &text);
QString pathStyleToString(PathStyle style);
PathStyle pathStyleFromString(const QString &text, PathStyle fallback);

// A Card, a Group or an Unsupported Card. JSON Canvas calls all of these "nodes".
struct Node
{
    QString id;
    NodeKind kind = NodeKind::Text;
    QRectF rect;
    QString color;     // "" (default), "1".."6" (preset) or "#rrggbb"
    QString text;      // Card: Markdown
    QString label;     // Group: title
    QJsonObject raw;   // Unsupported Card: the JSON it was loaded from, written back as-is

    bool isCard() const { return kind != NodeKind::Group; }
    // A Card's text as Markdown with every newline a line break, as Obsidian shows it.
    QString markdown() const;
    // What an Unsupported Card shows in place of its content.
    QString unsupportedText() const;

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
    Side fromSide = Side::None;   // None means the end floats
    Side toSide = Side::None;
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

using Item = std::variant<Node, Connection>;

QString itemId(const Item &item);
