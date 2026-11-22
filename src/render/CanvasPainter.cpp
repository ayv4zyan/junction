#include "CanvasPainter.h"
#include "DesignValues.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QSvgGenerator>
#include <QTextDocument>
#include <functional>

namespace {

using namespace DesignValues;   // LabelFontSize, CardFontSize, CardPadding, LabelPadding*, *Tint
constexpr qreal ArrowLength = 12;
constexpr qreal ExportMargin = 40;        // empty space around what is exported
constexpr qreal LineWidth = 2;

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
    document.setMarkdown(markdown);
    document.setTextWidth(rect.width());
    document.setDefaultFont(font(CardFontSize));

    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, color);
    context.clip = QRectF(QPointF(0, 0), rect.size());

    painter.save();
    painter.translate(rect.topLeft());
    painter.setClipRect(context.clip);
    document.documentLayout()->draw(&painter, context);
    painter.restore();
}

// What an export shows: the nodes named in `onlyIds` (all of them when it is empty), the
// Connections between two of those, and the area around them with a margin.
struct ExportedPart
{
    QVector<Node> groups;
    QVector<Node> cards;
    QVector<std::pair<Connection, Geometry::Route>> connections;
    QRectF bounds;   // empty when there is nothing to export
};

ExportedPart exportedPart(const Document &document, const QSet<QString> &onlyIds)
{
    auto included = [&](const QString &id) { return onlyIds.isEmpty() || onlyIds.contains(id); };
    ExportedPart part;
    for (const Node &n : document.nodes()) {
        if (!included(n.id))
            continue;
        (n.kind() == NodeKind::Group ? part.groups : part.cards).append(n);
        part.bounds = part.bounds.united(n.rect);
    }
    if (part.bounds.isEmpty())
        return part;
    for (const Connection &c : document.connections()) {
        const auto route = document.route(c);
        if (route && included(c.fromNode) && included(c.toNode)) {
            part.connections.append({c, *route});
            part.bounds = part.bounds.united(route->path.boundingRect());
        }
    }
    // room around it, and above for the Group label bars
    part.bounds.adjust(-ExportMargin, -ExportMargin - GroupLabelSpace, ExportMargin, ExportMargin);
    return part;
}

bool writeSvg(const QString &path, const QRectF &bounds, const std::function<void(QPainter &)> &paint)
{
    QSvgGenerator svg;
    svg.setFileName(path);
    svg.setSize(bounds.size().toSize());
    svg.setViewBox(bounds);
    QPainter painter;
    if (!painter.begin(&svg))   // fails when the file can't be opened for writing
        return false;
    paint(painter);
    return painter.end();       // the rest of the file is written here
}

bool writePng(const QString &path, const QRectF &bounds, const std::function<void(QPainter &)> &paint)
{
    constexpr qreal scale = 2;   // sharp on high-DPI screens and in documents
    QImage image((bounds.size() * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    painter.scale(scale, scale);
    painter.translate(-bounds.topLeft());
    paint(painter);
    painter.end();
    return image.save(path);
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

std::optional<Palette> Palette::fromVariantMap(const QVariantMap &map)
{
    Palette p;
    bool complete = map.contains("radius") && map.value("presets").toList().size() == 6;
    auto read = [&](const char *key, QColor &target) {
        target = map.value(key).value<QColor>();
        complete = complete && target.isValid();
    };
    read("background", p.background);
    read("grid", p.grid);
    read("cardFill", p.cardFill);
    read("cardBorder", p.cardBorder);
    read("text", p.text);
    read("mutedText", p.mutedText);
    read("line", p.line);
    read("selection", p.selection);
    p.radius = map.value("radius").toReal();
    for (const QVariant &preset : map.value("presets").toList()) {
        p.presets.append(preset.value<QColor>());
        complete = complete && p.presets.last().isValid();
    }
    if (!complete)
        return std::nullopt;
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

QColor colorUnder(QPointF point, const QVector<Node> &nodes, const Palette &palette)
{
    QColor color = palette.background;
    for (const Node &n : nodes) {
        if (n.kind() != NodeKind::Group || !n.rect.contains(point))
            continue;
        // Each Group is painted over what is below it at GroupTint opacity.
        const QColor tint = palette.resolve(n.color, palette.cardBorder);
        color = QColor::fromRgbF(color.redF() + (tint.redF() - color.redF()) * GroupTint,
                                 color.greenF() + (tint.greenF() - color.greenF()) * GroupTint,
                                 color.blueF() + (tint.blueF() - color.blueF()) * GroupTint);
    }
    return color;
}

void drawConnection(QPainter &painter, const Geometry::Route &route, const Connection &connection,
                    const Palette &palette, bool selected, bool showLabel, const QColor &labelFill)
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
        painter.setBrush(labelFill.isValid() ? labelFill : palette.background);
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

    if (node.kind() == NodeKind::Group) {
        QColor fill = accent;
        fill.setAlphaF(GroupTint);
        painter.setBrush(fill);
        painter.drawRoundedRect(node.rect, palette.radius, palette.radius);

        // The label on its bar above the Group, as GroupItem shows it.
        const QFont labelFont = font(CardFontSize, QFont::Bold);
        const QString label = node.displayText();
        const qreal width = QFontMetricsF(labelFont).horizontalAdvance(label) + 2 * CardPadding;
        const QRectF bar(node.rect.left(), node.rect.top() - GroupLabelSpace, width, GroupLabelHeight);
        QColor barFill = accent;
        barFill.setAlphaF(GroupLabelTint);
        painter.setPen(Qt::NoPen);
        painter.setBrush(barFill);
        painter.drawRoundedRect(bar, palette.radius, palette.radius);
        painter.setFont(labelFont);
        painter.setPen(palette.text);
        painter.drawText(bar, Qt::AlignCenter, label);
        return;
    }

    painter.setBrush(palette.cardFill);
    painter.drawRoundedRect(node.rect, palette.radius, palette.radius);
    if (!node.color.isEmpty()) {   // colored cards get a light tint of their color
        QColor tint = accent;
        tint.setAlphaF(CardTint);
        painter.setBrush(tint);
        painter.drawRoundedRect(node.rect, palette.radius, palette.radius);
    }

    const QRectF content = node.rect.adjusted(CardPadding, CardPadding, -CardPadding, -CardPadding);
    if (node.kind() == NodeKind::Text) {
        drawMarkdown(painter, node.displayText(), content, palette.text);
    } else {
        painter.setPen(palette.mutedText);
        painter.setFont(font(CardFontSize));
        painter.drawText(content, Qt::AlignCenter | Qt::TextWordWrap, node.displayText());
    }
}

bool exportImage(const Document &document, const QSet<QString> &onlyIds, const QString &path,
                  const Palette &palette, QString *error)
{
    const ExportedPart part = exportedPart(document, onlyIds);
    if (part.bounds.isEmpty()) {
        *error = QCoreApplication::translate("CanvasPainter", "Nothing to export.");
        return false;
    }
    const auto paint = [&](QPainter &painter) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(part.bounds, palette.background);
        for (const Node &n : part.groups)
            drawNode(painter, n, palette);
        for (const auto &[connection, route] : part.connections)
            drawConnection(painter, route, connection, palette, false, true,
                           colorUnder(route.labelPosition, part.groups, palette));
        for (const Node &n : part.cards)
            drawNode(painter, n, palette);
    };
    const bool written = path.endsWith(".svg", Qt::CaseInsensitive) ? writeSvg(path, part.bounds, paint)
                                                                    : writePng(path, part.bounds, paint);
    if (!written)
        *error = QCoreApplication::translate("CanvasPainter", "Could not write %1").arg(path);
    return written;
}

} // namespace CanvasPainter
