#pragma once

// Where a Connection runs: which sides it uses, the path through its Waypoints, hit-testing.

#include "CanvasItems.h"

#include <QPainterPath>
#include <utility>

namespace Geometry {

constexpr qreal GridSize = 20;
constexpr qreal ElbowStub = 24;   // how far an elbow path leaves a card before turning

struct Route
{
    QPainterPath path;
    QPointF start;
    QPointF end;
    QPointF startDirection;        // unit vector an arrowhead at the start points to
    QPointF endDirection;          // unit vector an arrowhead at the end points to
    QVector<QPointF> midpoints;    // one per span between start, Waypoints and end
    QPointF labelPosition;
};

QPointF sidePoint(const QRectF &rect, Side side);
QPointF sideNormal(Side side);   // unit vector pointing out of the card

// Where the line from the centre of `rect` towards `toward` crosses its border.
QPointF borderPoint(const QRectF &rect, QPointF toward);

// The sides a Connection really uses: its Pinned sides, or the facing ones for Floating Ends.
std::pair<Side, Side> resolveSides(const QRectF &from, const QRectF &to, const Connection &c);

Route route(const QRectF &from, const QRectF &to, const Connection &c);

bool hits(const Route &route, QPointF point, qreal tolerance);

// The Handle of `rect` within `radius` of `point`; Floating when there is none.
Side handleAt(const QRectF &rect, QPointF point, qreal radius);

// Where a floating toolbar of `size` goes for a selection (both in view coordinates): centred
// above it, `gap` away, kept inside `view`.
QPointF floatingPosition(const QRectF &selection, const QSizeF &size, const QRectF &view, qreal gap);

qreal snap(qreal value);
QPointF snap(QPointF point);

} // namespace Geometry
