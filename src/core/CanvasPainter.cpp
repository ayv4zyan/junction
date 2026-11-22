#include "CanvasPainter.h"

#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QSvgGenerator>
#include <QTextDocument>

namespace {

constexpr qreal CardPadding = 12;
constexpr qreal ArrowLength = 12;
constexpr qreal LineWidth = 2;
constexpr int LabelFontSize = 13;      // LabelEditor.qml uses the same sizes
constexpr qreal LabelPaddingX = 6;
constexpr qreal LabelPaddingY = 3;

// The application font (Geist, see JunctionDesign::installFonts) at a pixel size, matching the
// sizes used in QML.
QFont font(int pixelSize, QFont::Weight weight = QFont::Normal)
{
    QFont f;
    f.setPixelSize(pixelSize);
    f.setWeight(weight);
    return f;
}

void drawArrowhead(QPainter &painter, QPointF tip, QPointF direction, const QColor &color)
{
    const QPointF base = tip - direction * ArrowLength;
    const QPointF across(-direction.y() * ArrowLength / 2, direction.x() * ArrowLength / 2);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPolygon(QPolygonF{tip, base + across, base - across});
}

void drawMarkdown(QPainter &painter, const QString &markdown, const QRectF &rect, const QColor &color)
{
    QTextDocument document;
    // Like Obsidian, every newline is a line break (in plain Markdown it would be a space).
    // Two spaces before a newline mark a hard break in Markdown.
    document.setMarkdown(QString(markdown).replace('\n', QStringLiteral("  \n")));
    document.setTextWidth(rect.width());
    document.setDefaultFont(font(15));

    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, color);
    context.clip = QRectF(QPointF(0, 0), rect.size());

    painter.save();
    painter.translate(rect.topLeft());
    painter.setClipRect(context.clip);
    document.documentLayout()->draw(&painter, context);
    painter.restore();
}

} // namespace

QColor Palette::resolve(const QString &color, const QColor &fallback) const
{
    if (color.startsWith('#'))
        return QColor(color);
    bool ok = false;
    const int index = color.toInt(&ok) - 1;
    if (ok && index >= 0 && index < presets.size())
        return presets[index];
    return fallback;
}

Palette Palette::fromVariantMap(const QVariantMap &map)
{
    Palette p;
    auto read = [&](const char *key, QColor &target) {
        if (map.contains(key))
            target = map[key].value<QColor>();
    };
    read("background", p.background);
    read("grid", p.grid);
    read("cardFill", p.cardFill);
    read("cardBorder", p.cardBorder);
    read("text", p.text);
    read("mutedText", p.mutedText);
    read("line", p.line);
    read("selection", p.selection);
    if (map.contains("radius"))
        p.radius = map["radius"].toReal();
    for (const QVariant &preset : map["presets"].toList())
        p.presets.append(preset.value<QColor>());
    return p;
}

namespace CanvasPainter {

void drawGrid(QPainter &painter, const QTransform &view, const QSizeF &screenSize, const Palette &palette)
{
    const qreal zoom = view.m11();
    qreal step = Geometry::GridSize;
    while (step * zoom < 12)   // don't draw dots closer than 12 px apart
        step *= 2;

    const QRectF world = view.inverted().mapRect(QRectF(QPointF(0, 0), screenSize));
    QVector<QPointF> dots;
    for (qreal x = std::floor(world.left() / step) * step; x <= world.right(); x += step) {
        for (qreal y = std::floor(world.top() / step) * step; y <= world.bottom(); y += step)
            dots.append(view.map(QPointF(x, y)));
    }
    painter.setPen(QPen(palette.grid, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawPoints(dots.constData(), dots.size());
}

void drawConnection(QPainter &painter, const Geometry::Route &route, const Connection &connection,
                    const Palette &palette, bool selected, bool showLabel)
{
    const QColor color = selected ? palette.selection : palette.resolve(connection.color, palette.line);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, selected ? LineWidth + 1 : LineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(route.path);

    if (connection.toArrow)
        drawArrowhead(painter, route.end, route.endDirection, color);
    if (connection.fromArrow)
        drawArrowhead(painter, route.start, route.startDirection, color);

    if (showLabel && !connection.label.isEmpty()) {
        const QRectF box = labelRect(route, connection.label);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette.background);
        painter.drawRoundedRect(box, palette.radius, palette.radius);
        painter.setFont(font(LabelFontSize));
        painter.setPen(palette.text);
        painter.drawText(box, Qt::AlignCenter, connection.label);
    }
}

QRectF labelRect(const Geometry::Route &route, const QString &label)
{
    if (label.isEmpty())
        return {};
    const QSizeF text = QFontMetricsF(font(LabelFontSize)).boundingRect(QRectF(), Qt::AlignCenter, label).size();
    const QSizeF box = text + QSizeF(2 * LabelPaddingX, 2 * LabelPaddingY);
    return QRectF(route.labelPosition - QPointF(box.width() / 2, box.height() / 2), box);
}

void drawNode(QPainter &painter, const Node &node, const Palette &palette)
{
    const QColor accent = palette.resolve(node.color, palette.cardBorder);
    painter.setPen(QPen(accent, 1.5));

    if (node.kind == NodeKind::Group) {
        QColor fill = node.color.isEmpty() ? palette.cardBorder : accent;
        fill.setAlphaF(0.08);
        painter.setBrush(fill);
        painter.drawRoundedRect(node.rect, palette.radius, palette.radius);
        painter.setFont(font(15, QFont::Bold));
        painter.setPen(palette.text);
        painter.drawText(node.rect.topLeft() + QPointF(4, -8), node.label);
        return;
    }

    painter.setBrush(palette.cardFill);
    painter.drawRoundedRect(node.rect, palette.radius, palette.radius);
    if (!node.color.isEmpty()) {   // colored cards get a light tint of their color
        QColor tint = accent;
        tint.setAlphaF(0.12);
        painter.setBrush(tint);
        painter.drawRoundedRect(node.rect, palette.radius, palette.radius);
    }

    const QRectF content = node.rect.adjusted(CardPadding, CardPadding, -CardPadding, -CardPadding);
    if (node.kind == NodeKind::Text) {
        drawMarkdown(painter, node.text, content, palette.text);
    } else {
        painter.setPen(palette.mutedText);
        painter.setFont(font(15));
        painter.drawText(content, Qt::AlignCenter | Qt::TextWordWrap,
                         QStringLiteral("Unsupported: %1").arg(node.raw["type"].toString()));
    }
}

bool exportCanvas(const Document &document, const QStringList &onlyIds, const QString &path,
                  const Palette &palette, QString *error)
{
    auto included = [&](const QString &id) { return onlyIds.isEmpty() || onlyIds.contains(id); };

    QVector<Node> groups, cards;
    QRectF bounds;
    for (const Node &n : document.nodes()) {
        if (!included(n.id))
            continue;
        (n.kind == NodeKind::Group ? groups : cards).append(n);
        bounds = bounds.united(n.rect);
    }
    if (bounds.isEmpty()) {
        *error = QStringLiteral("Nothing to export.");
        return false;
    }

    QVector<std::pair<Connection, Geometry::Route>> connections;
    for (const Connection &c : document.connections()) {
        const Node *from = document.node(c.fromNode);
        const Node *to = document.node(c.toNode);
        if (from && to && included(from->id) && included(to->id)) {
            connections.append({c, Geometry::route(from->rect, to->rect, c)});
            bounds = bounds.united(connections.last().second.path.boundingRect());
        }
    }
    bounds.adjust(-40, -60, 40, 40);   // extra room on top for Group labels

    auto paint = [&](QPainter &painter) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(bounds, palette.background);
        for (const Node &n : groups)
            drawNode(painter, n, palette);
        for (const auto &[connection, route] : connections)
            drawConnection(painter, route, connection, palette, false);
        for (const Node &n : cards)
            drawNode(painter, n, palette);
    };

    if (path.endsWith(".svg", Qt::CaseInsensitive)) {
        QSvgGenerator svg;
        svg.setFileName(path);
        svg.setSize(bounds.size().toSize());
        svg.setViewBox(bounds);
        QPainter painter(&svg);
        paint(painter);
        return true;
    }

    constexpr qreal scale = 2;   // sharp on high-DPI screens and in documents
    QImage image((bounds.size() * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    painter.scale(scale, scale);
    painter.translate(-bounds.topLeft());
    paint(painter);
    painter.end();
    if (!image.save(path)) {
        *error = QStringLiteral("Could not write %1").arg(path);
        return false;
    }
    return true;
}

} // namespace CanvasPainter
