#pragma once

// QPainter drawing of a Canvas. The live view uses it for the grid and Connections;
// export uses it for everything.

#include "Document.h"
#include "Geometry.h"

#include <QColor>
#include <QVariantMap>
#include <optional>

class QPainter;

// Colors and the corner radius come only from the design tokens (src/design: Colors.forPainter)
// as a QVariantMap, so the painted layers and exports match the rest of the UI. There are no
// defaults here on purpose: a second copy of the colors would drift from the tokens.
struct Palette
{
	QColor background;
	QColor grid;
	QColor cardFill;
	QColor cardBorder;
	QColor text;
	QColor mutedText;
	QColor line;
	QColor selection;
	QVector<QColor> presets;   // JSON Canvas colors "1".."6"
	qreal radius = 0;          // corners of Cards, Groups and Connection labels

	QColor resolve(const QString &color, const QColor &fallback) const;
	// Nothing when a color is missing or invalid (tst_design_values checks Colors.forPainter).
	static std::optional<Palette> fromVariantMap(const QVariantMap &map);
};

namespace CanvasPainter {

// `view` maps world coordinates to the screen; the grid is drawn in screen space.
void drawGrid(QPainter &painter, const QTransform &view, const QSizeF &screenSize, const Palette &palette);

// The Canvas's color at `point` as drawn: the background, tinted by each of `nodes`' Groups that
// the point lies in. A Connection's label box is filled with it, so the label sits on the same
// color as its surroundings (inside a Group too).
QColor colorUnder(QPointF point, const QVector<Node> &nodes, const Palette &palette);

// These draw in world coordinates (set the painter's transform first).
// `showLabel` is false while the label is being edited: the editor takes its place.
// `labelFill` is the label box's color (see colorUnder); the background when not given.
void drawConnection(QPainter &painter, const Geometry::Route &route, const Connection &connection,
					const Palette &palette, bool selected, bool showLabel = true,
					const QColor &labelFill = QColor());
void drawNode(QPainter &painter, const Node &node, const Palette &palette);

// The box a Connection's label is drawn in, centred on the route's label position (world
// coordinates). Empty when there is no label.
QRectF labelRect(const Geometry::Route &route, const QString &label);

// Writes PNG or SVG depending on the file extension. Empty `onlyIds` means the whole Canvas.
bool exportImage(const Document &document, const QSet<QString> &onlyIds, const QString &path,
				  const Palette &palette, QString *error);

} // namespace CanvasPainter
